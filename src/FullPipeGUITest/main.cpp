#include <iostream>
#include <ipc.h>
#include <string>
#include <thread>
#include <mutex>
#include <boost/algorithm/string.hpp>
#include <vector>
#include <chrono>
#include <iomanip>

static std::string g_recv_socket = "/tmp/gstreamer_ipc_recv.sock";
using ms = std::chrono::milliseconds;

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
            std::cout<<"x\t\ty\t\twidth\t\theight\t\tlabel\t\t\t\tpts\t\tconfidence\t\tinferFps\t\tdecFps"<<std::endl;
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
            const int element_nums = 9;
            std::vector<std::string> fields;
            boost::split(fields, serialized, boost::is_any_of(","));
            if (!fields.empty()) {
                fields.pop_back();
            }
            if (fields.size() < element_nums || fields.size() % element_nums != 0) {
                std::cout << serialized << std::endl;
                break;
            }
            for(unsigned i =0; i<fields.size();i+=element_nums){
                std::cout<<std::fixed<<std::setprecision(2)<<fields[i]<<"\t\t"<<fields[i+1]<<"\t\t"<<fields[i+2]<<"\t\t"<<fields[i+3]<<"\t\t"<<
                        fields[i+4]<<"\t\t\t\t"<<fields[i+5]<<"\t\t"<<fields[i+6]<<"\t\t"<<fields[i+7]<<"\t\t"<<fields[i+8]<<std::endl;
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

int main(int argc, const char* argv[])
{

    std::thread t(receiveRoutine);

    std::this_thread::sleep_for(ms(2000));

    auto poller = HddlUnite::Poller::create("client");
    auto conn = HddlUnite::Connection::create(poller);
    while(!conn->connect("/tmp/gstreamer_ipc111.sock")){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::string toSend = g_recv_socket;
    if(argc > 1){
        toSend = g_recv_socket + ",/tmp/gstreamer_ipc_recv.sock"+std::string(argv[1]);
    }
    int length = static_cast<int>(toSend.length());
    {
        std::lock_guard<std::mutex> lock(conn->getMutex());
        int result = conn->write((void*)&length, sizeof(length));
        if(result != sizeof(length)){
            std::cout<<"Failed to send socket length!"<<std::endl;
            exit(1);
        }
        std::cout<<"Sent length "<<length<<" in "<<sizeof(length)<<" bytes"<<std::endl;
        result = conn->write(&toSend[0], length);
        if(result != length){
            std::cout<<"Failed to send socket addr!"<<std::endl;
            exit(1);
        }
        std::cout<<"Sent addr "<<toSend<<" in "<<length<<" bytes"<<std::endl;
    }
    t.join();
    return 0;
}
