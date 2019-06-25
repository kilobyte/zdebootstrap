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

static plf::colony<const char *> goals;
static plf::colony<std::string> plan;
static void apt_fetch(void);
static void got_package(const char *pav);
static tqueue *tq;
static pthread_mutex_t done_mut = PTHREAD_MUTEX_INITIALIZER;

static void zd_task(const char* task)
{
    const char *spc = strchrnul(task, ' ');

    #define TASK(x) if (!strncmp(task, (x), spc-task))
    TASK("apt-avail")
        apt_avail();
    else TASK("apt-sim")
        plan = apt_sim(goals);
    else TASK("fetch")
        apt_fetch();
    else TASK("unpack")
    {
        int fd;
        if (!find_deb(orig_wd, deb_avail_size(spc+1), spc+1, &fd))
            ERR("can't find .deb for %s\n", spc+1);
        deb pkg(spc+1);
        pkg.open_file(fd);
        pkg.unpack();
    }
    else TASK("configure")
        printf("configure: not implemented.\n");
    else TASK("done")
        pthread_mutex_unlock(&done_mut);
    else ERR("unknown task: “%s”\n", task);
}

static void apt_fetch(void)
{
    for (auto c=plan.begin(); c!=plan.end(); )
    {
        const char *pav = c->c_str();
        if (find_deb(orig_wd, deb_avail_size(pav), pav, nullptr))
        {
            got_package(pav);
            c=plan.erase(c);
        }
        else
        {
            ++c;
            printf("%s: ✗\n", pav);
        }
    }

    if (plan.empty())
        return;

    apt_download(plan, got_package);
}

static void got_package(const char *pav)
{
    printf("%s: ✓\n", pav);

    char buf[264];
    snprintf(buf, sizeof(buf), "unpack %s", pav);
    tq->req(buf, "configure");
    tq->put(buf);
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

    pthread_mutex_lock(&done_mut);
    tqueue slaves(zd_task, nthreads);
    tq=&slaves;
    slaves.req("apt-avail", "fetch");
    slaves.req("apt-sim", "fetch");
    slaves.req("fetch", "configure");
    slaves.req("configure", "done");
    slaves.put("apt-avail");

    for (int i=optind; i<argc; i++)
        goals.insert(argv[i]);
    slaves.put("apt-sim");

    pthread_mutex_lock(&done_mut);
    slaves.finish();
    status_write();

    return 0;
}
