#include "zdebootstrap.h"
#include "status.h"
#include <pthread.h>
#include <stdio.h>

static pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER;
static deb822 status;

void status_add(std::map<std::string, std::string> &&par)
{
    pthread_mutex_lock(&status_mutex);
    status.contents.insert(par);
    pthread_mutex_unlock(&status_mutex);
}

void status_write()
{
    pthread_mutex_lock(&status_mutex);
    FILE *f=fopen("var/lib/dpkg/status", "we");
    if (!f)
        ERR("can't create dpkg status file: %m\n");
    status.fprint(f);
    if (fclose(f))
        ERR("can't write dpkg status file: %m\n");
    pthread_mutex_unlock(&status_mutex);
}
