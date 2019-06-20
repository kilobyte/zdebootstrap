#include "zdebootstrap.h"
#include "apt.h"
#include "822.h"
#include <unistd.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <wait.h>
#include <unordered_map>
#include <map>

extern char **environ;

static int devnull;

static std::unordered_map<std::string, par822> avail;

static int spawn(char *const *argv, int *outfd, int *errfd=nullptr)
{
    int pid;
    int p[2], r[2];
    posix_spawn_file_actions_t fa;

    if (!devnull)
    {
        int dev=open("/dev/null", O_RDWR|O_CLOEXEC);
        if (dev==-1)
            ERR("open(“/dev/null”) failed: %m\n");
        if (!__sync_bool_compare_and_swap(&devnull, 0, dev))
            close(dev);
    }

    if (outfd)
        if (pipe2(p, O_CLOEXEC))
            ERR("pipe2 failed: %m\n");
    if (errfd)
        if (pipe2(r, O_CLOEXEC))
            ERR("pipe2 failed: %m\n");

    if (posix_spawn_file_actions_init(&fa))
        ERR("posix_spawn_file_actions_init failed: %m\n");
    posix_spawn_file_actions_adddup2(&fa, devnull, 0);
    posix_spawn_file_actions_adddup2(&fa, outfd? p[1] : devnull, 1);
    if (errfd)
        posix_spawn_file_actions_adddup2(&fa, r[1], 2);

    if (posix_spawn(&pid, argv[0], &fa, nullptr, argv, environ))
    {
        int e=errno;
        posix_spawn_file_actions_destroy(&fa);
        if (outfd)
            close(p[0]), close(p[1]);
        if (errfd)
            close(r[0]), close(r[1]);
        errno=e;
        ERR("posix_spawn failed: %m\n");
    }

    posix_spawn_file_actions_destroy(&fa);
    if (outfd)
    {
        close(p[1]);
        *outfd=p[0];
    }
    if (errfd)
    {
        close(r[1]);
        *errfd=r[0];
    }
    return pid;
}

plf::colony<std::string> apt_sim(const plf::colony<const char*> &goals)
{
    const char *args[goals.size()+4];
    int i=0;
    args[i++] = "/usr/bin/apt-get";
    args[i++] = "-s";
    args[i++] = "install";
    for (auto gi=goals.cbegin(); gi!=goals.cend(); ++gi)
        args[i++] = *gi;
    args[i] = 0;

    int aptfd;
    int pid=spawn((char*const*)args, &aptfd);

    plf::colony<std::string> pav;

    FILE *f=fdopen(aptfd, "r");
    char line[256];

    while (fgets(line, sizeof(line), f))
    {
        char pkg[sizeof(line)], *p=pkg;
        char ver[sizeof(line)], *v=ver;
        char arch[sizeof(line)], *a=arch;

        // We want: Inst strace (4.15-2 Debian:9.9/stable [arm64])

        const char *c=line;
        #define GET(x) if (*c++!=x) continue
        GET('I'); GET('n'); GET('s'); GET('t'); GET(' ');
        while (*c && *c!=' ')
            *p++=*c++;	// package name
        GET(' '); GET('(');
        while (*c && *c!=' ')
            *v++=*c++;	// version
        GET(' ');
        while (*c && *c!=' ')
            c++;	// dist -- ignore
        GET(' '); GET('[');
        while (*c && *c!=']')
            *a++=*c++;	// arch
        GET(']'); GET(')'); GET('\n'); GET(0);

        *p=*v=*a=0;
        pav.emplace(line, snprintf(line, sizeof(line), "%s:%s=%s", pkg, arch, ver));
    }

    fclose(f);
    int status;
    if (waitpid(pid, &status, 0)!=pid)
        ERR("wait failed: %m\n");
    if (status)
        ERR("apt failed\n");

    return pav;
}

static bool opstat(int dir, size_t len, const char *name, int *fd)
{
    struct stat st;
    int f;

    if (!fd)
    {
        if (fstatat(dir, name, &st, 0))
            return false;
    }
    else
    {
        f=openat(dir, name, O_RDONLY|O_CLOEXEC);
        if (f==-1)
            return false;
        if (fstat(f, &st))
            return close(f), false;
    }

    if (!S_ISREG(st.st_mode) || len && (size_t)st.st_size!=len)
        return (fd?close(f):0), false;
    if (fd)
        *fd=f;
    return true;
}

