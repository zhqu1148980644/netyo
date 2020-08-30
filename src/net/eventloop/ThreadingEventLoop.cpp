#include "ThreadingEventLoop.h"
#include <thread>
#include <iostream>
#include <functional>
using namespace std;
using namespace std::placeholders;


thread_local EventLoop * thread_loop_ptr = nullptr;

EventLoop::EventLoop(string name)
    : name(name), 
      belonging_thread(this_thread::get_id()),
      selector(Selector::create_selector(
          bind(&EventLoop::handle_event, this, _1, _2))
      ),
      taskq([this](auto && callback) {
          this->call_soon(forward<decltype(callback)>(callback));
      }, selector.get()),
      timerq([this](auto && callback) {
          this->call_soon(forward<decltype(callback)>(callback));
      }, selector.get()),
      thread_loop_pptr(&thread_loop_ptr) {

    if (thread_loop_ptr) {
        // error
        exit(-1);
    }
    thread_loop_ptr = this;
}

EventLoop::~EventLoop() {
    close();
    if (events_handling) {
        // error

    }
    if (thread_loop_ptr == this)
        thread_loop_ptr = nullptr;
    if (!_closed) {
        // error
    }
}

void EventLoop::handle_event(void * pdata, int event) {
    current_active_channel = pdata;
    static_cast<Channel*>(pdata)->handle_events(event);
    current_active_channel = nullptr;
}

void EventLoop::close() {
    if (!_close) {
        _close = true;
        call_soon([](){ return; });
    }
}

EventLoop* EventLoop::get_loop() {
    return this;
}

void EventLoop::operator()() {
    run();
}

void EventLoop::run() {
    if (events_handling || !within_self_thread()) {
        // error
    }
    while (!_close) {
        events_handling = true;
        selector->select();
        events_handling = false;
    }
    if (_close) {
        _closed = true;
    }
}


EventLoopThread::EventLoopThread(const string & name) 
    : loopname(name),
      loopthread([this]() { start_loop(); }) {
    loop = running_loop.get_future().get();
}

EventLoopThread::~EventLoopThread() {
    if (loop) {
        close();
    }
}

void EventLoopThread::start_loop() {
    EventLoop loop(loopname);
    loop.call_soon([this, &loop]() {
        running_loop.set_value(&loop);
    });
    loop.run();
    this->loop = nullptr;
}

const string & EventLoopThread::name() const {
    return loopname;
}

EventLoop* EventLoopThread::get_loop() {
    return loop;
}

void EventLoopThread::close() {
    loop->close();
    join();
    loop = nullptr;
}

void EventLoopThread::join() {
    if (loop) {
        loopthread.join();
    }
}

void EventLoopThread::wait() {
    join();
}


string ThreadingEventLoop::BASE_THREAD_NAME = "Thread-";

ThreadingEventLoop::ThreadingEventLoop(int num, const string & name)
    : thread_num(num > 0 ? num : 1),
      poolname(name) {
    for (int i = 0; i < thread_num; i++) {
        threads.emplace_back(
            make_shared<EventLoopThread>(BASE_THREAD_NAME + to_string(i))
        );
    }
}

void ThreadingEventLoop::close() {
    for (auto & pth : threads) {
        pth->close();
    }
}

void ThreadingEventLoop::join() {
    for (auto & pth : threads) {
        pth->join();
    }
}

void ThreadingEventLoop::wait() {
    join();
}

EventLoop* ThreadingEventLoop::get_loop() {
    EventLoop* next_loop =  threads[thread_index]->get_loop();
    thread_index = thread_index + 1 >= threads.size() ? 0 : thread_index + 1;
    return next_loop;
}