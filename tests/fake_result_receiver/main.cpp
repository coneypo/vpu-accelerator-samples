#include "HLog.h"
#include <IPC.h>
#include <atomic>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>
using namespace HddlUnite;

static std::mutex mutex_total_results;
static std::map<u_int64_t, std::string> total_results;
static std::atomic<bool> data_ready(false);

int receiveRoutine(const char* socket_address)
{
    auto poller = Poller::create();
    auto connection = Connection::create(poller);
    if (!connection->listen(socket_address)) {
        HError("Error: Create service listening socket failed.");
        return -1;
    }

    while (true) {
        auto event = poller->waitEvent(100);
        switch (event.type) {
        case Event::Type::CONNECTION_IN:
            HInfo("connection in");
            connection->accept();
            break;
        case Event::Type::MESSAGE_IN: {

            int length = 0;
            auto& data_connection = event.connection;
            AutoMutex autoLock(data_connection->getMutex());

            if (!data_connection->read(&length, sizeof(length))) {
                HError("Error: receive message length failed");
                break;
            }

            if (length <= 0) {
                HError("Error: invalid message length, length=%lu", length);
                break;
            }

            std::string serialized(static_cast<size_t>(length), ' ');
            if (!data_connection->read(&serialized[0], length)) {
                HError("Error: receive message failed, expectLen=%lu ", length);
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
                HError("Error: data format doesn't match.");
                break;
            }

            //save data
            mutex_total_results.lock();
            total_results.insert(std::make_pair(std::stoul(fields[5]), serialized));
            mutex_total_results.unlock();
            break;
        }
        case Event::Type::CONNECTION_OUT:
            HInfo("connection out");
            mutex_total_results.lock();
            // it will continously display the result if don't clear
            total_results.clear();
            mutex_total_results.unlock();
            // if you want to stop display when disconnecting.
            break;

        default:
            break;
        }
    }
}

int main(int argc, char* argv[])
{
    std::string unix_domain_socket;
    if (argc < 2) {
        unix_domain_socket = "/var/tmp/gstreamer_ipc.sock";

    } else {
        unix_domain_socket = argv[1];
    }

    std::thread t(receiveRoutine, unix_domain_socket.c_str());
    t.detach();
    while (true) {
        mutex_total_results.lock();
        for (auto&& p : total_results) {
            std::cout << p.second << std::endl;
        }
        mutex_total_results.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return 0;
}
