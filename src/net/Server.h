#pragma once
#include "Transport.h"
#include "eventloop/ThreadingEventLoop.h"
#include "../utils/Common.h"
#include <memory>
#include <functional>
#include <iostream>
#include "TimingWheel.h"
#include <unordered_map>

using namespace std;
using namespace chrono;

class TcpServer : public NoCopyble {
protected:
    class TimeOutEntry {
    private:
        weak_ptr<Transport> conn;
    public:
        TimeOutEntry() {}
        explicit TimeOutEntry(const weak_ptr<Transport> & conn) : conn(conn) {}
        ~TimeOutEntry() {
            if (!conn.expired()) {
                conn.lock()->shutdown();
            }
        }
    };

protected:
    using LoopFetcher = function<EventLoop*()>;
    LoopFetcher get_loop;
    EventLoop* server_loop;

    Channel::Protocol* channel_protocol;
    TcpTransport::Protocol* client_protocol;
    TcpTransport::Protocol server_protocol;
    shared_ptr<Transport> server;
    chrono::seconds client_timeout;
    unordered_map<shared_ptr<Transport>, weak_ptr<TimeOutEntry>> connections;
    TimingWheel timing_wheel;
    vector<shared_ptr<Transport>> v;

public:
    TcpServer(
        const InetAddr & addr,
        const LoopFetcher & get_loop, 
        chrono::seconds timeout = 0s,
        TcpTransport::Protocol* protocol = &TcpTransport::default_protocol,
        Channel::Protocol* channel_protocol = &TcpTransport::default_channel_protocol)
    : get_loop(get_loop),
      server_loop(get_loop()),
      client_protocol(protocol),
      channel_protocol(channel_protocol),
      client_timeout(timeout),
      timing_wheel(server_loop, duration_cast<seconds>(10min).count()),
      server(new TcpServerAcceptor(server_loop, 
                    Socket::server_socket(addr), 0s, 
                    &server_protocol, channel_protocol)) {

    server_protocol = {
        {},
        [this](auto pconn) {
            auto socket = pconn->get_socket().accept();
            if (socket.fd() < 0) {
                socket.close();
            }
            else {
                auto pclient = shared_ptr<Transport>(new TcpTransport{
                    this->get_loop(), move(socket), client_timeout,
                    this->client_protocol, this->channel_protocol
                });
                connections.emplace(pclient, weak_ptr<TimeOutEntry>());
                pclient->activate();
            }
        }
    };
    if (!client_protocol->reset_timeout_cb) {
        client_protocol->reset_timeout_cb = 
            [this](auto pconn, const auto & timeout) {
            if (!connections.count(pconn)) {
                // error
            }
            else {
                if (connections[pconn].expired()) {
                    auto entry = make_shared<TimeOutEntry>(pconn);
                    connections[pconn] = entry;
                    timing_wheel.insert(timeout.count(), entry);
                }
                else {
                    timing_wheel.insert(
                        timeout.count(), 
                        connections[pconn].lock()
                    );
                }
            }
        };
    }
    
    auto conn_lost_cb = client_protocol->connection_lost_cb;
    if (conn_lost_cb) {
        client_protocol->connection_lost_cb = 
        [this, conn_lost_cb](auto pconn) {
            conn_lost_cb(pconn);
            connections.erase(pconn);
        };
    }
    else {
        client_protocol->connection_lost_cb = [this](auto pconn) {
            connections.erase(pconn);
        };
    }
}
    ~TcpServer() {
        for (auto [pconn, wptr] : connections) {
            wptr.reset();
        }
    }
    bool operator()() {
        return activate();
    }
    bool activate() {
        server_loop->call_soon([this]() {
            server->get_socket().listen();
            server->activate();
        });
        return true;
    }
};
