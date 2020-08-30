#pragma once

#include <functional>
#include <sys/eventfd.h>
#include <unistd.h>
#include "../../utils/Common.h"
#include "MpscQueue.h"
#include "Channel.h"
#include "selectors/Selector.h"


using namespace std;


// uses a LockFree atomic Multiproducer/single consumer queue
// Only resourse operations outsise mpsc queue needs to run in the loop's thread
class TaskQueue {
protected:
    using CallBack = function<void()>;
    using PChannel = shared_ptr<Channel>;
    using RunInLoopCallBack = function<void(CallBack)>;

    int wakeup_fd;
    PChannel pch;
    MpscQueue<CallBack> tasks;
    RunInLoopCallBack run_in_loop;

    void handle_wakeup();
    static int create_event_fd();
    static void write_event_fd(int wakeup_fd);
    static void read_event_fd(int wakeup_fd);
    
public:
    TaskQueue(const RunInLoopCallBack & run_in_loop, 
                         Selector* selector);
    ~TaskQueue();

    static Channel::Protocol channel_protocol;

    void reset();

    CallBack && pop();

    void push(const CallBack & cb);
    void push(CallBack && cb);

};

