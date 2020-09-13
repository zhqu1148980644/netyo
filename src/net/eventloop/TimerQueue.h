#pragma once

#include "../../utils/Common.h"
#include "Channel.h"
#include <queue>
#include <deque>
#include <unordered_set>
#include <memory>
#include <unistd.h>
#include "selectors/Selector.h"
#include <chrono>



using Time = chrono::steady_clock::time_point;
using Interval = chrono::microseconds;
using TimerId = u_int64_t;


class Timer {
private:
    using CallBack = function<void()>;

    static atomic<TimerId> num_timers;

    Time _when;
    const TimerId _id;
    const Interval interval;
    CallBack callback;


public:
    Timer(CallBack cb, 
          const Time & when, 
          const Interval & interval = 0ms);

    Timer(CallBack cb,
        const Interval & interval = 0ms);


    bool operator<(const Timer & t2) const {
        return _when < t2._when;
    }

    bool is_repeat() const {
        return interval > 0ms;
    }
    
    TimerId id() const {
        return _id;
    }

    void run() {
        callback();
        if (interval > 0ms)
            _when = chrono::steady_clock::now() + interval;
    }
    
    Time when() const { return _when; }
    void set_time(const Time & newtime) { _when = newtime; }
};


using namespace std;
using namespace chrono;

// all resource operations are not atomic, need use call_soon for inclusive calling.
class TimerQueue : public NoCopyble {
protected:
    using PTimer = shared_ptr<Timer>;
    using PChannel = shared_ptr<Channel>;
    using RunInLoopCallBack = function<void(function<void()>)>;

    struct Cmp {
        bool operator()(const PTimer & pt1, const PTimer & pt2) {
            return *pt2 < *pt1;
        }
    };

    int timer_fd;
    PChannel pch;
    unordered_set<TimerId> valid_timers;
    priority_queue<PTimer, deque<PTimer>, Cmp> timers;    
    RunInLoopCallBack run_in_loop;

    void handle_exprired();

    void reset(const vector<PTimer> & expired, const Time & time);
    void insert(const PTimer & timer_node);
    void erase(TimerId timer_id);
    static int create_timer_fd();
    static void write_timer_fd(int timer_fd, const Time & expire_time);
    static void read_timer_fd(int timer_fd);

public:
    TimerQueue(const RunInLoopCallBack & run_in_loop, Selector* selector);
    ~TimerQueue();

    static Channel::Protocol channel_protocol;

    template <typename Func>
    TimerId add_timer(Func && cb, 
                    const Time & time, 
                    const Interval & interval = 0ms) {
        auto timer_node = make_shared<Timer>(forward<Func>(cb), 
                                              time, interval);
        run_in_loop([=](){
            if (timers.empty() || timer_node < timers.top()) {
                write_timer_fd(timer_fd, timer_node->when());
            }
            insert(timer_node);
        });
        return timer_node->id();
    }
    void remove_timer(TimerId timer_id);
    void reset();
};