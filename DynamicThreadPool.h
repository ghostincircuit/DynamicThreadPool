#pragma once

#include <list>
#include <queue>
#include <pthread.h>
#include <semaphore.h>
#include <cassert>

#ifdef DEBUG_THREAD_POOL
#include <iostream>
#endif

class DynamicThreadPool {
public:
        typedef void *(*MyThreadFunction)(void *param);
        //sz:expected thread pool size
        DynamicThreadPool(size_t sz);
        //
        ~DynamicThreadPool();
        //add a workload to threadpool
        bool add(MyThreadFunction func, void *param=NULL);
        //get the number of threads in threadpool
        size_t getPoolSize();
        //set the new number of threads in threadpool, the number of threads would
        //increaase/decrease accordingly
        bool setPoolSize(size_t n);
        //get the number of pending(not worked on) workloads
        size_t getPending();
private:
        //<<<<<<<<<<lock protected
        pthread_mutex_t lock;
        size_t threads;
        size_t to_kill;
        bool should_stop;
        //>>>>>>>>>>unlock protected
        pthread_cond_t quit_cond;
        sem_t activities;//all threads need 1 for 1 action
        struct Workload {
                Workload() {};
                Workload(MyThreadFunction f, void *p) : work(f), param(p) {}
                MyThreadFunction work;
                void *param;
        };
        std::queue<Workload, std::list<Workload> > req_q;
        struct Worker {
                inline Worker(DynamicThreadPool *p) : pool(p) {};
                DynamicThreadPool *pool;
                static void *worker_func(void *data);
        };
};

inline DynamicThreadPool::DynamicThreadPool(size_t sz)
                    : threads(0),
                    to_kill(0),
                    lock(PTHREAD_MUTEX_INITIALIZER),
                    quit_cond(PTHREAD_COND_INITIALIZER),
                    should_stop(false) {
        sem_init(&activities, 0, 0);
        setPoolSize(sz);
}

inline DynamicThreadPool::~DynamicThreadPool() {
        //flag all threads to quit
        //<<<<<<<<<<lock
        pthread_mutex_lock(&lock);
        should_stop = true;
        to_kill = threads;
        //read the following comment to know how necessary this is!!!
        const int kill_count = to_kill;
        pthread_mutex_unlock(&lock);
        //>>>>>>>>>>unlock
        //note that we should not use threads here because some
        //threads may already begin to suicdie, we need a sure and
        //definite value!!
        for (int i = 0; i < kill_count; i++)
                sem_post(&activities);

        //wait for the last thread to end
        //<<<<<<<<<<lock
        pthread_mutex_lock(&lock);
        to_kill = threads;
        if (threads != 0)
                pthread_cond_wait(&quit_cond, &lock);
        assert(to_kill == 0);
        assert(threads == 0);
        pthread_mutex_unlock(&lock);
        //>>>>>>>>>>unlock
        pthread_cond_destroy(&quit_cond);
        pthread_mutex_destroy(&lock);
        sem_destroy(&activities);
}

inline bool DynamicThreadPool::add(MyThreadFunction func, void *param) {
        bool ret;
        //<<<<<<<<<<lock
        pthread_mutex_lock(&lock);
        ret = !should_stop;
        if (ret) {
                req_q.emplace(func, param);
                sem_post(&activities);
        }
        pthread_mutex_unlock(&lock);
        //>>>>>>>>>>unlock
        return ret;
}

inline size_t DynamicThreadPool::getPoolSize() {
        size_t ret;
        //<<<<<<<<<<lock
        pthread_mutex_lock(&lock);
        ret = threads;
        pthread_mutex_unlock(&lock);
        //>>>>>>>>>>unlock
        return ret;
}

inline bool DynamicThreadPool::setPoolSize(size_t n) {
        bool ret;
        //<<<<<<<<<<lock
        pthread_mutex_lock(&lock);
        //note that we may already be in the process of killing
        //what really matters is the number of sem_posts we have done
        //so that makes some worker wake with nothing to do but to continue
        //waiting
        int diff = n - threads;//note that this may be negative
        ret = !should_stop;
        if (should_stop || diff == 0) {
        } else if (diff > 0) {
                while (diff--) {
                        pthread_t threadid;
                        Worker *d = new Worker(this);
                        pthread_create(&threadid,
                                       NULL,
                                       Worker::worker_func,
                                       (void *)d);
                        pthread_detach(threadid);
#ifdef DEBUG_THREAD_POOL
                        std::cout << "add 1 worker" << std::endl;
#endif//DEBUG_THREAD_POOL
                }
                to_kill = 0;//thus some worker may wake up and find nothing to do
                threads = n;
        } else {// if (diff < 0) {
                size_t new_to_kill = -diff;
                size_t old_to_kill = to_kill;
                int more_to_kill = new_to_kill - old_to_kill;
                to_kill = new_to_kill;
                if (more_to_kill > 0) {
                        for (int i = 0; i < more_to_kill; i++)
                                sem_post(&activities);
                }
        }
        pthread_mutex_unlock(&lock);
        //>>>>>>>>>>unlock
        return ret;
}

inline size_t DynamicThreadPool::getPending() {
        size_t ret;
        //<<<<<<<<<<lock
        pthread_mutex_lock(&lock);
        ret = req_q.size();
        pthread_mutex_unlock(&lock);
        //>>>>>>>>>>unlock
        return ret;
}

inline void *DynamicThreadPool::Worker::worker_func(void *data) {
        //DynamicThreadPool *pool = static_cast<Workload *>data->pool;
        Worker * worker = (Worker *)data;
        DynamicThreadPool *pool = worker->pool;
        delete worker;
        while (1) {
                enum {SUICIDE, WORK, WAIT} todo;
                Workload load;
                sem_wait(&pool->activities);
                //<<<<<<<<<<lock
                pthread_mutex_lock(&pool->lock);
                if (pool->to_kill) {
                        pool->to_kill--;
                        pool->threads--;
                        //this assignment should exist to differ
                        //WAIT and WORK
                        todo = SUICIDE;
                        if (pool->threads == 0 && pool->should_stop)
                                pthread_cond_signal(&pool->quit_cond);
#ifdef DEBUG_THREAD_POOL
                        std::cout << "quit 1 worker, "
                                  << pool->threads << " left" << std::endl;
#endif//DEBUG_THREAD_POOL
                } else if (!pool->req_q.empty()) {
                        load = pool->req_q.front();pool->req_q.pop();
                        todo = WORK;
                } else {
                        todo = WAIT;
                        //this happens when user of pool is frequently setting
                        //pool size to different value
                }
                pthread_mutex_unlock(&pool->lock);
                //>>>>>>>>>>unlock
                if (todo == WORK)
                        (void)load.work(load.param);
                else if (todo == SUICIDE) {
                        return NULL;
                }
        }
}