bool find_deb(int dir, size_t len, const char *pav, int *fd)
{
    const char *colon = strchrnul(pav, ':');
    const char *equal = strchrnul(colon, '=');
    int plen = colon-pav;
    int alen = equal-colon-1;
    if (alen<=0 || !*equal || !*++equal)
        return false;
    colon++;

    char name[256];
    #define TRY if (opstat(dir, len, name, fd)) return true
    // First, try unmangled name.
    sprintf(name, "%.*s_%s_%.*s.deb", plen, pav, equal, alen, colon);
    TRY;
    const char *epoch = strchr(equal, ':');
    if (!epoch || epoch==equal)
        return false;
    // Then, epoch with : mangled to %3a.
    sprintf(name, "%.*s_%.*s%%3a%s_%.*s.deb", plen, pav, (int)(epoch-equal), equal,
        epoch+1, alen, colon);
    TRY;
    // Then, with epoch elided.
    sprintf(name, "%.*s_%s_%.*s.deb", plen, pav, epoch+1, alen, colon);
    TRY;
    #undef TRY
    return false;
}

static std::string pav822(par822 &p)
{
    const std::string& pkg(field822(p, "Package", ""));
    if (pkg.empty())
        ERR("paragraph with no Package in apt-avail\n");
    const std::string& ver(field822(p, "Version", ""));
    if (ver.empty())
        ERR("paragraph with no Version in apt-avail\n");
    const std::string& arch(field822(p, "Architecture", ""));
    if (arch.empty())
        ERR("paragraph with no Version in apt-avail\n");

    char buf[256];
    return std::string(buf, snprintf(buf, 256, "%s:%s=%s", pkg.c_str(),
        arch.c_str(), ver.c_str()));
}

void apt_avail(void)
{
    const char *args[]=
    {
        "/usr/bin/apt-cache",
        "dumpavail",
        0
    };

    int aptfd;
    int pid=spawn((char*const*)args, &aptfd);
    deb822 av;
    av.parse_file(aptfd);

    int status;
    if (waitpid(pid, &status, 0)!=pid)
        ERR("wait failed: %m\n");
    if (status)
        ERR("apt failed\n");

    for (auto c=av.contents.begin(); c!=av.contents.end(); ++c)
        avail[pav822(*c)] = std::move(*c);
}

size_t deb_avail_size(const char *pav)
{
    auto c=avail.find(pav);
    if (c==avail.cend())
        return 0;

    const char *size = field822(c->second, "Size", "0").c_str();
    // missing size is ok, invalid size is fatal

    char *end;
    size_t val = strtoul(size, &end, 10);
    if (*end)
        ERR("invalid Size in apt-avail for %s: '%s'\n", pav, size);
    return val;
}

void apt_download(plf::colony<std::string> &plan, void (got_package)(const char*))
{
    #define ARGN 3
    const char *args[plan.size()+ARGN+1];
    int i=0;
    args[i++] = "/usr/bin/apt-get";
    //args[i++] = "var/cache/apt";
    args[i++] = "-oDebug::pkgAcquire=1";
    args[i++] = "download";
    for (auto gi=plan.cbegin(); gi!=plan.cend(); ++gi)
        args[i++] = gi->c_str();
    args[i] = 0;

    int aptfd;
    int pid=spawn((char*const*)args, nullptr, &aptfd);

    FILE *f=fdopen(aptfd, "r");
    if (!f)
        ERR("fdopen failed: %m\n");

    std::unordered_map<std::string, const char*> versions;
    for (int j=ARGN; j<i; j++)
    {
        const char *pav = args[j];
        const char *arch = strchr(pav, ':');
        assert(arch);
        const char *ver = strrchr(pav, '=');
        assert(ver);
        versions[std::string(pav, arch-pav)] = ver+1;
    }

    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        const char *pref = "Dequeuing ";
        const size_t preflen = strlen(pref);
        if (strncmp(pref, line, preflen))
            continue;
        char *pkg = strrchr(line+preflen, '/');
        pkg = pkg? pkg+1 : line+preflen;
        char *ext = strstr(pkg, ".deb\n");
        if (!ext || ext[5])
            continue;
        *ext=0;
        if (verbose >= 2)
            printf("apt got “%s”\n", pkg);
        char *ver = strchr(pkg, '_');
        if (!ver)
            continue;
        *ver++=0;
        char *arch = strchr(ver, '_');
        if (!arch)
            continue;
        arch++;
        const auto vi = versions.find(pkg);
        if (vi == versions.cend())
            continue;
        char buf[256];
        snprintf(buf, sizeof(buf), "%s:%s=%s", pkg, arch, vi->second);
        got_package(buf);
        versions.erase(vi);
    }

    fclose(f);
    int status;
    if (waitpid(pid, &status, 0)!=pid)
        ERR("wait failed: %m\n");
    if (status)
        ERR("apt download failed\n");
}
