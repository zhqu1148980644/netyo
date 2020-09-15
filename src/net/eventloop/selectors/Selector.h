#pragma once

#include <memory>
#include "../../../utils/Common.h"
#include <vector>
#include <functional>

using namespace std;

class Channel;

class Selector : public NoCopyble {
public:
    using EventHandler = function<void(void*, int events)>;
protected:
    EventHandler handler;
public:
    static int EPOLL_WAIT_TIMEOUT;
    static int EVENTS_LIST_SIZE;
    Selector(EventHandler && handler);
    virtual ~Selector() = default;
    virtual void add(int fd, int events, void* pdata) = 0;
    virtual void modify(int fd, int events, void* pdata) = 0;
    virtual void remove(int fd) = 0;
    virtual void select(int timeout = EPOLL_WAIT_TIMEOUT) = 0;
    virtual int fd() = 0;
    static std::unique_ptr<Selector> create_selector(EventHandler && handler);
};