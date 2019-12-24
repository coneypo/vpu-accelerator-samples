//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include <atomic>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <unordered_map>

#include "utils/FileUtils.h"
#include "utils/HLog.h"
#include "utils/IPC.h"
#include "utils/Mutex.h"
#include "utils/ScopeGuard.h"
#include "utils/StringUtils.h"

namespace HddlUnite {
class Connection::Impl : public std::enable_shared_from_this<Connection::Impl> {
public:
    Impl();
    virtual ~Impl();

    State getState() const;
    uint64_t getId() const;
    IPCHandle getHandle() const;

    bool listen(const std::string& serverName, int numListen = 5);
    bool connect(const std::string& serverName);
    IPCHandle accept();

    int read(void* buffer, int bufferSize);
    int write(const void* buffer, int bufferSize);

    bool read(IPCHandle& handle);
    bool write(IPCHandle handle) const;

    static void setAccessAttributes(std::tuple<std::string, std::string, int>& accessAttrs);

private:
    bool makeBlock();
    void setFd(int socketFd);
    void setState(Connection::State state);

    friend Connection;
    friend std::enable_shared_from_this<Connection::Impl>;

private:
    int m_fd;
    uint64_t m_id;
    std::string m_serverName;
    Connection::State m_state;

    static std::atomic<uint64_t> connectionCounter;
    static std::string m_group;
    static std::string m_user;
    static uint64_t m_mode;
};

class Poller::Impl {
public:
    explicit Impl(std::string& name);
    ~Impl();

    bool isOK();
    size_t getTotalConnections();
    Event waitEvent(int milliseconds);
    bool addConnection(Connection::Ptr spConnection);
    void removeConnection(uint64_t connectionId);

private:
    static void blockPipeSignal();
    void insertConnection(int socketFd, Connection::Ptr& spConnection);
    Connection::Ptr getConnectionBySocket(int socketFd);
    Connection::Ptr getConnectionById(uint64_t connectionId);
    void eraseConnectionById(uint64_t connectionId);

private:
    Mutex m_mutex;
    int m_epollFd;
    std::string m_name;
    std::unordered_map<int, Connection::Ptr> m_connections;

    static const int m_epollSize = 1000;
};

std::atomic<uint64_t> Connection::Impl::connectionCounter(0);
std::string Connection::Impl::m_group("users");
std::string Connection::Impl::m_user;
uint64_t Connection::Impl::m_mode(0660);

Connection::Impl::Impl()
    : m_fd(-1)
    , m_id(++connectionCounter)
    , m_state(Connection::State::NONE)
{
}

Connection::Impl::~Impl()
{
    HDebug("[Connection##%lu] to destruct connection, socketFd=%d", m_id, m_fd);
    if (m_fd > 0) {
        close(m_fd);
    }
    if (!m_serverName.empty()) {
        unlink(m_serverName.c_str());
    }
    HDebug("[Connection##%lu] destruct connection done", m_id);
}

Connection::State Connection::Impl::getState() const
{
    return m_state;
}

uint64_t Connection::Impl::getId() const
{
    return m_id;
}

IPCHandle Connection::Impl::getHandle() const
{
    return m_fd;
}

bool Connection::Impl::listen(const std::string& serverName, int numListen)
{
    if (serverName.empty()) {
        HError("Error: serverName should not be empty");
        return false;
    }

    if (numListen <= 0) {
        HError("Error: numListen should be greater than zero");
        return false;
    }

    m_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_fd < 0) {
        HError("Error: create unix domain socket failed, errorCode=%s", strerror(errno));
        return false;
    }

    HDebug("[Connection##%lu] create socket(%d) done", m_id, m_fd);

    auto scopeGuard = makeScopeGuard([&] {
        HDebug("[Connection##%lu] close socket(%d), unlink socket file(%s)", m_id, m_fd, serverName);
        close(m_fd);
        unlink(serverName.c_str());
        m_fd = -1;
        m_state = Connection::State::ERROR;
    });

    unlink(serverName.c_str());

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    size_t copyBytes = StringUtils::copyStringSafe(addr.sun_path, serverName);
    socklen_t addrlen = offsetof(sockaddr_un, sun_path) + copyBytes + 1;
    if (bind(m_fd, (struct sockaddr*)&addr, addrlen) != 0) {
        HError("Error: bind socketFd(%d) failed, errorCode=%s", m_fd, strerror(errno));
        return false;
    }

    HDebug("[Connection##%lu] bind socket(%d) on socket file(%s) done", m_id, m_fd, serverName);

    if (!FileUtils::updateAccessAttribute(serverName, m_group, m_user, m_mode)) {
        HError("Error: update access attributes failed, serverName=%s", serverName);
        return false;
    }

