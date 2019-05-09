#include <archive.h>
#include <archive_entry.h>
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>

#include "zdebootstrap.h"
#include "deb.h"
#include "util.h"

deb::deb(const char *fn) : filename(fn), ar(0), ar_mem(0)
{
}

void deb::open_file()
{
    int fd;

    if ((fd = openat(orig_wd, filename, O_RDONLY|O_CLOEXEC|O_NOCTTY)) == -1)
        ERR("can't open '%s': %m\n", filename);

    struct stat st;
    if (fstat(fd, &st))
        ERR("can't stat '%s': %m\n", filename);
    if (!S_ISREG(st.st_mode))
        ERR("not a regular file: '%s'\n", filename);
    if (!(len = st.st_size))
        ERR("empty deb file: '%s'\n", filename);
    if (len < 256)
        ERR("implausible small deb file: '%s'\n", filename);
    printf("opening %s (size %zu)\n", filename, len);

    ar_mem = (char*)mmap(0, len, PROT_READ, MAP_SHARED, fd, 0);
    if (ar_mem == MAP_FAILED)
        ERR("can't mmap '%s': %m\n", filename);

    close(fd);
    madvise(ar_mem, len, MADV_SEQUENTIAL);

    if (memcmp(ar_mem, "0.939000\n", 9))
    {
        if (!(ar = archive_read_new()))
            ERR("can't create a new libarchive object: %s\n", archive_error_string(ar));

        archive_read_support_filter_none(ar);
        archive_read_support_format_ar(ar);

        if (archive_read_open_memory(ar, ar_mem, len))
            ERR("can't open deb ar: %s\n", archive_error_string(ar));
    }
    else
    {
        char *endptr;
        size_t clen = strtoul(ar_mem+9, &endptr, 10);
        if (clen<64 || clen==ULONG_MAX || *endptr!='\n')
            ERR("deb has invalid clen: %s\n", filename);
        c_mem = endptr+1;
        if (c_mem-ar_mem+clen > len)
            ERR("deb has truncated control file: %s\n", filename);
        d_mem = c_mem+clen;
    }
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

static bool is_valid_name(const char *s)
{
    if (*s<'0' || *s>'9' && *s<'a' || *s>'z')
        return false;

    for (s++; *s; s++)
        if (*s!='+' && *s!='-' && *s!='.' && *s<'0' || *s>'9' && *s<'a' || *s>'z')
            return false;

    return true;
}

const std::string& deb::field(const std::string& name)
{
    // Already checked there's exactly one paragraph.
    const auto f = control.contents[0].find(name);
    if (f == control.contents[0].cend())
        ERR("deb '%s': field '%s' missing\n", filename, name.c_str());
    return f->second;
}

const std::string& deb::field(const std::string& name, const std::string& none)
{
    // Already checked there's exactly one paragraph.
    const auto f = control.contents[0].find(name);
    if (f == control.contents[0].cend())
        return none;
    return f->second;
}

void deb::read_control()
{
    if (ar)
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
    }

    read_control_inner();

    if (control.contents.size() != 1)
    {
        ERR("deb '%s': control has %zu paragraphs instead of one\n",
            filename, control.contents.size());
    }

    const std::string& package = field("Package");
    const std::string& ma = field("Multi-Arch", "");
    const std::string& arch = field("Architecture");
    if (!is_valid_name(package.c_str()))
        ERR("deb '%s': invalid control/Package: '%s'\n", filename, package.c_str());
    if (!is_valid_name(arch.c_str()))
        ERR("deb '%s': invalid control/Architecture: '%s'\n", filename, arch.c_str());

    if (ma == "same")
        basename = package + ":" + arch;
    else
        basename = package;

    printf("%s\n", basename.c_str());
}

void deb::read_data()
{
    if (ar)
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
    }

    read_data_inner();
    write_list();
    write_info();
}

