#include "DynamicThreadPool.h"
#include <pthread.h>
#include <unistd.h>
#include <iostream>
#include <cstdlib>
using namespace std;

void test0()
{
        DynamicThreadPool pool(32);
}

void test1()
{
/*
  add one, end one, add one, end one
 */
        const int N = 16;
        DynamicThreadPool pool(4);
        auto func1 =
                [] (void *self) {
                int n = (int)self;
                sleep(1);
                cout << n << " started" << endl;
                sleep(1);
                cout << n << " finished" << endl;
                return (void *)NULL;
        };
        for (int i = 0; i < N; i++) {
                pool.add(func1, (void *)(i));
                cout << "add " << i << endl;
                sleep(3);
        }
        //cout << "end of function" << endl;
}

void test2()
{
/*
  add add add add .... wait
 */
        const int N = 16;
        DynamicThreadPool pool(4);
        auto func1 =
                [] (void *self) {
                int n = (int)self;

                cout << n << " started" << endl;
                sleep(8);
                cout << n << " finished" << endl;
                return (void *)NULL;
        };
        for (int i = 0; i < N; i++) {
                pool.add(func1, (void *)(i));
                cout << "add " << i << endl;
                cout << "pending: " << pool.getPending() << endl;
                sleep(1);
        }
        cout << "end of function" << endl;
}

void *test3_func_work(void *self)
{
        int r = (int)self;
        cout << "work: " << r << endl;
        sleep(4);
        return NULL;
};

void test3()
{//a thrad would continously add
//main thread would adjust the size of pool, 2->4->6
        auto func_add =
                [] (void *self) -> void * {
                auto pool = static_cast<DynamicThreadPool *>(self);
                for (auto i = 0; i < 16; i++) {
                        cout << "add " << i << endl;
                        pool->add(test3_func_work, (void *)i);
                        sleep(1);
                }
                return NULL;
        };
        //DynamicThreadPool p(2);
        auto p = new DynamicThreadPool(2);
        pthread_t adder;
        pthread_create(&adder, NULL, func_add, (void *)p);
        sleep(6);
        //p.setPoolSize(4);
        p->setPoolSize(4);
        int sz;
        do {
                sleep(1);
                //sz = p.getPoolSize();
                sz = p->getPoolSize();
                cout << "pool size: " << sz << endl;
        } while(sz != 4);
        sleep(1);
        //p.setPoolSize(6);
        p->setPoolSize(6);
        cout << "waiting" << endl;
        delete p;
        cout << "over" << endl;
}

void test4()
{
/*
1 thread add 1 work per second
1 thread adjust pool size randomly
*/
        auto func_add =
                [] (void *self) -> void * {
                auto pool = static_cast<DynamicThreadPool *>(self);
                for (auto i = 0; i < 32; i++) {
                        cout << "add " << i << endl;
                        pool->add(test3_func_work, (void *)i);
                        sleep(1);
                }
                return NULL;
        };
        auto func_set = 
                [] (void *self) -> void * {
                auto pool = static_cast<DynamicThreadPool *>(self);
                sleep(4);
                cout << "pool size to 1" << endl;
                pool->setPoolSize(1);
                sleep(10);
///*
                cout << "pool size to 4" << endl;
                pool->setPoolSize(4);
                sleep(4);
                cout << "pool size to 8" << endl;
                pool->setPoolSize(8);
                sleep(4);
                cout << "pool size to 2" << endl;
                pool->setPoolSize(2);
//*/
                return NULL;
        };
        auto p = new DynamicThreadPool(2);
        pthread_t adder, setter;
        pthread_create(&adder, NULL, func_add, (void *)p);
        sleep(2);
        pthread_create(&setter, NULL, func_set, (void *)p);
        assert(pthread_join(setter, NULL) == 0);
        assert(pthread_join(adder, NULL) == 0);
        while(1) {
                int r = p->getPending();
                sleep(1);
                if (r == 0)
                        break;
        }
        cout << "delete" << endl;
        delete p;
        cout << "over" << endl;
}

void test5()
{
/*
1 thread add 1 work per second
1 thread adjust pool size wildly randomly
main thread finally wait pending queue to be empty and then delete
*/
        auto func_add =
                [] (void *self) -> void * {
                auto pool = static_cast<DynamicThreadPool *>(self);
                for (auto i = 0; i < 32; i++) {
                        cout << "add " << i << endl;
                        pool->add(test3_func_work, (void *)i);
                        sleep(1);
                }
                return NULL;
        };
        auto func_set = 
                [] (void *self) -> void * {
                srandom(3);
                auto pool = static_cast<DynamicThreadPool *>(self);
                for (auto i = 0; i < 16; i++) {
                        sleep(2);
                        int s = random()%8;
                        pool->setPoolSize(s+1);
                        cout << "pool size set to" << s+1 << endl;
                }
                return NULL;
        };
        auto p = new DynamicThreadPool(2);
        pthread_t adder, setter;
        pthread_create(&adder, NULL, func_add, (void *)p);
        sleep(2);
        pthread_create(&setter, NULL, func_set, (void *)p);
        assert(pthread_join(setter, NULL) == 0);
        assert(pthread_join(adder, NULL) == 0);
        while(1) {
                int r = p->getPending();
                sleep(1);
                if (r == 0)
                        break;
        }
        cout << "delete" << endl;
        //sleep(1);
        delete p;
        cout << "over" << endl;
}

void test6()
{
/*
1 thread add 1 work per second
1 thread adjust pool size wildly randomly
main thread does not wait pending queue to be empty
*/
        auto func_add =
                [] (void *self) -> void * {
                auto pool = static_cast<DynamicThreadPool *>(self);
                for (auto i = 0; i < 32; i++) {
                        cout << "add " << i << endl;
                        pool->add(test3_func_work, (void *)i);
                        sleep(1);
                }
                return NULL;
        };
        auto func_set = 
                [] (void *self) -> void * {
                srandom(3);
                auto pool = static_cast<DynamicThreadPool *>(self);
                for (auto i = 0; i < 16; i++) {
                        sleep(2);
                        int s = random()%8;
                        pool->setPoolSize(s+1);
                        cout << "pool size set to" << s+1 << endl;
                }
                return NULL;
        };
        auto p = new DynamicThreadPool(2);
        pthread_t adder, setter;
        pthread_create(&adder, NULL, func_add, (void *)p);
        sleep(2);
        pthread_create(&setter, NULL, func_set, (void *)p);
        assert(pthread_join(setter, NULL) == 0);
        assert(pthread_join(adder, NULL) == 0);
/*
        while(1) {
                int r = p->getPending();
                sleep(1);
                if (r == 0)
                        break;
        }
*/
        cout << "delete" << endl;
        delete p;
        cout << "over" << endl;
}

int main()
{

        test0();
        test1();
        test2();
        test3();
        test4();
        test5();
        test6();
        //test7();

        //test1();
        return 0;
}