    HDebug("[Connection##%lu] update attribute of socket file(%s) done", m_id, serverName);

    if (::listen(m_fd, numListen) != 0) {
        HError("Error: listen on socketFd(%d) failed, errorCode=%s", m_fd, strerror(errno));
        return false;
    }

    HDebug("[Connection##%lu] listen on socket(%d) done", m_id, m_fd);

    m_state = Connection::State::LISTENING;
    m_serverName = serverName;
    scopeGuard.dismiss();

    return true;
}

bool Connection::Impl::connect(const std::string& serverName)
{
    if (serverName.empty()) {
        HError("Error: serverName should not be empty");
        return false;
    }

    m_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_fd < 0) {
        HError("Error: create unix domain socket failed, errorCode=%s", strerror(errno));
        return false;
    }

    HDebug("[Connection##%lu] create socket(%d) done", m_id, m_fd);

    auto scopeGuard = makeScopeGuard([&] {
        HDebug("[Connection##%lu] close socket(%d)", m_id, m_fd);
        close(m_fd);
        m_fd = -1;
    });

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    size_t copyBytes = StringUtils::copyStringSafe(addr.sun_path, serverName);
    socklen_t addrLen = offsetof(struct sockaddr_un, sun_path) + copyBytes + 1;

    if (::connect(m_fd, (struct sockaddr*)&addr, addrLen) != 0) {
        HError("Error: connect to server failed, connectFd=%d serverName=%s errorCode=%s", m_fd, serverName, strerror(errno));
        return false;
    }

    HDebug("[Connection##%lu] connect socket(%d) to socket file(%s) done", m_id, m_fd, serverName);

    m_state = Connection::State::CONNECTED;
    scopeGuard.dismiss();

    return true;
}

IPCHandle Connection::Impl::accept()
{
    if (m_state != Connection::State::LISTENING) {
        HError("Error: connection is not in listening state");
        return {};
    }

    struct sockaddr_un addr = {};
    socklen_t addrLen = sizeof(addr);

    int connectFd = ::accept(m_fd, (struct sockaddr*)&addr, &addrLen);
    if (connectFd <= 0) {
        HError("Error: accept connection failed, listenFd=%d errorCode=%s", m_fd, strerror(errno));
        return {};
    }

    HDebug("[Connection##%lu] accept new socket(%d) through listen socket(%d)", m_id, connectFd, m_fd);

    return connectFd;
}

int Connection::Impl::read(void* buffer, const int bufferSize)
{
    if (m_fd < 0) {
        HError("Error: invalid connection, m_fd=%d", m_fd);
        return 0;
    }

    if (!buffer || bufferSize < 0) {
        HError("Error: invalid input parameters, buffer=%p bufferSize=%d", buffer, bufferSize);
        return 0;
    }

    if (!makeBlock()) {
        HError("Error: make fd block failed");
        return 0;
    }

    HDebug("[Connection##%lu] make data socket(%d) blocked", m_id, m_fd);

    char* data = static_cast<char*>(buffer);

    size_t leftBytes = bufferSize;
    while (leftBytes > 0) {
        ssize_t readBytes = ::read(m_fd, data, leftBytes);
        HDebug("[Connection##%lu] read %d bytes through data socket(%d)", m_id, readBytes, m_fd);
        if (readBytes == 0) { // reach EOF
            break;
        } else if (readBytes < 0) {
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                HError("Error: read socket(%d) failed, errorCode=%s", m_fd, strerror(errno));
                break;
            } else {
                HInfo("error(%s) occured when reading socket(%d), ignore it", strerror(errno), m_fd);
                readBytes = 0;
            }
        }

        data += readBytes;
        leftBytes -= readBytes;
    }

    HDebug("[Connection##%lu] read done, socket=%d expect=%d actual=%d", m_id, m_fd, bufferSize, bufferSize - leftBytes);

    return bufferSize - leftBytes;
}