void deb::write_list()
{
    mkdir_p("var/lib/dpkg/info");

    FILE *f = fopen(("var/lib/dpkg/info/" + basename + ".list").c_str(), "we");
    if (!f)
        ERR("can't write to 'var/lib/dpkg/info/%s.list\n", basename.c_str());
    for (auto ci = contents.cbegin(); ci != contents.cend(); ++ci)
        fprintf(f, "%s\n", ci->c_str());
    fclose(f);
}

void deb::write_info()
{
    for (auto fi = info.cbegin(); fi != info.cend(); ++fi)
    {
        const control_info& i(*fi);
        struct timespec t[2]={{0,UTIME_OMIT}, i.mtime};
        int f = open(("var/lib/dpkg/info/" + basename + "." + i.filename).c_str(),
            O_CREAT|O_TRUNC|O_WRONLY|O_CLOEXEC|O_NOFOLLOW, i.x? 0777 : 0666);
        if (f==-1
            || write(f, &i.contents[0], i.contents.size()) != (ssize_t)i.contents.size()
            || futimens(f, t)
            || close(f))
        {
            ERR("can't write to 'var/lib/dpkg/info/%s.%s: %m\n",
                basename.c_str(), i.filename.c_str());
        }
    }
}

deb::~deb()
{
    if (ar)
        archive_read_free(ar);
    if (ar_mem)
        munmap(ar_mem, len);
}

void deb::slurp_control_file()
{
    char buf[4096];
    la_ssize_t len;
    std::string txt;

    while ((len = archive_read_data(ac, buf, sizeof(buf))) > 0)
        txt.append(buf, len);
    if (len < 0)
        ERR("%s\n", archive_error_string(ac));

    control.parse(txt.c_str());
}

void deb::slurp_control_info(const char *name, bool x, time_t sec, long nsec)
{
    char buf[4096];
    la_ssize_t len;
    std::string txt;

    while ((len = archive_read_data(ac, buf, sizeof(buf))) > 0)
        txt.append(buf, len);
    if (len < 0)
        ERR("%s\n", archive_error_string(ac));

    info.emplace(name, x, sec, nsec, txt);
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
    archive_read_support_filter_zstd(ac);
    archive_read_support_format_tar(ac);

    if (ar? archive_read_open(ac, this, 0, deb_ar_comp_read, 0)
          : archive_read_open_memory(ac, c_mem, d_mem-c_mem))
    {
        ERR("can't open deb control: '%s': %s\n", filename,
            archive_error_string(ac));
    }

    struct archive_entry *ent;
    int err;
    while (!(err = archive_read_next_header(ac, &ent)))
    {
        const char *cf = archive_entry_pathname(ent);
        if (!strcmp(cf, "./control"))
            slurp_control_file();
        else if (strncmp(cf, "./", 2))
            ERR("deb control filename in '%s' doesn't start with './': '%s'\n", filename, cf);
        else if (!cf[2])
            /* top-level dir */;
        else if (!is_valid_name(cf+2))
            ERR("bad deb control filename in '%s': '%s'\n", filename, cf+2);
        else
        {
            slurp_control_info(cf+2, archive_entry_perm(ent)&0111,
                archive_entry_mtime(ent), archive_entry_mtime_nsec(ent));
        }
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
    archive_read_support_filter_zstd(ac);
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

    if (ar? archive_read_open(ac, this, 0, deb_ar_comp_read, 0)
          : archive_read_open_memory(ac, d_mem, len-(d_mem-ar_mem)))
        ERR("can't open deb data: '%s': %s\n", filename, archive_error_string(ac));

    struct archive_entry *ent;
    int err;
    while (!(err = archive_read_next_header(ac, &ent)))
    {
        const char *fn = archive_entry_pathname(ent);
        if (*fn == '.')
            fn++;
        if (*fn != '/')
            ERR("filename inside '%s' not absolute: '%s'\n", filename, fn);
        std::string fns = fn;
        if (fns.back() == '/')
        {
            fns.pop_back();
            if (fns.empty())
                fns = "/."; // dpkg special-cases /
        }

        contents.emplace(fns);

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

void deb::unpack()
{
    open_file();
    if (ar)
        check_deb_binary();
    read_control();
    read_data();
}
