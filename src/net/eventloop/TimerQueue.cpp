#include "TimerQueue.h"
#include <sys/times.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <atomic>
#include <vector>
#include <time.h>
#include <chrono>

using namespace std;
using namespace chrono;


atomic<TimerId> Timer::num_timers {0};

Timer::Timer(CallBack cb, 
             const Time & when, 
             const Interval & interval)
    : _when(when),
        callback(move(cb)),
        interval(interval), _id(num_timers++) {}

Timer::Timer(CallBack cb,
             const Interval & interval)
    : callback(move(cb)), _when(chrono::steady_clock::now()),
        interval(interval), _id(num_timers++) {}



Channel::Protocol TimerQueue::channel_protocol = {
    [](void * pthis) {
        static_cast<TimerQueue*>(pthis)->handle_exprired();
    }
};

TimerQueue::TimerQueue(const RunInLoopCallBack & run_in_loop, 
                              Selector * selector)
    : run_in_loop(run_in_loop),
      timer_fd(create_timer_fd()),
      pch(new Channel(timer_fd, this, selector, &channel_protocol)) {
        pch->enable(Channel::READ);
}

TimerQueue::~TimerQueue() {
    run_in_loop([this]() {
        ::close(timer_fd);
    });
}

void TimerQueue::handle_exprired() {
    read_timer_fd(timer_fd);
    auto now = std::chrono::steady_clock::now();
    while (timers.size() && timers.top()->when() < now) {
        auto ptimer = timers.top(); timers.pop();
        if (valid_timers.count(ptimer->id())) {
            ptimer->run();
            if (ptimer->is_repeat()) {
                insert(ptimer);
            }
            else {
                valid_timers.erase(ptimer->id());
            }
        }
    }
    if (timers.size()) {
        write_timer_fd(timer_fd, timers.top()->when());
    }
    else {
    }
}


void TimerQueue::remove_timer(TimerId timer_id) {
    run_in_loop([=](){
        erase(timer_id);
    });
}

void TimerQueue::insert(const PTimer & timer_node) {
    valid_timers.insert(timer_node->id());
    timers.push(timer_node);
}

void TimerQueue::erase(TimerId timer_id) {
    valid_timers.erase(timer_id);
}

int TimerQueue::create_timer_fd() {
    int timer_fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer_fd < 0) {
        // error
    }
    return timer_fd;;
}

void TimerQueue::write_timer_fd(int timer_fd, const Time & expire_time) {
    auto remain_ms = 1500ms;
    auto now = steady_clock::now();
    if (expire_time > now + 1500ms) {
        remain_ms = duration_cast<milliseconds>(expire_time - now);
    }
    auto sec = duration_cast<seconds>(remain_ms).count();
    auto nanosec = duration_cast<nanoseconds>(remain_ms).count();
    itimerspec newtime {{}, {sec, (nanosec % (sec * nanoseconds::period::den))}};
    itimerspec oldtime{};

    int res = ::timerfd_settime(timer_fd, 0, &newtime, &oldtime);
    if (res < 0) {
        // 
    }
}

void TimerQueue::read_timer_fd(int timer_fd) {
    uint64_t tmp;
    ssize_t n = ::read(timer_fd, &tmp, sizeof(tmp));
    if (n < 0) {
        // error
    }
}