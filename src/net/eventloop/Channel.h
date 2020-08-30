#pragma once

#include <memory>
#include <functional>
#include <poll.h>
#include <assert.h>
#include "selectors/Selector.h"
#include <type_traits>
#include <iostream>
using namespace std;

class Channel {
public:
    struct Protocol {
        using CallBack = function<void(void *)>;
        CallBack read_cb, write_cb, close_cb, error_cb;
    };
    static Protocol default_protocol;

public:
    enum Events {
        READ = POLLIN | POLLPRI, 
        WRITE = POLLOUT, 
        NONE = 0
    };
    enum Operation {ADD = 1, MOD = 2, DEL = 3};

private:
    enum class States {NEW, ADDED, DELETED, DESTROYED};

    const int _fd;
    void * controller;
    Selector* selector;

    States _state = States::NEW;
    int _events = 0, _prev_events = 0;

    Protocol* protocol;
    string name;
public:
    using CallBack = Protocol::CallBack;


    Channel(string name, int fd, void * controller, Selector* selector, 
            Protocol* protocol = &default_protocol)
        : name(name), _fd(fd), controller(controller), selector(selector), 
          protocol(protocol) {}

    int fd() const { return _fd; }
    int events() const { return _events; }
    int prev_events() const { return _prev_events; }
    operator bool() const { return _events != 0; }
    bool is_writing() const { return _events & WRITE; }
    bool is_reading() const { return _events & READ; }


    void enable(int evs) {
        _events |= evs;
        notify();
    }
    void disable(int evs) {
        _events &= ~evs;
        notify();
    }
    void enable_all() {
        enable(READ | WRITE);
        notify();
    }
    void disable_all() {
        disable(READ | WRITE);
        notify();
    }
    void update_events(int evs) {
        if (_events != evs) {
            _events = evs;
            notify();
        }
    }
    void remove() {
        disable_all();
    }

    void destroy() {
        _state = States::DESTROYED;
    }

    void notify() {
        if (_state == States::DESTROYED)
            return;
        if (_state == States::NEW || _state == States::DELETED) {
            _state = States::ADDED;
            selector->add(_fd, _events, this);
        }
        else {
            assert(_state == States::ADDED);
            if (*(this)) {
                selector->modify(_fd, _events, this);
            }
            else {
                selector->remove(_fd);
                _state = States::DELETED;
            }
        }
    }

    void handle_events(int newevents) {
        if (_state == States::DESTROYED)
            return;
        if ((newevents & POLLHUP) && !(newevents & POLLIN)) {
            if (protocol->close_cb) protocol->close_cb(controller);
        }
        if (newevents & (POLLNVAL | POLLERR)) {
            if (protocol->error_cb) protocol->error_cb(controller);
        }
        if (newevents & ((POLLIN | POLLPRI) | POLLRDHUP)) {
            if (protocol->read_cb) protocol->read_cb(controller);
        }
        if (newevents & POLLOUT) {
            if (protocol->write_cb) protocol->write_cb(controller);
        }
        _prev_events = newevents;
    }
};

