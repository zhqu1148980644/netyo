#pragma once
#include <memory>
#include <functional>
#include "TimerQueue.h"
#include "TaskQueue.h"
#include "../../utils/Logging.h"
#include "../../utils/Common.h"
#include "selectors/Selector.h"
#include <thread>
#include <future>
using namespace std;
using namespace chrono;


class EventLoop {
public:
    unique_ptr<Selector> selector;
protected:
    TaskQueue taskq;
    TimerQueue timerq;

    
    bool events_handling = false, _close = false, _closed = false;  
    void * current_active_channel = nullptr;

    thread::id belonging_thread;
    EventLoop** thread_loop_pptr = nullptr;
    string name;

public:
    EventLoop(string name);
    ~EventLoop();
    void close();

    EventLoop* get_loop();

    bool within_self_thread() const {
        return belonging_thread == this_thread::get_id();
    }

    void assert_within_self_thread() {
        if (!within_self_thread()) {
            exit(1);
        }
    }

    void run();
    void operator()();
    void handle_event(void * pdata, int event);

    template <typename CallBack>
    void call_soon(CallBack&& cb) {
        if (within_self_thread() && !_close) {
            cb();
        }
        else if (!_closed) {
            taskq.push(forward<CallBack>(cb));
        }
    }

    template <typename CallBack>
    void call_later(CallBack&& cb, const microseconds & delay = 0s) {
        return call_at(forward<CallBack>(cb), steady_clock::now() + delay);
    }

    template <typename CallBack>
    void call_at(CallBack&& cb, const time_point<steady_clock> & time) {
        if (time > steady_clock::now()) {
            timerq.add_timer(forward<CallBack>(cb), time);
        }
    }

    template <typename CallBack>
    void call_every(CallBack&& cb, const microseconds & interval) {
        timerq.add_timer(forward<CallBack>(cb), 
                         steady_clock::now() + interval, 
                         interval);
    }
};



class ThreadingEventLoop;

class EventLoopThread : public NoCopyble {
protected:
    friend class ThreadingEventLoop;

    EventLoop* loop = nullptr;
    string loopname;
    thread loopthread;

    promise<EventLoop *> running_loop;
    void start_loop();
    EventLoop* get_loop();

public:
    EventLoopThread(const string & name);
    ~EventLoopThread();
    const string & name() const;
    void close();
    void join();
    void wait();
};



class ThreadingEventLoop {
private:
    int thread_num = 1;
    string poolname;
    vector<shared_ptr<EventLoopThread>> threads;
    int thread_index = 0; 
public:
    static string BASE_THREAD_NAME;

    ThreadingEventLoop(int num = 3, const string & name = "MainThreadingPool");
    ~ThreadingEventLoop() = default;
    int size() const;
    void close();
    void join();
    void wait();

    EventLoop * get_loop(bool mainloop);

    template <typename CallBack>
        void call_soon(CallBack&& cb);

    template <typename CallBack>
    void call_later(CallBack&& cb, const microseconds & delay = 0ms) {

    }

    template <typename CallBack>
    void call_at(CallBack&& cb, const time_point<steady_clock> & time) {

    }

    template <typename CallBack>
    void call_every(CallBack&& cb, const microseconds & interval) {

    }
};