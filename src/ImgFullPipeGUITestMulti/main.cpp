#include <iostream>
#include <ipc.h>
#include <string>
#include <thread>
#include <mutex>
#include <boost/algorithm/string.hpp>
#include <vector>
#include <chrono>
#include <iomanip>
#include <cstdlib>
#include <sstream>

static std::string g_recv_socket = "/tmp/gstreamer_ipc_first.sock";
using ms = std::chrono::milliseconds;

int receiveRoutine(unsigned id)
{
    auto poller = HddlUnite::Poller::create();
    auto connection = HddlUnite::Connection::create(poller);
    std::string sockAddr = g_recv_socket+std::to_string(id);
    std::string titleCoordStr = "\033["+std::to_string(20+id*3)+";0H";
    std::string coordStr = "\033["+std::to_string(21+id*3)+";0H";
    // std::cout<<"Received socket sets to listen on "<<sockAddr<<std::endl;
    if (!connection->listen(sockAddr)) { //add to poller here
        return -1;
    }

    volatile bool running = true;

    while (running) {
        auto event = poller->waitEvent(100);
        switch (event.type) {
        case HddlUnite::Event::Type::CONNECTION_IN:
            connection->accept();
            // std::cout<<"Recv Socket incoming connecttion accepted"<<std::endl;
            {
                std::stringstream ss;
                ss<<titleCoordStr<<"x\ty\twidth\theight\tlabel\t\t\t\tpts\t\tconfidence\t\tinferFps\t\tpipeLineFps\t\tImageName\t\t";
                std::cout<<ss.str();
            }
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
            const int element_nums = 10;
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
                std::stringstream ss;
                if(std::stof(fields[i+7])>9999)
                    fields[i+7] = "NaN";

                ss<<coordStr<<"\033[2K";
                std::cout<<ss.str();

                ss.str(std::string());
                ss<<coordStr<<std::fixed<<std::setprecision(2)<<fields[i]<<"\t"<<fields[i+1]<<"\t"<<fields[i+2]<<"\t"<<fields[i+3]<<"\t"<<
                        std::setw(32)<<std::left<<fields[i+4]<<fields[i+5]<<"\t\t"<<fields[i+6]<<"\t\t"<<fields[i+7]<<"\t\t"<<fields[i+8]<<"\t\t"<<fields[i+9];
                std::cout<<ss.str();
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
    return 0;
}

int main(int argc, const char* argv[])
{
    // std::cout<<"\033[2J\033[0;0H";
    // std::cout<<"Line1"<<std::endl;
    // std::cout<<std::endl;

    // std::cout<<"Line3"<<std::endl;
    // std::cout<<std::endl;

    // std::cout<<"Line5"<<std::endl;
    // std::cout<<std::endl;

    // std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    // std::cout<<"\033[0;0HNewLineCord0,0"<<std::endl;
    // std::cout<<"\033[3;0HNewLineCord3,0"<<std::endl;
    // std::cout<<"\033[5;0HNewLineCord5,0"<<std::endl;

    if(argc != 2){
        std::cout<<"Usage: FullPipeGUITestMulti [socketnum]"<<std::endl;
        exit(0);
    }

    int sockNum = atoi(argv[1]);
    if(sockNum<1){
        sockNum=1;
    }
    if(sockNum>8){
        sockNum=8;
    }

    std::vector<std::thread> vTh;
    vTh.reserve(sockNum);

    for(unsigned i =0; i<sockNum; ++i){
        vTh.emplace_back(receiveRoutine, i);
    }

    std::this_thread::sleep_for(ms(3000));

    std::cout<<"\033[2J\033[0;0H";

    auto poller = HddlUnite::Poller::create("client");
    auto conn = HddlUnite::Connection::create(poller);
    while(!conn->connect("/tmp/gstreamer_ipc_second.sock")){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::string toSend = "";
    for(unsigned i =0; i<sockNum; ++i){
        toSend = toSend + g_recv_socket + std::to_string(i);
        if(i!=sockNum-1){
            toSend = toSend+",";
        }
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

    for(auto& th: vTh){
        th.join();
    }
    return 0;
}
