#include <archive.h>
#include <archive_entry.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>

#include "zdebootstrap.h"
#include "deb.h"

#include <stdio.h>
#define ERR(...) do {fprintf(stderr, __VA_ARGS__); exit(1);} while (0)

deb::deb(const char *fn)
{
    filename = fn;

    if ((fd = openat(orig_wd, filename, O_RDONLY)) == -1)
        ERR("can't open '%s': %m\n", filename);

    struct stat st;
    if (fstat(fd, &st))
        ERR("can't stat '%s': %m\n", filename);
    if (!S_ISREG(st.st_mode))
        ERR("not a regular file: '%s'\n", filename);
    if (!(len = st.st_size))
        ERR("empty deb file: '%s'\n", filename);
    printf("opening %s (size %zu)\n", filename, len);

    void *ar_mem = mmap(0, len, PROT_READ, MAP_SHARED, fd, 0);
    if (ar_mem == MAP_FAILED)
        ERR("can't mmap '%s': %m\n", filename);

    madvise(ar_mem, len, MADV_SEQUENTIAL);

    if (!(ar = archive_read_new()))
        ERR("can't create a new libarchive object: %s\n", archive_error_string(ar));

    archive_read_support_filter_none(ar);
    archive_read_support_format_ar(ar);

    if (archive_read_open_memory(ar, ar_mem, len))
        ERR("can't open deb ar: %s\n", archive_error_string(ar));
}

void deb::check_deb_binary()
{
    struct archive_entry *ent;
    if (archive_read_next_header(ar, &ent))
    {
        ERR("bad deb file '%s': no more ar entries, wanted debian-binary\n",
            filename);
    }

    const char *ent_name = archive_entry_pathname(ent);
    if  (!ent_name || strcmp(ent_name, "debian-binary"))
    {
        ERR("bad deb file '%s': wanted debian-binary, got '%s'\n",
            filename, ent_name);
    }

    char buf[5];
    if (archive_read_data(ar, buf, 5) != 4)
        ERR("bad deb file '%s': bad debian-binary\n", filename);

    if (memcmp(buf, "2.0\n", 4))
        ERR("bad deb file '%s': version not 2.0\n", filename);
}

void deb::read_control()
{
    struct archive_entry *ent;
    if (archive_read_next_header(ar, &ent))
    {
        ERR("bad deb file '%s': no more ar entries, wanted control.tar*\n",
            filename);
    }

    const char *ent_name = archive_entry_pathname(ent);
    if  (!ent_name || strncmp(ent_name, "control.tar", 11))
    {
        ERR("bad deb file '%s': wanted control.tar*, got '%s'\n",
            filename, ent_name);
    }

    read_control_inner();
}

void deb::read_data()
{
    struct archive_entry *ent;
    if (archive_read_next_header(ar, &ent))
    {
        ERR("bad deb file '%s': no more ar entries, wanted data.tar*\n",
            filename);
    }

    const char *ent_name = archive_entry_pathname(ent);
    if  (!ent_name || strncmp(ent_name, "data.tar", 8))
    {
        ERR("bad deb file '%s': wanted data.tar*, got '%s'\n",
            filename, ent_name);
    }

    read_data_inner();
}

deb::~deb()
{
    archive_read_free(ar);
    munmap(ar_mem, len);
    close(fd);
}

la_ssize_t deb_ar_comp_read(struct archive *arc, void *c_data, const void **buf)
{
    deb *pkg = static_cast<deb*>(c_data);

    const void *ibuf;
    size_t len;
    off_t offset;
    int err = archive_read_data_block(pkg->ar, &ibuf, &len, &offset);
    if (err > ARCHIVE_EOF)
    {
        ERR("can't read deb contents from '%s': %s\n", pkg->filename,
            archive_error_string(arc));
    }

    *buf = ibuf;
    return len;
}

void deb::read_control_inner()
{
    if (!(ac = archive_read_new()))
        ERR("can't create a new libarchive object: %s\n", archive_error_string(ac));

    archive_read_support_filter_gzip(ac);
    archive_read_support_filter_xz(ac);
    archive_read_support_format_tar(ac);

    if (archive_read_open(ac, this, 0, deb_ar_comp_read, 0))
    {
        ERR("can't open deb control: '%s': %s\n", filename,
            archive_error_string(ac));
    }

    struct archive_entry *ent;
    int err;
    while (!(err = archive_read_next_header(ac, &ent)))
    {
        printf("%s\n", archive_entry_pathname(ent));
        archive_read_data_skip(ac);
    }

    if (err != ARCHIVE_EOF)
    {
        ERR("can't list deb control: '%s': %s\n", filename,
            archive_error_string(ac));
    }

    archive_read_free(ac);
}


void deb::read_data_inner()
{
    if (!(ac = archive_read_new()))
        ERR("can't create a new libarchive object: %s\n", archive_error_string(ac));

    archive_read_support_filter_gzip(ac);
    archive_read_support_filter_bzip2(ac);
    archive_read_support_filter_lzma(ac);
    archive_read_support_filter_xz(ac);
    archive_read_support_format_tar(ac);

    // TODO: free earlier allocs on error
    if (!(aw = archive_write_disk_new()))
        ERR("can't create a new libarchive write object\n");

    archive_write_disk_set_options(aw, (geteuid()? 0 : ARCHIVE_EXTRACT_OWNER)
        |ARCHIVE_EXTRACT_PERM
        |ARCHIVE_EXTRACT_TIME
        |ARCHIVE_EXTRACT_UNLINK
        |ARCHIVE_EXTRACT_SECURE_NODOTDOT
        |ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS
        |ARCHIVE_EXTRACT_CLEAR_NOCHANGE_FFLAGS
        |ARCHIVE_EXTRACT_SECURE_SYMLINKS);

    if (archive_read_open(ac, this, 0, deb_ar_comp_read, 0))
        ERR("can't open deb data: '%s': %s\n", filename, archive_error_string(ac));

    struct archive_entry *ent;
    int err;
    while (!(err = archive_read_next_header(ac, &ent)))
    {
        printf("%s\n", archive_entry_pathname(ent));
        if ((err = archive_read_extract2(ac, ent, aw)))
        {
            ERR("error extracting deb entry '%s' from '%s': %s\n",
                archive_entry_pathname(ent), filename, archive_error_string(ac));
        }
    }

    if (err != ARCHIVE_EOF)
        ERR("can't list deb data: '%s': %s\n", filename, archive_error_string(ac));

    archive_write_free(aw);
    archive_read_free(ac);
}
