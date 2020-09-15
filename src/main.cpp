#include "net/Server.h"
#include "net/Transport.h"
#include <chrono>
#include <iostream>
#include <functional>

using namespace std;
using namespace chrono;
using namespace std::placeholders;

int main() {
    ThreadingEventLoop loop {4, "test"};

    string rsp_msg = 
R"(HTTP/1.1 200 OK
Content-Type: text/html; charset=UTF-8
Content-Length: 0
Last-Modified: Wed, 08 Jan 2003 23:11:55 GMT
Server: Apache/1.3.3.7 (Unix) (Red-Hat/Linux)
Accept-Ranges: bytes
Connection: Close

)";


    TcpTransport::Protocol protocol {
        [](auto pconn) {
            // cout << "Connection " << pconn->get_socket() << " made!" << endl;
        },
        [&rsp_msg](auto pconn) {
            auto [rit, red] = pconn->rbuffer.reader();
            string msg(rit, red);
            // cout << "Received Message" <<  msg << endl;
            // cout << "Sending back..." << endl;
            pconn->send(rsp_msg.data(), rsp_msg.size());
            pconn->close();
        }, {}, {}, {}, {},
        [](auto pconn) {
            // cout << "Conection " << pconn->get_socket() << " closed!" << endl;
        }
    };
    
    TcpServer server{
        {"0.0.0.0", 44567}, 
        bind(&ThreadingEventLoop::get_loop, &loop, _1), 
        1min,
        &protocol
    };
    server();
    cout << "Listening: " << server.get_socket() << endl;
    loop.wait();

    return 0;
}
