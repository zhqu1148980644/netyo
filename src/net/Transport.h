#pragma once

#include "eventloop/Channel.h"
#include "eventloop/ThreadingEventLoop.h"
#include <memory>
#include <queue>
#include "Socket.h"
#include "Buffer.h"
#include <chrono>


using namespace std;
using namespace chrono;

class Transport : public enable_shared_from_this<Transport> {
public:
    enum States {INITIAL, ACTIVATED, DISCONNECTING, DISCONNECTED};
protected:
    EventLoop * loop = nullptr;
    Socket socket;
    Channel channel;

    virtual void handle_onread() = 0;
    virtual void handle_onwrite() = 0;
    virtual void handle_onclose() = 0;
    virtual void handle_onerror() = 0;

public:
    int _state = INITIAL;
    Buffer rbuffer, wbuffer;
    static Channel::Protocol default_channel_protocol;

    Transport(EventLoop* loop, Socket && socket, Channel::Protocol* channel_protocol);
    virtual ~Transport() = 0;

    bool operator()();
    int state() const;
    void set_state(int state);
    Socket& get_socket();
    Channel& get_channel();
    EventLoop* set_event_loop(EventLoop* loop);
    EventLoop* get_event_loop() const;

    virtual bool activated();
    virtual bool is_closing();
    virtual bool closed();
    virtual bool is_reading();
    virtual bool is_writing();
    virtual void pause_reading();
    virtual void resume_reading();
    virtual void pause_writing();
    virtual void resume_writing();

    virtual bool activate() = 0;
    virtual void* set_transport_protocol(void * protocol) = 0;
    virtual void* get_transport_protocol() const = 0;
    virtual void send(const void* data, size_t len) = 0;
    virtual void send_file() = 0;
    virtual void close() = 0;
    virtual void force_close() = 0;
};


class TcpTransport : public Transport {
public:
    struct Protocol {
        using CallBack = function<void(shared_ptr<Transport>)>;
        CallBack connection_made_cb,
                 data_received_cb,
                 pause_writing_cb,
                 resume_writing_cb,
                 done_writing_cb,
                 eof_received_cb,
                 connection_lost_cb;
        function<void(shared_ptr<Transport>, const seconds&)> reset_timeout_cb;
    };

protected:
    Protocol* protocol = nullptr;
    chrono::seconds timeout = 0s;
    chrono::time_point<chrono::steady_clock> last_active;

    void reset_timeout();
    void done_writing();

    virtual void handle_onread() override;
    virtual void handle_onwrite() override;
    virtual void handle_onclose() override;
    virtual void handle_onerror() override;
public:
    static Protocol default_protocol;
    size_t write_highlevel = 4000, read_lowlevel = 500;

    TcpTransport(EventLoop* loop, Socket && socket, chrono::seconds timeout,
                 Protocol* protocol, 
                 Channel::Protocol* channel_protocol);

    virtual ~TcpTransport() override;
    virtual bool activate() override;
    virtual void* set_transport_protocol(void * protocol) override;
    virtual void* get_transport_protocol() const override;
    virtual void send(const void* data, size_t len) override;
    virtual void send_file() override;
    virtual void close() override;
    virtual void force_close() override;
};


class TcpServerAcceptor : public TcpTransport {
private:
    // maybe more control  
    virtual void handle_onread() override;
public:
    virtual bool activate() override;
    using TcpTransport::TcpTransport;
    virtual ~TcpServerAcceptor() override;
};