bool Connection::Impl::read(IPCHandle& handle)
{
    if (m_fd < 0) {
        HError("Error: invalid connection, m_fd=%d", m_fd);
        return false;
    }

    if (!makeBlock()) {
        HError("Error: make fd block failed");
        return false;
    }

    HDebug("[Connection##%lu] make data socket(%d) blocked", m_id, m_fd);

    int num_fd = -1;
    struct iovec iov[1];
    iov[0].iov_base = &num_fd;
    iov[0].iov_len = sizeof(int);

    char cmsgBuf[CMSG_LEN(sizeof(int))] = {};
    struct cmsghdr* cmsg;
    cmsg = (struct cmsghdr*)cmsgBuf;

    struct msghdr msg = {};
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    msg.msg_name = nullptr;
    msg.msg_namelen = 0;
    msg.msg_control = cmsg;
    msg.msg_controllen = CMSG_LEN(sizeof(int));

    ssize_t ret = -1;
    while (ret <= 0) {
        ret = ::recvmsg(m_fd, &msg, 0);

        if (!ret) {
            HError("The peer has performed an orderly shutdown. socket=(%d)", m_fd);
            return false;
        }

        if (ret < 0) {
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                HError("Error: receive fd through socket (%d) failed", m_fd);
                return false;
            }
        }
    }

    int rfd = -1;
    if (msg.msg_controllen == CMSG_LEN(sizeof(int))) {
        rfd = *(int*)CMSG_DATA(cmsg);
    }

    if (rfd < 0) {
        HError("Error: could not get recv_fd from socket");
        return false;
    }

    handle = rfd;
    return true;
}

int Connection::Impl::write(const void* buffer, const int bufferSize)
{
    if (m_fd < 0) {
        HError("Error: invalid connection, m_fd=%d", m_fd);
        return 0;
    }

    if (!buffer || bufferSize < 0) {
        HError("Error: invalid input parameters, buffer=%p bufferSize=%d", buffer, bufferSize);
        return 0;
    }

    const char* data = static_cast<const char*>(buffer);

    size_t leftBytes = bufferSize;
    while (leftBytes > 0) {
        ssize_t writeBytes = ::write(m_fd, data, leftBytes);
        HDebug("[Connection##%lu] write %d bytes through data socket(%d)", m_id, writeBytes, m_fd);
        if (writeBytes < 0) {
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                HError("Error: write socket(%d) failed, errorCode=%s", m_fd, strerror(errno));
                break;
            } else {
                HInfo("error(%s) occured when writing socket(%d), ignore it", strerror(errno), m_fd);
                writeBytes = 0;
            }
        }

        data += writeBytes;
        leftBytes -= writeBytes;
    }

    HDebug("[Connection##%lu] write done, socket=%d expect=%d actual=%d", m_id, m_fd, bufferSize, bufferSize - leftBytes);

    return bufferSize - leftBytes;
}

bool Connection::Impl::write(const IPCHandle handle) const
{
    if (m_fd < 0) {
        HError("Error: invalid connection, m_fd=%d", m_fd);
        return false;
    }

    int fdNum = 1;

    struct iovec iov[1];
    iov[0].iov_base = &fdNum;
    iov[0].iov_len = sizeof(fdNum);

    char cmsgBuf[CMSG_LEN(sizeof(int))];
    struct cmsghdr* cmsg;

    cmsg = reinterpret_cast<struct cmsghdr*>(cmsgBuf);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *(int*)CMSG_DATA(cmsg) = handle;

    struct msghdr msg = {};
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_name = nullptr;
    msg.msg_namelen = 0;
    msg.msg_control = cmsg;
    msg.msg_controllen = CMSG_LEN(sizeof(int));

    ssize_t ret = -1;
    while (ret <= 0) {
        ret = ::sendmsg(m_fd, &msg, 0);
        if (ret < 0) {
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                HError("Error: send fd(%d) through socket(%d) failed.\n", handle, m_fd);
                return false;
            }
        }
    }

    return true;
}

bool Connection::Impl::makeBlock()
{
    int flag = fcntl(m_fd, F_GETFL, 0) & (~O_NONBLOCK);
    int status = fcntl(m_fd, F_SETFL, flag);
    if (status < 0) {
        HError("Error: make socketFd(%d) blocked failed, errorCode=%s", m_fd, strerror(errno));
        return false;
    }

    return true;
}

void Connection::Impl::setFd(int socketFd)
{
    m_fd = socketFd;
}

void Connection::Impl::setState(Connection::State state)
{
    m_state = state;
}

void Connection::Impl::setAccessAttributes(std::tuple<std::string, std::string, int>& accessAttrs)
{
    m_group = std::get<0>(accessAttrs);
    m_user = std::get<1>(accessAttrs);
    m_mode = std::get<2>(accessAttrs);
}

Poller::Impl::Impl(std::string& name)
    : m_name(std::move(name))
{
    m_epollFd = epoll_create(m_epollSize);
}

Poller::Impl::~Impl()
{
    close(m_epollFd);
}

bool Poller::Impl::isOK()
{
    return m_epollFd >= 0;
}

size_t Poller::Impl::getTotalConnections()
{
    AutoMutex autoLock(m_mutex);
    return static_cast<size_t>(m_connections.size());
}

