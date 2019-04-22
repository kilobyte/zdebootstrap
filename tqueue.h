#include <queue>
#include <stack>
#include <pthread.h>

typedef void tworker_t(const char *arg);

struct tqueue
{
    tqueue(tworker_t *worker);
    ~tqueue(void);
    void put(const char *item);
    void finish(void) { done=1; wakeall(); }
    //void halt(void) { done=2; wakeall(); }
    void slave(void);
private:
    void wakeall(void);
    void kill_slaves(void);
    std::queue<char *> q;
    std::stack<pthread_t> slaves;
    tworker_t *worker;
    int unspawned;
    int idle;
    int done;
    pthread_mutex_t mut;
    pthread_cond_t moar;
};
