#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <vector>

#include "deb.h"
#include "util.h"
#include "zdebootstrap.h"

const char *target;
int orig_wd;

static void* unpack_thread(void* arg)
{
    deb pkg((const char*)arg);
    pkg.unpack();
    return 0;
}

int main(int argc, char **argv)
{
    if ((orig_wd = open(".", O_RDONLY|O_DIRECTORY|O_PATH|O_CLOEXEC)) == -1)
        ERR("can't open current working directory\n");

    if (argc < 2)
        ERR("Usage: zunpack a.deb b.deb...\n");

    target = "target";
    if (mkdir_p(target))
        ERR("can't mkdir -p '%s': %m\n", target);
    if (chdir(target))
        ERR("can't chdir to '%s': %m\n", target);

    std::vector<pthread_t> th(argc-1);
    for (int i=1; i<argc; i++)
        if (pthread_create(&th[i], 0, unpack_thread, argv[i]))
            ERR("pthread_create: %m\n");

    for (int i=1; i<argc; i++)
        if (pthread_join(th[i], 0))
            ERR("pthread_join: %m\n");

    return 0;
}
