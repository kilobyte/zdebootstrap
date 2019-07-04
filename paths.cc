#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <fcntl.h>

#include "paths.h"
#include "zdebootstrap.h"

int verbose;
const char *target = "target";
int orig_wd, target_wd;
static char *abs_target;
static size_t abs_target_len;
static int proc_self;
plf::colony<const char*> path_excludes;

static int mkdir_p(const char *path)
{
    std::string p = path;

    for (size_t i=1; i < p.size(); i++)
        if (p[i] == '/')
            if (mkdir(p.substr(0,i).c_str(), 0755) && errno != EEXIST)
                return -1;

    if (mkdir(path, 0755) && errno != EEXIST)
        return -1;

    return 0;
}

void mk_target(void)
{
    if (mkdir_p(target))
        ERR("can't mkdir -p '%s': %m\n", target);
    if (chdir(target))
        ERR("can't chdir to '%s': %m\n", target);
    if ((target_wd=open(".", O_DIRECTORY|O_PATH_RD|O_CLOEXEC))==-1)
        ERR("can't open target dir '%s': %m\n", target);
    if (!(abs_target=getcwd(0,0)))
        ERR("getcwd failed: %m\n");
    abs_target_len=strlen(abs_target);
    if (mkdir_p("var/lib/dpkg/info"))
        ERR("can't mkdir -p 'var/lib/dpkg/info': %m\n");
}

// Open a file/dir securely, beating "..", symlink+race attacks, etc.
// The target may be a symlink, dirs inside at most mount points.
int open_in_target(const char *path, int flags)
{
    if (path[0]=='.' && path[1]=='/')
        path+=2;

    if (!strcmp(path, ".")) // would be elided later, simplify logic
        return openat(target_wd, ".", flags|O_CLOEXEC|O_NOFOLLOW);

    int fd = openat(target_wd, path, flags|O_CLOEXEC|O_NOFOLLOW);
    if (fd==-1)
        return -1;

    if (!proc_self)
    {
        int ps=open("/proc/self", O_DIRECTORY|O_CLOEXEC);
        if (ps!=-1)
            if (!__sync_bool_compare_and_swap(&proc_self, 0, ps))
                close(ps);
    }

    if (proc_self == -1)
        return fd; // until resolveat() is merged, can't do more

    char fds[15];
    sprintf(fds, "fd/%d", fd);
    char realpath[4096]; // limited buffer is ok
    ssize_t ll = readlinkat(proc_self, fds, realpath, sizeof(realpath));
    if (ll == -1)
        ERR("can't read proc link for “%s”: %m\n", path);

    size_t plen = strlen(path);
    if ((size_t)ll != abs_target_len+1+plen
        || memcmp(realpath, abs_target, abs_target_len)
        || realpath[abs_target_len]!='/' // cwd was sanitized
        || memcmp(realpath+abs_target_len+1, path, plen))
    {
        ERR("Treyf path: “%s” in “%s” resolved to “%.*s”\n", path, abs_target,
            (int)ll, realpath);
    }

    return fd;
}
