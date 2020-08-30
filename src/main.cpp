#include "net/Server.h"
#include "net/Transport.h"
#include <chrono>
#include <iostream>

using namespace std;

using namespace std;
using namespace chrono;

int main() {
    ThreadingEventLoop loop {4, "test"};

    TcpTransport::Protocol protocol {
        [](auto pconn) {
            cout << "Connection " << string(pconn->get_socket()) << " made!" << endl;
        },
        [](auto pconn) {
            auto [rit, red] = pconn->rbuffer.reader();
            auto [wit, wed] = pconn->wbuffer.writer();
            string msg;
            copy(rit, red, back_inserter(msg));
            cout << "Received Message" <<  msg << endl;
            cout << "Sending back..." << endl;
            pconn->send(msg.data(), msg.size());
            pconn->shutdown();
        }, {}, {}, {}, {},
        [](auto pconn) {
            cout << "Conection " << string(pconn->get_socket()) << " closed!" << endl;
        }
    };
    
    TcpServer server{
        {"0.0.0.0", 44567}, 
        bind(&ThreadingEventLoop::get_loop, &loop), 
        30s,
        &protocol
    };
    cout << "configure done" << endl;
    server();
    loop.wait();

    return 0;
}