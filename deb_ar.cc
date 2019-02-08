#include <archive.h>
#include <archive_entry.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>

#include "deb_ar.h"
#include "deb_control.h"
#include "deb_data.h"

#include <stdio.h>
#define ERR(...) do {fprintf(stderr, __VA_ARGS__); exit(1);} while (0)

deb_ar::deb_ar(const char *fn)
{
    filename = fn;

    if ((fd = open(filename, O_RDONLY)) == -1)
        ERR("can't open '%s': %m\n", filename);

    struct stat st;
    if (fstat(fd, &st))
        ERR("can't stat '%s': %m\n", filename);
    len = st.st_size;
    printf("opening %s (size %zu)\n", filename, len);

    void *mem = mmap(0, len, PROT_READ, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED)
        ERR("can't mmap '%s': %m\n", filename);

    madvise(mem, len, MADV_SEQUENTIAL);

    if (!(arc = archive_read_new()))
        ERR("can't create a new libarchive object: %s\n", archive_error_string(arc));

    archive_read_support_filter_none(arc);
    archive_read_support_format_ar(arc);

    if (archive_read_open_memory(arc, mem, len))
        ERR("can't open deb ar: %s\n", archive_error_string(arc));
}

void deb_ar::check_deb_binary()
{
    struct archive_entry *ent;
    if (archive_read_next_header(arc, &ent))
    {
        ERR("bad deb file '%s': no more ar entries, wanted debian-binary\n",
            archive_error_string(arc));
    }

    const char *ent_name = archive_entry_pathname(ent);
    if  (!ent_name || strcmp(ent_name, "debian-binary"))
    {
        ERR("bad deb file '%s': wanted debian-binary, got '%s'\n",
            filename, ent_name);
    }

    char buf[5];
    if (archive_read_data(arc, buf, 5) != 4)
        ERR("bad deb file '%s': bad debian-binary\n", filename);

    if (memcmp(buf, "2.0\n", 4))
        ERR("bad deb file '%s': version not 2.0\n", filename);
}

void deb_ar::read_control()
{
    struct archive_entry *ent;
    if (archive_read_next_header(arc, &ent))
    {
        ERR("bad deb file '%s': no more ar entries, wanted control.tar*\n",
            archive_error_string(arc));
    }

    const char *ent_name = archive_entry_pathname(ent);
    if  (!ent_name || strncmp(ent_name, "control.tar", 11))
    {
        ERR("bad deb file '%s': wanted control.tar*, got '%s'\n",
            filename, ent_name);
    }

    deb_control dctrl(this);
}

void deb_ar::read_data()
{
    struct archive_entry *ent;
    if (archive_read_next_header(arc, &ent))
    {
        ERR("bad deb file '%s': no more ar entries, wanted data.tar*\n",
            archive_error_string(arc));
    }

    const char *ent_name = archive_entry_pathname(ent);
    if  (!ent_name || strncmp(ent_name, "data.tar", 8))
    {
        ERR("bad deb file '%s': wanted data.tar*, got '%s'\n",
            filename, ent_name);
    }

    deb_data ddata(this);
}

deb_ar::~deb_ar()
{
    archive_read_free(arc);
    munmap(mem, len);
    close(fd);
}

la_ssize_t deb_ar_comp_read(struct archive *arc, void *c_data, const void **buf)
{
    deb_ar *ar = static_cast<deb_ar*>(c_data);

    const void *ibuf;
    size_t len;
    off_t offset;
    int err = archive_read_data_block(ar->arc, &ibuf, &len, &offset);
    if (err > ARCHIVE_EOF)
    {
        ERR("can't read deb contents from '%s': %s\n", ar->filename,
            archive_error_string(arc));
    }

    *buf = ibuf;
    return len;
}
