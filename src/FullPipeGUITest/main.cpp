#include <iostream>
#include <ipc.h>
#include <string>
#include <thread>
#include <mutex>
#include <boost/algorithm/string.hpp>
#include <vector>
#include <chrono>

static std::string g_recv_socket = "/tmp/gstreamer_ipc_test2.sock";
using ms = std::chrono::milliseconds;

struct Packet{
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    std::string label;
    std::size_t pts;
    double confidence;
};

int receiveRoutine()
{
    auto poller = HddlUnite::Poller::create();
    auto connection = HddlUnite::Connection::create(poller);
    std::cout<<"Received socket sets to listen"<<std::endl;
    if (!connection->listen(g_recv_socket)) { //add to poller here
        return -1;
    }

    volatile bool running = true;

    while (running) {
        auto event = poller->waitEvent(100);
        switch (event.type) {
        case HddlUnite::Event::Type::CONNECTION_IN:
            connection->accept();
            std::cout<<"Recv Socket incoming connecttion accepted"<<std::endl;
            std::cout<<"x\t\ty\t\twidth\t\theight\t\tlabel\t\tpts\t\tconfidence"<<std::endl;
            break;
        case HddlUnite::Event::Type::MESSAGE_IN: {

            int length = 0;
            auto& data_connection = event.connection;
            std::lock_guard<std::mutex> autoLock(data_connection->getMutex());

            if (!data_connection->read(&length, sizeof(length))) {
                break;
            }

            if (length <= 0) {
                break;
            }

            std::string serialized(static_cast<size_t>(length), ' ');
            if (!data_connection->read(&serialized[0], length)) {
                break;
            }

            //parse data to elements.
            const int element_nums = 7;
            std::vector<std::string> fields;
            boost::split(fields, serialized, boost::is_any_of(","));
            if (!fields.empty()) {
                fields.pop_back();
            }
            if (fields.size() < 7 || fields.size() % element_nums != 0) {
                std::cout << serialized << std::endl;
                break;
            }
            for(unsigned i =0; i<fields.size();i+=7){
                std::cout<<fields[i]<<"\t\t"<<fields[i+1]<<"\t\t"<<fields[i+2]<<"\t\t"<<fields[i+3]<<"\t\t"<<
                        fields[i+4]<<"\t\t"<<fields[i+5]<<"\t\t"<<fields[i+6]<<std::endl;
            }

            break;
        }
        case HddlUnite::Event::Type::CONNECTION_OUT:
            running = false;
            break;

        default:
            break;
        }
    }
}

int main()
{

    std::thread t(receiveRoutine);

    std::this_thread::sleep_for(ms(2000));

    auto poller = HddlUnite::Poller::create("client");
    auto conn = HddlUnite::Connection::create(poller);
    while(!conn->connect("/tmp/gstreamer_ipc111.sock")){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    int length = sizeof(g_recv_socket);
    {
        std::lock_guard<std::mutex> lock(conn->getMutex());
        int result = conn->write((void*)&length, sizeof(length));
        if(result != sizeof(length)){
            std::cout<<"Failed to send socket length!"<<std::endl;
            exit(1);
        }
        std::cout<<"Sent length "<<length<<" in "<<sizeof(length)<<" bytes"<<std::endl;
        result = conn->write(&g_recv_socket[0], length);
        if(result != length){
            std::cout<<"Failed to send socket addr!"<<std::endl;
            exit(1);
        }
        std::cout<<"Sent addr "<<g_recv_socket<<" in "<<length<<" bytes"<<std::endl;
    }
    t.join();
    return 0;
}
