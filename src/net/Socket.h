// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
// Modification: bakezq

#pragma once
#include "../utils/Common.h"
#include "string"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <errno.h>
#include <iostream>



//     /* Structure describing an Internet socket address.  */
//     struct sockaddr_in {
//         sa_family_t    sin_family; /* address family: AF_INET */
//         uint16_t       sin_port;   /* port in network byte order */
//         struct in_addr sin_addr;   /* internet address */
//     };

//     /* Internet address. */
//     typedef uint32_t in_addr_t;
//     struct in_addr {
//         in_addr_t       s_addr;     /* address in network byte order */
//     };

//     struct sockaddr_in6 {
//         sa_family_t     sin6_family;   /* address family: AF_INET6 */
//         uint16_t        sin6_port;     /* port in network byte order */
//         uint32_t        sin6_flowinfo; /* IPv6 flow information */
//         struct in6_addr sin6_addr;     /* IPv6 address */
//         uint32_t        sin6_scope_id; /* IPv6 scope-id */
//     };



class InetAddr {
private:
    union {
        sockaddr_in addr;
        sockaddr_in6 addr6;
    };
public:
    InetAddr() : addr6{}  {}
    InetAddr(const sockaddr_in & addr) : addr(addr) {}
    InetAddr(const sockaddr_in6 & addr) : addr6(addr) {}
    InetAddr(const string & ipaddr, uint16_t port) : addr6{} {
        if (::inet_pton(AF_INET, ipaddr.c_str(), &addr.sin_addr) > 0) {
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
        }
        else if (::inet_pton(AF_INET6, ipaddr.c_str(), &addr6.sin6_addr) > 0) {
            addr6.sin6_family = AF_INET6;
            addr6.sin6_port = htons(port);
        }
        else {
            // error
        }
    }
    InetAddr(const InetAddr & other) : addr6(other.addr6) {}
    InetAddr& operator=(const InetAddr & other) {
        addr6 = other.addr6;
        return *this;
    }
    const struct sockaddr * sockaddr() const {
        return static_cast<const struct sockaddr*>((void *)&addr6);
    }

    socklen_t sockaddrlen() const {
        return is_ipv4() ? 
                    static_cast<socklen_t>(sizeof(sockaddr_in)) 
                  : static_cast<socklen_t>(sizeof(sockaddr_in6));
    }

    bool is_ipv4() const {
        return addr.sin_family == AF_INET;
    }

    operator string() const {
        return ip() + ":" + to_string(port());
    }

    string ip() const {
        char buf[64];
        if (is_ipv4()) {
            ::inet_ntop(AF_INET, &addr.sin_addr, buf, 
                static_cast<socklen_t>(sizeof(buf)));
        }
        else {
            ::inet_ntop(AF_INET6, &addr6.sin6_addr, buf,
                static_cast<socklen_t>(sizeof(buf)));
        }
        return buf;
    }

    uint16_t port() const {
        return addr.sin_port;
    }

    sa_family_t family() const {
        return addr.sin_family;
    }
};


class Socket : public NoCopyble {
private:
    int sock_fd = -1;
    InetAddr localaddr;

public:
    static const int DEFAULT_FAMILY = AF_INET,
                     DEFAULT_PROTO = IPPROTO_TCP,
                     DEFAULT_TYPE = SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC;

    Socket() {}
    Socket(int family, int type, int proto)
        : sock_fd(::socket(family, type, proto)) {
        if (sock_fd < 0) {
            // error
            exit(1);
        }
    }
    Socket(int sock_fd) : sock_fd(sock_fd), localaddr(getsockname()) {
        if (sock_fd < 0) {
            // error
        }
    }
    Socket(const Socket & other) = delete;
    Socket(Socket && other) {
        swap(sock_fd, other.sock_fd);
        swap(localaddr, other.localaddr);
    }
    ~Socket() {
        close();
    }
    static Socket server_socket(const InetAddr & localaddr) {
        Socket sock = Socket(DEFAULT_FAMILY, DEFAULT_TYPE, DEFAULT_PROTO);
        sock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
    #ifdef SO_REUSEPORT
        sock.setsockopt(SOL_SOCKET, SO_REUSEPORT, 1);
    #endif
        if (sock.bind(localaddr) < 0) {
            // erro
            // ERROR << "Binding " << string(localaddr) << " Failed! \n";
        }
        return sock;
    }

    void setsockopt(int level, int optname, int value) {
        ::setsockopt(sock_fd, level, optname, &value, 
                     static_cast<socklen_t>(sizeof(value)));
    }

    int bind(const InetAddr & localaddr) {
        int res = ::bind(sock_fd, 
                         localaddr.sockaddr(), localaddr.sockaddrlen());

        if (res < 0) {
            // ERROR << "Binding " << string(localaddr) << " Failed! \n";
        }
        else {
            this->localaddr = getsockname();
        }
        return res;
    }

    int connect(const InetAddr & localaddr) {
        int res = ::connect(sock_fd, 
                            localaddr.sockaddr(), localaddr.sockaddrlen());
        if (res < 0) {
            // error
        }
        else {
            this->localaddr = getsockname();
        }
        return res;
    }

    int listen() {
        int res = ::listen(sock_fd, SOMAXCONN);
        if (res < 0) {
            // error
            // ERROR << "Listening " << string(localaddr) << " Failed.\n";
        }
        return res;
    }

    Socket accept() {
        sockaddr_in6 addr6 {};
        socklen_t size = sizeof(addr6);
        int connfd = ::accept4(sock_fd, 
                               static_cast<sockaddr*>((void*)&addr6),
                               &size,
                               SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (connfd < 0) {
            // WARNING << "Accept failed with error code " << connfd << "\n";
        }
        return connfd;
    }

    int shutdown(int op = SHUT_WR) {
        int res = ::shutdown(sock_fd, op);
        if (res < 0) {
            // error
        }
        return res;
    }

    int close() {
        int res = 0;
        if (sock_fd > 0) 
            ::close(sock_fd);
        return res;
    }

    int getsockerr() {
        int optval = 0;
        socklen_t optlen = static_cast<socklen_t>(sizeof(optval));
        if (::getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
            return errno;
        else
            return optval;
    }

    sockaddr_in6 getsockname() {
        sockaddr_in6 addr6{};
        socklen_t addrlen = static_cast<socklen_t>(sizeof(addr6));
        if (::getsockname(sock_fd,
                          static_cast<sockaddr*>((void*)&addr6),
                          &addrlen) < 0) {
            // error
        }
        return addr6;
    }

    sockaddr_in6 getpeername() {
        sockaddr_in6 addr6{};
        socklen_t addrlen = static_cast<socklen_t>(sizeof(addr6));
        if (::getpeername(sock_fd,
                          static_cast<sockaddr*>((void*)&addr6),
                          &addrlen) < 0) {
            // error
        }
        return addr6;
    }

    operator string() {
        return string(localaddr) + "|" + string(InetAddr(getpeername()));
    }

    int fd() const {
        return sock_fd;
    }

    ssize_t send(const void* data, size_t len) {
        return ::write(sock_fd, data, len);
    }
};


