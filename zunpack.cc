#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <getopt.h>
#include <vector>

#include "deb.h"
#include "util.h"
#include "zdebootstrap.h"

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

    target = "target";

    static struct option options[] =
    {
        { "target",	1, 0, 't' },
        {0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "t:", options, 0)) != -1)
    {
        switch (opt)
        {
        case 't':
            target = optarg;
            break;
        default:
            exit(1);
        }
    }

    if (!argv[optind])
        ERR("Usage: zunpack a.deb b.deb...\n");

    if (mkdir_p(target))
        ERR("can't mkdir -p '%s': %m\n", target);
    if (chdir(target))
        ERR("can't chdir to '%s': %m\n", target);

    std::vector<pthread_t> th(argc-1);
    for (int i=optind; i<argc; i++)
        if (pthread_create(&th[i], 0, unpack_thread, argv[i]))
            ERR("pthread_create: %m\n");

    for (int i=optind; i<argc; i++)
        if (pthread_join(th[i], 0))
            ERR("pthread_join: %m\n");

    return 0;
}
