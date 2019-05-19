#include "tqueue.h"
#include "nproc.h"
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
    slaves.reserve(unspawned);

    if (!time0)
        time0 = getticks();
}

tqueue::~tqueue(void)
{
    finish();
    kill_slaves();

    pthread_cond_destroy(&moar);
    pthread_mutex_destroy(&mut);

    while (!q.empty())
    {
        free(q.front());
        q.pop();
    }
}

void tqueue::saytime(const char *state, const char *task)
{
    if (verbose < 2)
        return;
    int64_t t=getticks() - time0;
    printf("%4d.%06d %s %s\n", (int)(t/NANO), (int)(t%NANO/1000), state, task);
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
        char *task = q.front();
        q.pop();
        pthread_mutex_unlock(&mut);

        saytime("⇒", task);
        worker(task);
        saytime("✓", task);
        free(task);

        pthread_mutex_lock(&mut);

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

void tqueue::put(const char *item, bool spawn)
{
    pthread_mutex_lock(&mut);
    q.push(strdup(item));

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
