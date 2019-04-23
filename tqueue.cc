#include "tqueue.h"
#include "zdebootstrap.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

tqueue::tqueue(tworker_t *w, int nthreads): worker(w), idle(0), done(0)
{
    pthread_mutex_init(&mut, nullptr);
    pthread_cond_init(&moar, nullptr);

    unspawned = nthreads? nthreads : sysconf(_SC_NPROCESSORS_CONF);
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

        worker(task);
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

void tqueue::put(const char *item)
{
    pthread_mutex_lock(&mut);
    q.push(strdup(item));

    if (idle)
        pthread_cond_signal(&moar);
    else if (unspawned)
    {
        unspawned--;

        pthread_t th;
        if (pthread_create(&th, 0, slaveth, (void*)this))
            ERR("can't create thread: %m");
        slaves.push(th);
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
    pthread_t th;
    while (!slaves.empty())
    {
        th = slaves.top();
        slaves.pop();
        pthread_join(th, nullptr);
    }
}
