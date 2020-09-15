#pragma once
#include "Selector.h"
#include "../Channel.h"
#include <vector>
#include <sys/epoll.h>

using namespace std;


class EpollSelector : public Selector {
public:
    using EventList = vector<epoll_event>;
private:
    int epoll_fd;
    EventList events;

    void update(int op, int fd, int newevents = 0, void* pdata = nullptr);
public:

    EpollSelector(Selector::EventHandler && handler);
    ~EpollSelector() override;
    virtual void select(int timeout) override;
    virtual void add(int fd, int events, void* pdata) override;
    virtual void modify(int fd, int events, void* pdata) override;
    virtual void remove(int fd) override;
    virtual int fd() override;
};