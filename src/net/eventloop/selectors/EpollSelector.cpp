#include "EpollSelector.h"
#include <poll.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <assert.h>
#include <iostream>
using namespace std;

EpollSelector::EpollSelector(Selector::EventHandler && handler)
    : Selector(move(handler)),
      epoll_fd(::epoll_create1(EPOLL_CLOEXEC)),
      events(EVENTS_LIST_SIZE) {}

EpollSelector::~EpollSelector() {
    ::close(epoll_fd);
}

int EpollSelector::fd() {
    return epoll_fd;
}

void EpollSelector::select(int timeout) {
    int num_events = static_cast<size_t>(::epoll_wait(
        epoll_fd, 
        &events.front(), 
        static_cast<int>(events.size()),
        timeout
    ));
    if (errno != EINTR) {
        // FATAL << "Epoll wait failed with error: " << errno << "\n";
    }
    if (num_events <= 0) {
        return;
    }
    else {
        assert(num_events <= events.size());
        for (int i = 0; i < num_events; ++i) {
            if (events[i].data.ptr) {
                handler(events[i].data.ptr, events[i].events);
            }
        }

        if (num_events == events.size()) {
            events.resize(events.size() * 2);
        }
        else if (num_events < events.size() / 4) {
            events.resize(events.size() / 4);
        }
    }
}

void EpollSelector::add(int fd, int events, void* pdata) {
    update(EPOLL_CTL_ADD, fd, events, pdata);
}

void EpollSelector::modify(int fd, int events, void* pdata) {
    update(EPOLL_CTL_MOD, fd, events, pdata);
}

void EpollSelector::remove(int fd) {
    update(EPOLL_CTL_DEL, fd);
}

void EpollSelector::update(int op, int fd, int newevents, void* pdata) {
    epoll_event event{};
    event.events = newevents | EPOLLET;
    event.data.ptr = pdata;
    if (::epoll_ctl(epoll_fd, op, fd, &event) < 0) {
        // error
        // FATAL << "Update epoll event failed \n";
    }
}
