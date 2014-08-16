all:exe
exe: TestDynamicThreadPool.cpp
	g++ -std=c++11 -g -O2 -o $@ $^ -lpthread -DDEBUG_THREAD_POOL
clean:
	rm -rf *~ exe
