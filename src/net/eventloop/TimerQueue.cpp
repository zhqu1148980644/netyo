#include "TimerQueue.h"
#include <sys/times.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <atomic>
#include <vector>
#include <time.h>

using namespace std;



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
      pch(new Channel("Timer queue",timer_fd, this, selector, &channel_protocol)) {
        pch->enable(Channel::READ);
}

TimerQueue::~TimerQueue() {
    run_in_loop([this]() {
        pch->remove();
        ::close(timer_fd);
    });
}

void TimerQueue::handle_exprired() {
    read_timer_fd(timer_fd);
    vector<PTimer> expired;
    auto now = std::chrono::steady_clock::now();
    while (!timers.empty()) {
        if (timers.top()->when() < now) {
            auto ptimer = timers.top(); timers.pop();
            if (valid_timers.count(ptimer->id())) {
                expired.push_back(ptimer);
                ptimer->run();
            }
        }
        else break;
    }
    reset(expired, now);
}

void TimerQueue::reset(const vector<PTimer> & expired, const Time & time) {
    for (auto const & ptimer : expired) {
        // ptimer should be valid
        if (ptimer->is_repeat())
            insert(ptimer);
    }
    if (!timers.empty()) {
        write_timer_fd(timer_fd, timers.top()->when());
    }
}

void TimerQueue::reset() {

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
    auto remainmsc = chrono::duration_cast<chrono::microseconds>(
        expire_time - chrono::steady_clock::now()
    );
    auto num_msc = remainmsc.count();
    itimerspec oldtime{};
    itimerspec newtime {{},
        {num_msc / decltype(remainmsc)::period::den,
        (num_msc % decltype(remainmsc)::period::den) * 1000}
    };

    int res = ::timerfd_settime(timer_fd, 0, &newtime, &oldtime);
}

void TimerQueue::read_timer_fd(int timer_fd) {
    uint64_t tmp;
    ssize_t n = ::read(timer_fd, &tmp, sizeof(tmp));
    if (n != sizeof(tmp)) {
        // error
    }
}