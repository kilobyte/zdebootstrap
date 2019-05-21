#include "zdebootstrap.h"
#include "apt.h"
#include <unistd.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <wait.h>

extern char **environ;

static int devnull;

static int spawn(int *outfd, char *const *argv)
{
    int pid;
    int p[2];
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

    if (posix_spawn_file_actions_init(&fa))
        ERR("posix_spawn_file_actions_init failed: %m\n");
    posix_spawn_file_actions_adddup2(&fa, devnull, 0);
    posix_spawn_file_actions_adddup2(&fa, outfd? p[1] : devnull, 1);

    if (posix_spawn(&pid, argv[0], &fa, nullptr, argv, environ))
    {
        int e=errno;
        posix_spawn_file_actions_destroy(&fa);
        if (outfd)
            close(p[0]), close(p[1]);
        errno=e;
        ERR("posix_spawn failed: %m\n");
    }

    posix_spawn_file_actions_destroy(&fa);
    if (outfd)
    {
        close(p[1]);
        *outfd=p[0];
    }
    return pid;
}

void apt_sim(const plf::colony<const char*> &goals)
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
    int pid=spawn(&aptfd, (char*const*)args);

    FILE *f=fdopen(aptfd, "r");
    char line[256];

    while (fgets(line, sizeof(line), f))
    {
        char pkg[256], *p=pkg;
        char ver[256], *v=ver;
        char arch[256], *a=arch;

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
        printf("[%s] [%s] [%s]\n", pkg, ver, arch);
    }

    fclose(f);
    int status;
    if (waitpid(pid, &status, 0)!=pid)
        ERR("wait failed: %m\n");
    if (status)
        ERR("apt failed\n");
}
