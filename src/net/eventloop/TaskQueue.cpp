#include "TaskQueue.h"

Channel::Protocol TaskQueue::channel_protocol = {
    [](void * pthis) {
        static_cast<TaskQueue*>(pthis)->handle_wakeup();
    }
};

TaskQueue::TaskQueue(const RunInLoopCallBack & run_in_loop,
                                    Selector* selector)
    : wakeup_fd(create_event_fd()),
      pch(new Channel(wakeup_fd, this, selector, &channel_protocol)),
      run_in_loop(run_in_loop) {
   pch->enable(Channel::READ);
}

TaskQueue::~TaskQueue() {
    run_in_loop([this]() {
        pch->remove();
        ::close(wakeup_fd);
    });
}

void TaskQueue::handle_wakeup() {
    read_event_fd(wakeup_fd);
    CallBack func;
    while (tasks.dequeue(func)) {
        func();
    }
}

TaskQueue::CallBack && TaskQueue::pop() {
    // no need to use call_soon, the mpsc queue can ensures no conficts
    CallBack func;
    tasks.dequeue(func);
    return move(func);
}

int TaskQueue::create_event_fd() {
    int event_fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (event_fd < 0) {
        // error
    }
    return event_fd;
}

void TaskQueue::write_event_fd(int wakeup_fd) {
    uint64_t wakeup = 1;
    ssize_t res = ::write(wakeup_fd, &wakeup, sizeof(wakeup));
    if (res < 0) {
        // error
    }
}

void TaskQueue::read_event_fd(int wakeup_fd) {
    uint64_t tmp;
    ssize_t res = ::read(wakeup_fd, &tmp, sizeof(tmp));
    if (res < 0) {
        // 
    }
}

void TaskQueue::push(const CallBack & cb) {
    // no need to use call_soon, the mpsc queue can ensures no conficts
    tasks.enqueue(cb);
    write_event_fd(wakeup_fd);
}

void TaskQueue::push(CallBack && cb) {
    // no need to use call_soon, the mpsc queue can ensures no conficts
    tasks.enqueue(move(cb));
    write_event_fd(wakeup_fd);
}
