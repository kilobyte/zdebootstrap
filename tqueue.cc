#include "tqueue.h"
#include "nproc.h"
#include "util.h"
#include "zdebootstrap.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int64_t time0 = 0;
#define NANO 1000000000

static int64_t getticks(void)
{
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return tv.tv_sec * NANO + tv.tv_nsec;
}

tqueue::tqueue(tworker_t *w, int nthreads): worker(w), idle(0), done(0)
{
    pthread_mutex_init(&mut, nullptr);
    pthread_cond_init(&moar, nullptr);

    unspawned = nthreads? nthreads : get_nproc();
    // Disabled due to a bug in plf::colony 5.09    slaves.reserve(unspawned);

    if (!time0)
        time0 = getticks();
}

tqueue::~tqueue(void)
{
    finish();
    kill_slaves();

    pthread_cond_destroy(&moar);
    pthread_mutex_destroy(&mut);
}

void tqueue::saytime(const char *state, const std::string& task)
{
    if (verbose < 2)
        return;
    int64_t t=getticks() - time0;
    printf("%4d.%06d %s %s\n", (int)(t/NANO), (int)(t%NANO/1000), state, task.c_str());
}

static void* slaveth(void *arg)
{
    ((tqueue *)arg)->slave();
    return 0;
}

void tqueue::slave(void)
{
    while (1)
    {
        // q is non-empty on the first iteration, mutex is locked.
        std::string task = q.front();
        q.pop();
        pthread_mutex_unlock(&mut);

        saytime("⇒", task);
        worker(task.c_str());
        saytime("✓", task);

        pthread_mutex_lock(&mut);
        task_done(task);

        while (q.empty())
        {
            if (done)
            {
                pthread_mutex_unlock(&mut);
                return;
            }

            idle++;
            pthread_cond_wait(&moar, &mut);
            idle--;
        }
    }
}

void tqueue::put(std::string item, bool spawn)
{
    pthread_mutex_lock(&mut);
    q.push(std::move(item));

    if (idle)
        pthread_cond_signal(&moar);
    else if (spawn && unspawned)
    {
        unspawned--;

        pthread_t th;
        if (pthread_create(&th, 0, slaveth, (void*)this))
            ERR("can't create thread: %m");
        slaves.insert(th);
    }
    pthread_mutex_unlock(&mut);
}

void tqueue::wakeall(void)
{
    pthread_mutex_lock(&mut);
    pthread_cond_broadcast(&moar);
    pthread_mutex_unlock(&mut);
}

void tqueue::kill_slaves(void)
{
    for (auto c = slaves.begin(); c!=slaves.end(); c=slaves.erase(c))
        pthread_join(*c, nullptr);
}

/****************************************************************************/

// This can't currently handle all reqs for b being already met.
void tqueue::req(std::string a, std::string b)
{
    if (tasks_done.count(a))
        return;
    reqs[b].emplace(a);
    want[a].emplace(b);
}

void tqueue::task_done(std::string task)
{
#ifndef NDEBUG
    if (tasks_done.count(task))
        ERR("Task “%s” finished twice!\n", task.c_str());
#endif
    tasks_done.insert(task);

    auto aw = want.find(task);
    if (aw == want.end())
        return;
    // Tell everyone who waits for us.
    for (auto c = aw->second.cbegin(); c!=aw->second.cend(); ++c)
    {
        auto r = reqs.find(*c);
        if (r == reqs.end())
            continue;
        r->second.erase(task);
        if (!r->second.size())
        {
            pthread_mutex_unlock(&mut);
            put(*c);
            pthread_mutex_lock(&mut);
        }
    }

    want.erase(aw);
}