bool Poller::Impl::addConnection(Connection::Ptr spConnection)
{
    if (!spConnection) {
        HError("Error: invalid connection");
        return false;
    }

    int socketFd = spConnection->getHandle();
    if (socketFd < 0) {
        HError("Error: invalid connection socket, socketFd=%d", socketFd);
        return false;
    }

    if (getConnectionBySocket(socketFd)) {
        HError("Error: connection with same socketFd(%d) already exists", socketFd);
        return false;
    }

    // under linux, just add the fd into epoll
    // we choose level-trigger mode, so blocking socket is enough.
    //
    // if we use edge-trigger mode, then we need to drain all available data in cache
    // using non-blocking socket on each epoll-event, and this can bring some difficulty
    // to application parser implementation

    struct epoll_event event = {};
    event.events = EPOLLIN | EPOLLRDHUP;
    event.data.fd = socketFd;

    auto ret = epoll_ctl(m_epollFd, EPOLL_CTL_ADD, socketFd, &event);
    if (ret) {
        HError("Error: epoll_ctl(EPOLL_CTL_ADD) failed, m_epollFd=%d socketFd=%d errorCode=%s", m_epollFd, socketFd, strerror(errno));
        return false;
    }

    insertConnection(socketFd, spConnection);

    HDebug("[Poller##%s] add connection(%lu) done, numElem=%lu", m_name, spConnection->getId(), m_connections.size());

    return true;
}

void Poller::Impl::removeConnection(uint64_t connectionId)
{
    auto spConnection = getConnectionById(connectionId);
    if (!spConnection) {
        HError("Error: cannot find connection with id=%lu", connectionId);
        return;
    }

    int socketFd = spConnection->getHandle();
    epoll_ctl(m_epollFd, EPOLL_CTL_DEL, socketFd, nullptr);

    eraseConnectionById(connectionId);

    HDebug("[Poller##%s] remove connection(id:%lu) done, numElem=%lu", m_name, spConnection->getId(), m_connections.size());
}

Event Poller::Impl::waitEvent(int milliseconds)
{
    blockPipeSignal();

    struct epoll_event event = {};

    int numEvents = epoll_wait(m_epollFd, &event, 1, milliseconds);

    if (numEvents < 0) {
        HError("Error: epoll_wait(EPOLL_CTL_ADD) failed, errorCode=%s", strerror(errno));
        return { nullptr, Event::Type::NONE };
    }

    if (!numEvents) {
        return { nullptr, Event::Type::NONE };
    }

    int socketFd = event.data.fd;
    auto connection = getConnectionBySocket(socketFd);

    if (!connection) {
        HError("Error: cannot find connection with socketFd=%d", socketFd);
        return { nullptr, Event::Type::NONE };
    }

    if (event.events & EPOLLIN) {
        auto state = connection->getState();
        switch (state) {
        case Connection::State::LISTENING:
            HDebug("[Poller##%s] event(CONNECTION_IN) occured on listening connection(%lu)", m_name, connection->getId());
            return { connection, Event::Type::CONNECTION_IN };

        case Connection::State::CONNECTED:
            // EPOLLIN and EPOLLHUP may arrive simultaneously
            if (event.events & EPOLLRDHUP || event.events & EPOLLHUP) {
                HDebug("[Poller##%s] event(CONNECTION_OUT) occured on data connection(%lu)", m_name, connection->getId());
                removeConnection(connection->getId());
                return { connection, Event::Type::CONNECTION_OUT };
            } else {
                HDebug("[Poller##%s] event(MESSAGE_IN) occured on data connection(%lu)", m_name, connection->getId());
                return { connection, Event::Type::MESSAGE_IN };
            }

        default:
            HError("Error: abnormal connection(%lu) with EPOLLIN, state=%d", connection->getId(), static_cast<int>(state));
            return { nullptr, Event::Type::NONE };
        }
    }

    return { nullptr, Event::Type::NONE };
}

void Poller::Impl::blockPipeSignal()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, []() {
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGPIPE);
        pthread_sigmask(SIG_BLOCK, &set, nullptr);
    });
}

void Poller::Impl::insertConnection(int socketFd, Connection::Ptr& spConnection)
{
    AutoMutex autoLock(m_mutex);
    m_connections.insert(std::make_pair(socketFd, spConnection));
}

Connection::Ptr Poller::Impl::getConnectionBySocket(int socketFd)
{
    AutoMutex autoLock(m_mutex);

    if (m_connections.count(socketFd)) {
        return m_connections[socketFd];
    }

    return {};
}

