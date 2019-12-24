//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <utility>

#include "utils/FileMonitor.h"
#include "utils/HLog.h"
#include "utils/Mutex.h"

namespace HddlUnite {
FileMonitor::Event translatetoEvent(const uint32_t mask)
{
    switch (mask) {
    case IN_ACCESS:
        return FileMonitor::Event::Access;
    case IN_OPEN:
        return FileMonitor::Event::Open;
    case IN_MODIFY:
        return FileMonitor::Event::Modify;
    case IN_CLOSE_WRITE:
        return FileMonitor::Event::CloseWrite;
    case IN_CLOSE_NOWRITE:
        return FileMonitor::Event::CloseNoWrite;
    case IN_DELETE_SELF:
        return FileMonitor::Event::DeleteSelf;
    default:
        return FileMonitor::Event::Empty;
    }
}

uint32_t translatetoMask(const FileMonitor::Event event)
{
    switch (event) {
    case FileMonitor::Event::Access:
        return IN_ACCESS;
    case FileMonitor::Event::Open:
        return IN_OPEN;
    case FileMonitor::Event::Modify:
        return IN_MODIFY;
    case FileMonitor::Event::CloseNoWrite:
        return IN_CLOSE_NOWRITE;
    case FileMonitor::Event::CloseWrite:
        return IN_CLOSE_WRITE;
    case FileMonitor::Event::DeleteSelf:
        return IN_DELETE_SELF;
    case FileMonitor::Event::All:
        return IN_ALL_EVENTS;
    default:
        return 0;
    }
}

class FileNode : public NonCopyable {
public:
    using Ptr = std::shared_ptr<FileNode>;
    using Callback = FileMonitor::Callback;

    FileNode(int watchfd, std::string filePath, Callback callback);

    int getwatchfd() const noexcept;
    void setWatchfd(int watchfd) noexcept;
    const std::string& getfilePath() const noexcept;
    void invokeCallback(FileMonitor::Event noticeEvent);

private:
    int m_watchfd { -1 };
    std::string m_filePath;
    Callback m_callback;
};

FileNode::FileNode(const int watchfd, std::string filePath, Callback callback)
    : m_watchfd(watchfd)
    , m_filePath(std::move(filePath))
    , m_callback(std::move(callback))
{
}

int FileNode::getwatchfd() const noexcept
{
    return m_watchfd;
}

void FileNode::setWatchfd(int watchfd) noexcept
{
    m_watchfd = watchfd;
}

const std::string& FileNode::getfilePath() const noexcept
{
    return m_filePath;
}

void FileNode::invokeCallback(const FileMonitor::Event noticeEvent)
{
    m_callback(m_filePath, noticeEvent);
}

FileNode::Ptr makeNoticedFile(const int watchfd, const std::string& filePath, const FileMonitor::Callback& callback) noexcept
{
    try {
        return std::make_shared<FileNode>(watchfd, filePath, callback);
    } catch (...) {
        return {};
    }
}

class FileMonitor::Impl {
public:
    Impl()
    {
        m_epollfd = epoll_create(10);
        m_FileNoticefd = inotify_init();
        struct epoll_event eventItem = {};
        eventItem.events = EPOLLIN;
        eventItem.data.fd = m_FileNoticefd;
        epoll_ctl(m_epollfd, EPOLL_CTL_ADD, m_FileNoticefd, &eventItem);
    }

    ~Impl()
    {
        m_stop = true;
        if (m_monitor.joinable()) {
            m_monitor.join();
        }
        for (auto& file : m_files) {
            inotify_rm_watch(m_FileNoticefd, file->getwatchfd());
        }
        close(m_FileNoticefd);
        close(m_epollfd);
    }

    bool addWatch(const std::string& filePath, const FileMonitor::Event event, const Callback& callback)
    {
        if (!FileUtils::exist(filePath) || !callback) {
            return false;
        }

        auto mask = translatetoMask(event) | IN_DELETE_SELF;
        auto watchfd = inotify_add_watch(m_FileNoticefd, filePath.c_str(), mask);
        if (watchfd < 0) {
            HError("Error: inotify_add_watch failed, error=%s", strerror(errno));
            return false;
        }

        auto file = makeNoticedFile(watchfd, filePath, callback);
        if (!file) {
            inotify_rm_watch(m_FileNoticefd, watchfd);
            return false;
        }

        AutoMutex autoLock(m_mutex);
        m_files.push_back(std::move(file));
        if (!m_monitor.joinable()) {
            m_monitor = std::thread(&Impl::monitorFilesRoutine, this);
        }

        return true;
    }

