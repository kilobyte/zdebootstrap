#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <fcntl.h>

#include "paths.h"
#include "zdebootstrap.h"

const char *target;
int orig_wd, target_wd;
char *abs_target;

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
    if ((target_wd=open(".", O_DIRECTORY|O_PATH|O_CLOEXEC))==-1)
        ERR("can't open target dir '%s': %m\n", target);
    if (!(abs_target=getcwd(0,0)))
        ERR("getcwd failed: %m\n");
    if (mkdir_p("var/lib/dpkg/info"))
        ERR("can't mkdir -p 'var/lib/dpkg/info': %m\n");
}
