/* *
 * Copyright (C) 2019 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef HDDLUNITE_IPC_H
#define HDDLUNITE_IPC_H

#include <memory>
#include <mutex>
#include <string>
#include <tuple>

#define IPCHandle int

namespace HddlUnite {
class Poller;

class Connection : public std::enable_shared_from_this<Connection> {
public:
    using Ptr = std::shared_ptr<Connection>;
    using WPtr = std::weak_ptr<Connection>;

    enum class State : uint8_t {
        NONE = 0,
        LISTENING = 1,
        CONNECTED = 2,
        ERROR = 3
    };

    explicit Connection(std::weak_ptr<Poller> poller);
    virtual ~Connection();

    State getState() const;
    uint64_t getId() const;
    IPCHandle getHandle() const;

    bool listen(const std::string& serverName, int numListen = 5);
    bool connect(const std::string& serverName);
    Connection::Ptr accept();

    std::mutex& getMutex() const noexcept;

    int read(void* buffer, int bufferSize) const;
    int write(const void* buffer, int bufferSize) const;

    bool read(IPCHandle& handle) const;
    bool write(IPCHandle handle) const;

    static Ptr create(const std::weak_ptr<Poller>& poller);
    static void setAccessAttributes(std::tuple<std::string, std::string, int> accessAttrs);

private:
    friend std::enable_shared_from_this<Connection>;

private:
    class Impl;
    bool m_isInPoller;
    mutable std::mutex m_mutex;
    std::unique_ptr<Impl> m_impl;
    std::weak_ptr<Poller> m_poller;
};

class Event {
public:
    enum class Type : uint8_t {
        NONE = 0,
        CONNECTION_IN = 1,
        CONNECTION_OUT = 2,
        MESSAGE_IN = 3
    };

    Connection::Ptr connection;
    Type type;

    static std::string getType(Event& event)
    {
        switch (event.type) {
        case Type::NONE:
            return "NONE";
        case Type::CONNECTION_IN:
            return "CONNECTION_IN";
        case Type::CONNECTION_OUT:
            return "CONNECTION_OUT";
        case Type::MESSAGE_IN:
            return "MESSAGE_IN";
        default:
            return "INVALID";
        }
    }
};

class Poller {
public:
    using Ptr = std::shared_ptr<Poller>;

    explicit Poller(std::string& name);
    virtual ~Poller();

    static Ptr create(std::string name = {});

    bool isOK() const;
    size_t getTotalConnections() const;
    Event waitEvent(long milliseconds) const;
    bool addConnection(Connection::Ptr spConnection) const;
    void removeConnection(uint64_t connectionId) const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
}

#endif //HDDLUNITE_IPC_H