    void deleteWatch(const std::string& filePath) noexcept
    {
        AutoMutex autoLock(m_mutex);

        auto watchfd = popNoticeFile(filePath);
        if (watchfd != -1) {
            inotify_rm_watch(m_FileNoticefd, watchfd);
        }
    }

    void monitorFilesRoutine()
    {
        while (!m_stop) {
            struct epoll_event epollEvent = {};
            int numEvents = epoll_wait(m_epollfd, &epollEvent, 1, 50000);

            if (numEvents < 0) {
                m_stop = false;
                return;
            }

            if (!numEvents || epollEvent.data.fd != m_FileNoticefd) {
                continue;
            }

            char inotifyBuf[s_bufferSize];
            auto numRead = read(m_FileNoticefd, inotifyBuf, s_bufferSize);

            if (numRead == -1) {
                HError("Error: inotify read error");
                m_stop = false;
                return;
            }

            invokeInotifyCallback(inotifyBuf, numRead);
        }
    }

private:
    FileNode::Ptr retrieveNoticeFile(int watchfd)
    {
        AutoMutex autoLock(m_mutex);
        auto it = std::find_if(m_files.begin(), m_files.end(), [watchfd](const FileNode::Ptr& obj) {
            return obj->getwatchfd() == watchfd;
        });

        if (it != m_files.end()) {
            return *it;
        }

        return {};
    }

    int popNoticeFile(const std::string& filePath)
    {
        AutoMutex autoLock(m_mutex);

        auto it = std::find_if(m_files.begin(), m_files.end(), [&filePath](const FileNode::Ptr& obj) {
            return obj->getfilePath() == filePath;
        });

        if (it != m_files.end()) {
            auto watchfd = (*it)->getwatchfd();
            m_files.erase(it);
            return watchfd;
        }

        return -1;
    }

    void resetWatch(const FileNode::Ptr& noticefile, const FileMonitor::Event event)
    {
        inotify_rm_watch(m_FileNoticefd, noticefile->getwatchfd());
        auto mask = translatetoMask(event) | IN_DELETE_SELF;
        auto watchfd = inotify_add_watch(m_FileNoticefd, noticefile->getfilePath().c_str(), mask);
        noticefile->setWatchfd(watchfd);
    }

    void invokeInotifyCallback(char* buf, ssize_t count)
    {
        char* bufPtr = buf;
        while (bufPtr < buf + count) {
            auto& inotifyEvent = *(struct inotify_event*)bufPtr;
            auto watchfd = inotifyEvent.wd;
            auto event = translatetoEvent(inotifyEvent.mask);
            auto noticefile = retrieveNoticeFile(watchfd);
            if (noticefile) {
                switch (event) {
                case FileMonitor::Event::Modify: {
                    noticefile->invokeCallback(FileMonitor::Event::Modify);
                    break;
                }
                case FileMonitor::Event::DeleteSelf: {
                    noticefile->invokeCallback(FileMonitor::Event::Modify);
                    resetWatch(noticefile, event);
                    break;
                default:
                    break;
                }
                }
            }
            bufPtr += sizeof(struct inotify_event) + inotifyEvent.len;
        }
    }

private:
    Mutex m_mutex;
    bool m_stop { false };
    int m_epollfd { -1 };
    int m_FileNoticefd { -1 };
    std::thread m_monitor;
    std::vector<FileNode::Ptr> m_files;
    constexpr static size_t s_bufferSize { 1000 };
};

FileMonitor::FileMonitor()
    : m_impl(new Impl())
{
}

FileMonitor::~FileMonitor()
{
}

bool FileMonitor::addWatch(const std::string& filePath, const FileMonitor::Event event, const Callback& callback) noexcept
{
    return m_impl->addWatch(filePath, event, callback);
}

void FileMonitor::deleteWatch(const std::string& filePath) noexcept
{
    m_impl->deleteWatch(filePath);
}
}
