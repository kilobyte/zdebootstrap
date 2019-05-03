#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <unistd.h>
#include <pthread.h>
#ifdef linux
# include <numa.h>
#endif

#include "nproc.h"

#define MAX_SANITY 4096

unsigned get_nproc(void)
{
    int n;
#ifdef linux
    // Intentionally #ifdef linux rather than HAVE_NUMA.
    /*
     * libnuma is best: handles both offline cores and affinity mask.
     * Alas, it can fail in some cases.
     *
     * It works by parsing /proc/self/status -- it might be tempting to
     * reimplement that ourselves, but there are obscure quirks libnuma
     * handles.  For example, one of my boxes says Cpus_allowed_list: 0-127
     * despite being only -j64.
     *
     * Usual fail mode: returns -1.
     */
    n = numa_num_task_cpus();
    if (n>0 && n<=MAX_SANITY)
        return n;
#endif

#ifdef __USE_GNU
    /*
     * pthreads can't handle offline cores.  On Linux, it's a wrapper over
     * sched_getaffinity() which behaves the same.
     *
     * Usual fail mode: a bogus value like 4286574024 (0xff7fedc8).
     * Usual wrongness: returns 1 unless box is already busy.
     */
    cpu_set_t set;
    if (!pthread_getaffinity_np(pthread_self(), sizeof(set), &set))
    {
        n = CPU_COUNT(&set);
        if (n>0 && n<=MAX_SANITY)
            return n;
    }
#endif

    /*
     * sysconf can give ONLN or CONF; neither handles affinity mask.
     *
     * Never seen it fail, man page says it returns -1.
     * Wrongness: seen it return 2 on a -j64 box.
     */
    n = sysconf(_SC_NPROCESSORS_CONF);
    if (n>0 && n<=MAX_SANITY)
        return n;

    return 1;
}
