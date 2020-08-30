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
    const static int EPOLL_WAIT_TIMEOUT = 10000;
    Selector(EventHandler && handler);
    virtual ~Selector() = default;
    virtual void add(int fd, int events, void* pdata) = 0;
    virtual void modify(int fd, int events, void* pdata) = 0;
    virtual void remove(int fd) = 0;
    virtual void select(int timeout = EPOLL_WAIT_TIMEOUT) = 0;
    virtual int fd() = 0;
    static std::unique_ptr<Selector> create_selector(EventHandler && handler);
};