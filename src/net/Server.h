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
                auto sp = conn.lock();
                sp->force_close();
            }
        }
    };

protected:
    using LoopFetcher = function<EventLoop*(bool)>;
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
      server_loop(get_loop(true)),
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
                int sockfd = 0;
                do {
                    auto socket = pconn->get_socket().accept();
                    sockfd = socket.fd();
                    if (sockfd < 0) {
                        socket.close();
                    }
                    else {
                        auto pclient = shared_ptr<Transport>(new TcpTransport{
                            this->get_loop(false), move(socket), client_timeout,
                            this->client_protocol, this->channel_protocol
                        });
                        connections.emplace(pclient, weak_ptr<TimeOutEntry>());
                        pclient->activate();
                    }
                } while (sockfd > 0);
            }
        };
        if (!client_protocol->reset_timeout_cb) {
            client_protocol->reset_timeout_cb = 
                [this](auto pconn, const auto & timeout) {
                server_loop->call_soon([=]() {
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
                });
            };
        }
        
        auto conn_lost_cb = client_protocol->connection_lost_cb;
        // must use call_later, as some contoller of channel mightbe predead.
        // TODO: Find another way to handle Segmentaion Fault caused by this
        if (conn_lost_cb) {
            client_protocol->connection_lost_cb = 
            [this, conn_lost_cb](auto pconn) {
                conn_lost_cb(pconn);
                server_loop->call_later([pconn, this]() { 
                    connections.erase(pconn); 
                }, 2s);
            };
        }
        else {
            client_protocol->connection_lost_cb = 
            [this](auto pconn) {
                server_loop->call_later([pconn, this] () {
                    connections.erase(pconn);
                }, 2s);
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
        server_loop->call_every([&]() {
            cout << "Server states: sockerr:" << server->get_socket().getsockerr() << " "
                 << "num connections: " << connections.size() << endl;
        }, 5s);
        return true;
    }

    const Socket & get_socket() const {
        return server->get_socket();
    }
};
