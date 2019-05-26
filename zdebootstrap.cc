#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>

#include "deb.h"
#include "util.h"
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
    if ((orig_wd = open(".", O_RDONLY|O_DIRECTORY|O_PATH|O_CLOEXEC)) == -1)
        ERR("can't open current working directory\n");

    target = "target";
    verbose = 0;

    static struct option options[] =
    {
        { "target",	1, 0, 't' },
        { "verbose",	0, 0, 'v' },
        { "quiet",	0, 0, 'q' },
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
    if (mkdir_p("var/lib/dpkg/info"))
        ERR("can't mkdir -p 'var/lib/dpkg/info': %m\n");

    plf::colony<const char *> goals;
    for (int i=optind; i<argc; i++)
        goals.insert(argv[i]);
    plf::colony<std::string> packages = apt_sim(goals);
    for (auto c=packages.cbegin(); c!=packages.cend(); ++c)
        printf("%s: %s\n", c->c_str(), find_deb(orig_wd, 0, c->c_str(), nullptr)?"✓":"✗");
    return 0;

    tqueue slaves(unpack_thread, nthreads);
    for (int i=optind; i<argc; i++)
        slaves.put(argv[i]);
    slaves.finish();
    status_write();

    return 0;
}