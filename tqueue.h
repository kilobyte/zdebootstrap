#include <queue>
#include <plf_colony.h>
#include <pthread.h>
#include <set>
#include <unordered_map>

typedef void tworker_t(const char *arg);

struct tqueue
{
    tqueue(tworker_t *worker, int nthreads);
    ~tqueue(void);
    void put(const char *item, bool spawn=true);
    void finish(void) { done=1; wakeall(); kill_slaves(); }
    //void halt(void) { done=2; wakeall(); kill_slaves(); }
    void slave(void);
    void req(std::string a, std::string b);
private:
    void wakeall(void);
    void kill_slaves(void);
    void saytime(const char *state, const char *task);
    std::queue<char *> q;
    plf::colony<pthread_t> slaves;
    tworker_t *worker;
    int unspawned;
    int idle;
    int done;
    pthread_mutex_t mut;
    pthread_cond_t moar;

    void task_done(std::string task);
    std::set<std::string> tasks_done;
    std::unordered_map<std::string, std::set<std::string>> want, reqs;
};