Connection::Ptr Poller::Impl::getConnectionById(uint64_t connectionId)
{
    AutoMutex autoLock(m_mutex);

    for (const auto& entry : m_connections) {
        const auto& connection = entry.second;
        if (connection->getId() == connectionId) {
            return connection;
        }
    }

    return {};
}

void Poller::Impl::eraseConnectionById(uint64_t connectionId)
{
    AutoMutex autoLock(m_mutex);

    for (const auto& entry : m_connections) {
        const auto& connection = entry.second;
        if (connection->getId() == connectionId) {
            m_connections.erase(entry.first);
            break;
        }
    }
}

Connection::Connection(std::weak_ptr<Poller> poller)
    : m_isInPoller(false)
    , m_impl(new Connection::Impl())
    , m_poller(std::move(poller))
{
}

Connection::~Connection()
{
    if (m_isInPoller) {
        auto spPoller = m_poller.lock();
        if (spPoller) {
            spPoller->removeConnection(getId());
        }
    }

    m_impl.reset();
}

uint64_t Connection::getId() const
{
    return m_impl->getId();
}

IPCHandle Connection::getHandle() const
{
    return m_impl->getHandle();
}

Connection::State Connection::getState() const
{
    return m_impl->getState();
}

bool Connection::listen(const std::string& serverName, int numListen)
{
    if (!m_impl->listen(serverName, numListen)) {
        return false;
    }

    auto spPoller = m_poller.lock();
    if (spPoller) {
        m_isInPoller = spPoller->addConnection(shared_from_this());
        if (!m_isInPoller) {
            HError("Error: add connection to poller failed");
            return false;
        }
    }

    return true;
}

bool Connection::connect(const std::string& serverName)
{
    if (!m_impl->connect(serverName)) {
        return false;
    }

    auto spPoller = m_poller.lock();
    if (spPoller) {
        m_isInPoller = spPoller->addConnection(shared_from_this());
        if (!m_isInPoller) {
            HError("Error: add connection to poller failed");
            return false;
        }
    }

    return true;
}

Connection::Ptr Connection::accept()
{
    auto connectFd = m_impl->accept();
    if (connectFd < 0) {
        return {};
    }

    Connection::Ptr spConnection;

    auto scopeGuard = makeScopeGuard([&] {
        if (!spConnection) {
            close(connectFd);
        } else {
            HDebug("connectFd(%d) will be closed in connection(%lu)", spConnection->getId());
        }
    });

    spConnection = Connection::create(m_poller);
    if (!spConnection) {
        HError("Error: create Connection::Impl instance failed");
        return {};
    }

    spConnection->m_impl->setFd(connectFd);
    spConnection->m_impl->setState(Connection::State::CONNECTED);

    HDebug("[Connection##%lu] new connection created with data socket(%d)", spConnection->getId(), connectFd);

    auto spPoller = m_poller.lock();
    if (spPoller) {
        if (!spPoller->addConnection(spConnection)) {
            HError("Error: add connection(%lu) to poller failed", spConnection->getId());
            return {};
        }
    }

    scopeGuard.dismiss();

    return spConnection;
}

Mutex& Connection::getMutex() const noexcept
{
    return m_mutex;
}

int Connection::read(void* buffer, const int bufferSize) const
{
    return m_impl->read(buffer, bufferSize);
}

int Connection::write(const void* buffer, const int bufferSize) const
{
    return m_impl->write(buffer, bufferSize);
}

bool Connection::read(IPCHandle& handle) const
{
    return m_impl->read(handle);
}

bool Connection::write(const IPCHandle handle) const
{
    return m_impl->write(handle);
}

Connection::Ptr Connection::create(const std::weak_ptr<Poller>& poller)
{
    return std::make_shared<Connection>(poller);
}

void Connection::setAccessAttributes(std::tuple<std::string, std::string, int> accessAttrs)
{
    Connection::Impl::setAccessAttributes(accessAttrs);
}

Poller::Poller(std::string& name)
    : m_impl(new Poller::Impl(name))
{
}

Poller::~Poller()
{
    m_impl.reset();
}

bool Poller::isOK() const
{
    return m_impl->isOK();
}

size_t Poller::getTotalConnections() const
{
    return m_impl->getTotalConnections();
}

Event Poller::waitEvent(long milliseconds) const
{
    return m_impl->waitEvent(milliseconds);
}

bool Poller::addConnection(Connection::Ptr spConnection) const
{
    return m_impl->addConnection(std::move(spConnection));
}

void Poller::removeConnection(uint64_t connectionId) const
{
    return m_impl->removeConnection(connectionId);
}

Poller::Ptr Poller::create(std::string name)
{
    return std::make_shared<Poller>(name);
}
}
