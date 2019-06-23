#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>

#include "deb.h"
#include "paths.h"
#include "tqueue.h"
#include "status.h"
#include "apt.h"
#include "zdebootstrap.h"

static void unpack_thread(const char* arg)
{
    deb pkg(arg);
    pkg.unpack();
}

static unsigned parse_u(const char *arg, const char *errmsg)
{
    char *end;
    unsigned val = strtoul(arg, &end, 10);
    if (*end)
        ERR("%s: '%s'\n", errmsg, arg);
    return val;
}

int main(int argc, char **argv)
{
    if ((orig_wd = open(".", O_DIRECTORY|O_PATH_RD|O_CLOEXEC)) == -1)
        ERR("can't open current working directory\n");

    target = "target";
    verbose = 0;

    static struct option options[] =
    {
        { "target",	1, 0, 't' },
        { "verbose",	0, 0, 'v' },
        { "quiet",	0, 0, 'q' },
        { "path-exclude", 1, 0, 1001 },
        {0}
    };

    unsigned nthreads=0;
    int opt;
    while ((opt = getopt_long(argc, argv, "t:j:vq", options, 0)) != -1)
    {
        switch (opt)
        {
        case 'j':
            if (*optarg)
                nthreads = parse_u(optarg, "not a number for -j");
            if (nthreads > 4096)
                ERR("insane number of threads requested\n");
            break;
        case 't':
            target = optarg;
            break;
        case 'v':
            verbose++;
            break;
        case 'q':
            verbose--;
            break;
        case 1001: // --path-exclude
            path_excludes.insert(optarg);
            break;
        default:
            exit(1);
        }
    }

    if (!argv[optind])
        ERR("Usage: zunpack a.deb b.deb...\n");

    mk_target();

    tqueue slaves(unpack_thread, nthreads);
    for (int i=optind; i<argc; i++)
        slaves.put(argv[i]);
    slaves.finish();
    status_write();

    return 0;
}
