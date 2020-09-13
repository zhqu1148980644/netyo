#include "net/Server.h"
#include "net/Transport.h"
#include <chrono>
#include <iostream>

using namespace std;

using namespace std;
using namespace chrono;

int main() {
    ThreadingEventLoop loop {4, "test"};

    string rsp_msg = 
R"(HTTP/1.1 200 OK
Connection: Keep-Alive
Content-Encoding: gzip
Content-Type: text/html; charset=utf-8
Date: Thu, 11 Aug 2016 15:23:13 GMT
Last-Modified: Mon, 25 Jul 2016 04:32:39 GMT
Server: Apache

<html>
    <body>
        <div>
        <p> Hello World </p>
        </div>
    </body>
</html>
)";


    TcpTransport::Protocol protocol {
        [](auto pconn) {
            cout << "Connection " << string(pconn->get_socket()) << " made!" << endl;
        },
        [&rsp_msg](auto pconn) {
            auto [rit, red] = pconn->rbuffer.reader();
            auto [wit, wed] = pconn->wbuffer.writer();
            string msg;
            copy(rit, red, back_inserter(msg));
            // cout << "Received Message" <<  msg << endl;
            // cout << "Sending back..." << endl;
            pconn->send(rsp_msg.data(), msg.size());
        }, {}, {}, {}, {},
        [](auto pconn) {
            cout << "Conection " << string(pconn->get_socket()) << " closed!" << endl;
        }
    };
    
    TcpServer server{
        {"0.0.0.0", 44567}, 
        bind(&ThreadingEventLoop::get_loop, &loop), 
        1min,
        &protocol
    };
    cout << "configure done" << endl;
    server();
    loop.wait();

    return 0;
}