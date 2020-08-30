#include "Transport.h"
#include "eventloop/Channel.h"
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <memory>
#include <iostream>

using namespace std;


Channel::Protocol Transport::default_channel_protocol = {
    [](void * pthis) {
        static_cast<Transport*>(pthis)->handle_onread();
    },
    [](void * pthis) {
        static_cast<Transport*>(pthis)->handle_onwrite();
    },
    [](void * pthis) {
        static_cast<Transport*>(pthis)->handle_onclose();
    },
    [](void * pthis) {
        static_cast<Transport*>(pthis)->handle_onerror();
    }
};

Transport::Transport(EventLoop* loop, Socket && socket, 
    Channel::Protocol * channel_protocol = &default_channel_protocol)
    : loop(loop), 
      socket(move(socket)),
      channel("server",this->socket.fd(), this, loop->selector.get(), channel_protocol) {
    socket.setsockopt(SOL_SOCKET, SO_KEEPALIVE, 1);
}

Transport::~Transport() {
    channel.remove(); // no recode
}

bool Transport::operator()() {
    return activate();
}

int Transport::state() const {
    return _state;
}
void Transport::set_state(int state) {
    _state = state;
}
Socket& Transport::get_socket() {
    return socket;
}
Channel& Transport::get_channel() {
    return channel;
}
EventLoop* Transport::set_event_loop(EventLoop* loop) {
    EventLoop * prevloop = this->loop;
    this->loop = loop;
    return prevloop;
}
EventLoop* Transport::get_event_loop() const {
    return loop;
}

bool Transport::is_reading() {
    return channel.is_reading();
}
void Transport::pause_reading() {
    if (channel.is_reading())
        channel.disable(Channel::READ);
}
void Transport::resume_reading() {
    if (!channel.is_reading())
        channel.enable(Channel::READ);
}
bool Transport::is_closing() {
    return state() == DISCONNECTING || state() == DISCONNECTED;
}

/**********************************TcpTransport********************************/

void TcpTransport::reset_timeout() {
    if (timeout > 0s && steady_clock::now() - last_active > 3s) {
        if (protocol->reset_timeout_cb) {
            protocol->reset_timeout_cb(shared_from_this(), timeout);
            last_active = steady_clock::now();
        }
    }
}

void TcpTransport::send_in_loop(const void* data, size_t len) {
    if (state() != ACTIVATED) {

    }
    reset_timeout();
    int lensent = 0;
    if (channel.is_writing() && wbuffer.empty()) {
        lensent = socket.send(data, len);
        if (lensent < 0) {
            handle_onclose();
            return;
        }
    }
    if (lensent < len) {
        size_t bytes_write = wbuffer.feed_wbuffer(data, len);
        if (!channel.is_writing())
            channel.enable(Channel::WRITE);
        if (wbuffer.size() > write_highlevel && protocol->pause_writing_cb)
            protocol->pause_writing_cb(shared_from_this());
    }
}


void TcpTransport::handle_onread() {
    loop->assert_within_self_thread();

    int nbytes = rbuffer.feed_rbuffer(socket.fd());
    if (nbytes < 0) {
        handle_onerror();   
    }
    else if (nbytes == 0) {
        if (protocol->eof_received_cb)
            protocol->eof_received_cb(shared_from_this());
        handle_onclose();
    }
    else {
        reset_timeout();
        if (protocol->data_received_cb)
            protocol->data_received_cb(shared_from_this());
    }
}
void TcpTransport::handle_onwrite() {
    loop->assert_within_self_thread();
    if (!channel.is_writing()) {

    }
    reset_timeout();
    int res = wbuffer.drain_wbuffer(socket.fd());
    if (res == 0) {
        channel.disable(Channel::WRITE);
        if (protocol->done_writing_cb)
            protocol->done_writing_cb(shared_from_this());
        if (state() == DISCONNECTING) {
            force_close();
        }
    }
    else if (res < 0) {
        handle_onerror();
    }
}
void TcpTransport::handle_onclose() {
    loop->assert_within_self_thread();
    shutdown();

}

void TcpTransport::force_close() {
    if (state() != DISCONNECTED) {
        set_state(DISCONNECTED);
        channel.remove();
        socket.shutdown(SHUT_RDWR);
        socket.close();
        if (protocol->connection_lost_cb)
            protocol->connection_lost_cb(shared_from_this());
    }
    else {

    }
}

void TcpTransport::handle_onerror() {
    int error = socket.getsockerr();

    if (error == 0) return;
    channel.remove();
    if (error == EPIPE || error == ECONNRESET || error == 104) {
        // RST
    }
    else {
        // Error
    }
    shutdown();
}


// user interface
TcpTransport::TcpTransport(
    EventLoop* loop, 
    Socket && socket, 
    chrono::seconds timeout = chrono::seconds{0},
    Protocol* protocol = &default_protocol, 
    Channel::Protocol* channel_protocol = &default_channel_protocol)
    : timeout(timeout), 
      Transport(loop, move(socket), channel_protocol),
      protocol(protocol) {

}

TcpTransport::~TcpTransport() {

}

bool TcpTransport::activate() {
    loop->call_soon([=]() {
        socket.setsockopt(SOL_SOCKET, SO_KEEPALIVE, true);
        if (protocol->connection_made_cb)
            protocol->connection_made_cb(shared_from_this());
        channel.enable(Channel::READ);
        set_state(ACTIVATED);
        reset_timeout();
    });
    return true;
}

void* TcpTransport::set_transport_protocol(void * protocol) {
    void * prev_protocol = this->protocol;
    this->protocol = static_cast<TcpTransport::Protocol*>(protocol);
    return prev_protocol;
}
void* TcpTransport::get_transport_protocol() const {
    return protocol;
}
void TcpTransport::send(const void* data, size_t len) {
    // need mutex? or some times(when there are no send_in_loop in queue) can just call send_in_loop?
    if (len) {
        if (!channel.is_writing()) {
            channel.enable(Channel::WRITE);
        }
        loop->call_soon([=](){
            send_in_loop(data, len);
        });
    }
}
void TcpTransport::send_file() {
    // wait for implement
}
void TcpTransport::shutdown() {
    loop->call_soon([this](){
        if (state() == ACTIVATED && !is_closing()) {
            if (!channel.is_writing()) {
                force_close();
            }
            else {
                set_state(DISCONNECTING);
            }
        }
    });
}

/***************************TcpServer*******************************/

TcpServerAcceptor::~TcpServerAcceptor() {

}

bool TcpServerAcceptor::activate() {
    loop->call_soon([=]() {
        socket.setsockopt(SOL_SOCKET, SO_KEEPALIVE, true);
        channel.enable(Channel::READ);
        set_state(ACTIVATED);
    });
    return true;
}

void TcpServerAcceptor::handle_onread() {
    if (socket.getsockerr() != 0) {
        handle_onerror();
        handle_onclose();
    }
    else {
        protocol->data_received_cb(shared_from_this());
    }
}
