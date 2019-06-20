#include <archive.h>
#include <archive_entry.h>
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <fnmatch.h>
#include <libgen.h>

#include "zdebootstrap.h"
#include "deb.h"
#include "util.h"
#include "status.h"

deb::deb(const char *fn) : filename(fn), ar(0), ar_mem(0), pdir(-1)
{
}

void deb::open_file(int fd)
{
    if (fd == -1)
        fd = openat(orig_wd, filename, O_RDONLY|O_CLOEXEC|O_NOCTTY);
    if (fd == -1)
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
    if (verbose >= 1)
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
    const auto f = control.contents.cbegin()->find(name);
    if (f == control.contents.cbegin()->cend())
        ERR("deb '%s': field '%s' missing\n", filename, name.c_str());
    return f->second;
}

const std::string& deb::field(const std::string& name, const std::string& none)
{
    // Already checked there's exactly one paragraph.
    return field822(*control.contents.cbegin(), name, none);
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

    if (verbose >= 0)
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
            O_CREAT|O_TRUNC|O_WRONLY|O_CLOEXEC|O_NOFOLLOW, i.x? 0755 : 0644);
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
    if (pdir!=-1)
        close(pdir);
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

static bool is_bad_filename(const char *fn)
{
    for (const char *c=fn; *c; ++c)
    {
        // control characters
        if (*c<' ' || *c==0x7f)
            return true;

        // path components consisting of only dots.  Some filesystems don't
        // like ... or ....
        if (*c=='/' && c[1]=='.')
            for (const char *d=c+2; ; ++d)
            {
                if (!*d || *d=='/')
                    return true;
                else if (*d!='.')
                    break;
            }

        // TODO: Unicode
    }

    return false;
}

void deb::open_dir(const char *dir)
{
    if (pdir != -1)
    {
        if (ppath == dir)
            return;
        close(pdir);
    }

    ppath = dir;
    if (*dir=='/')
        ++dir;
    pdir=open(*dir? dir : ".", O_DIRECTORY|O_PATH|O_CLOEXEC|O_NOFOLLOW);
    if (pdir == -1)
        ERR("open(“%s”, O_PATH) failed: %m\n", dir);
    // TODO: validate
}

void deb::extract_entry(struct archive_entry *ent, const char *fn)
{
    std::string path1(fn), path2(fn);
    const char *base = ::basename((char*)path1.c_str());
    const char *dir = ::dirname((char*)path2.c_str());
    open_dir(dir);

    mode_t type = archive_entry_filetype(ent);
    if (type!=AE_IFREG && type!=AE_IFLNK && type!=AE_IFDIR && type!=0)
        ERR("invalid file type in '%s' for '%s'\n", filename, fn);

#if 0
    switch (type)
    {
        case AE_IFREG:
            printf("📄 %s\n", fn);
            break;
        case AE_IFLNK:
            printf("🔗 %s → %s\n", fn, archive_entry_symlink(ent));
            break;
        case AE_IFDIR:
            printf("📁 %s\n", fn);
            break;
        case 0:
            printf("🔖 %s → %s\n", fn, archive_entry_hardlink(ent));
            return;
    }
#endif

    struct stat st;
    int err=fstatat(pdir, base, &st, AT_SYMLINK_NOFOLLOW);
    if (err && errno!=ENOENT)
        ERR("fstatat(“%s”, “%s”) failed: %m\n", dir, base);
    if (!err)
    {
        if (type==AE_IFDIR && S_ISDIR(st.st_mode))
            return; // many packages may have the same dir

        if (unlinkat(pdir, base, 0))
            ERR("unlinkat(“%s”, “%s”) failed: %m\n", dir, base);
    }

    int fd=-1;
    switch (type)
    {
        case AE_IFREG:
            // O_EXCL is intentional: unlink above is for retrying installation;
            // a file conflict is banned.
            fd=openat(pdir, base, O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW, 0700);
            if (fd==-1)
                ERR("Can't create file (“%s”, “%s”): %m\n", dir, base);
            if (archive_read_data_into_fd(ac, fd))
            {
                ERR("error extracting file '%s' from '%s': %s\n",
                    fn, filename, archive_error_string(ac));
            }
            break;
        case AE_IFLNK:
            if (symlinkat(archive_entry_symlink(ent), pdir, base))
                ERR("symlinkat(“%s”, “%s”) failed: %m\n", dir, base);
            fd=openat(pdir, base, O_PATH|O_CLOEXEC|O_NOFOLLOW);
            if (fd==-1)
                ERR("error opening just created symlink “%s” from “%s”: %m\n", fn, filename);
            break;
        case AE_IFDIR:
            if (mkdirat(pdir, base, 0700) && errno!=EEXIST)
                ERR("mkdirat(“%s”, “%s”) failed: %m\n", dir, base);
            fd=openat(pdir, base, O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
            if (fd==-1)
                ERR("error opening just created dir “%s” from “%s”: %m\n", fn, filename);
            break;
        case 0:
            // TODO: secure open
            if (linkat(AT_FDCWD, archive_entry_hardlink(ent), pdir, base, 0))
            {
                ERR("linkat(“%s”, “%s”, “%s”) failed: %m\n",
                    archive_entry_hardlink(ent), dir, base);
            }
            // Won't update owner, perms, mtime, ...
            return;
        default:
            ERR("invalid file type in '%s' for '%s'\n", filename, fn);
    }

    struct timespec times[2];
    times[0].tv_nsec=UTIME_NOW;
    times[1].tv_sec =archive_entry_mtime(ent);
    times[1].tv_nsec=archive_entry_mtime_nsec(ent);
    futimens(fd, times);
    // TODO: dir mtimes should be set after insides are written

    if (close(fd))
        ERR("error closing file '%s' from '%s': %m\n", fn, filename);
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

#if 0
    archive_write_disk_set_options(aw, (geteuid()? 0 : ARCHIVE_EXTRACT_OWNER)
        |ARCHIVE_EXTRACT_PERM
✓       |ARCHIVE_EXTRACT_TIME
✓       |ARCHIVE_EXTRACT_UNLINK
        |ARCHIVE_EXTRACT_SECURE_NODOTDOT
        |ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS
✗       |ARCHIVE_EXTRACT_CLEAR_NOCHANGE_FFLAGS
        |ARCHIVE_EXTRACT_SECURE_SYMLINKS);
#endif

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

        int skip = !fn[1];
        for (auto c=path_excludes.cbegin(); c!=path_excludes.cend(); ++c)
            if (!fnmatch(*c, fn, FNM_PATHNAME))
            {
                archive_read_data_skip(ac);
                skip=1;
                break;
            }
        if (skip)
            continue;

        if (is_bad_filename(fn))
            ERR("bad filename inside '%s': '%s'\n", filename, fn);

        std::string fns = fn;
        if (fns.back() == '/')
        {
            fns.pop_back();
            if (fns.empty())
                fns = "/."; // dpkg special-cases /
        }

        contents.emplace(fns);

        extract_entry(ent, fn);
    }

    if (err != ARCHIVE_EOF)
        ERR("can't list deb data: '%s': %s\n", filename, archive_error_string(ac));

    archive_read_free(ac);
}

void deb::unpack()
{
    if (!ar_mem)
        open_file();
    if (ar)
        check_deb_binary();
    read_control();
    read_data();
    (*control.contents.begin())["Status"]="install ok unpacked";
    status_add(std::move(*control.contents.begin()));
}
