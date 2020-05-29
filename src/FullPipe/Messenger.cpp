#include <Messenger.hpp>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <deque>

class Messenger::Impl{
public:
    Impl();

    ~Impl();

    Impl(const Impl& ) = delete;

    Impl& operator=(const Impl& ) = delete;

    MessageListener& spawn();

    bool setMessageAndNotify(ControlMessage msg);

private:
    std::vector<std::shared_ptr<MessageListener>> m_vListeners;
};

Messenger::Impl::Impl(){

}

Messenger::Impl::~Impl(){

}

MessageListener& Messenger::Impl::spawn(){
    auto listener = std::make_shared<MessageListener>();
    m_vListeners.push_back(listener);
    return *listener;
}

bool Messenger::Impl::setMessageAndNotify(ControlMessage msg){
    bool ret = true;
    for(auto item: m_vListeners){
        ret &= item->push(msg);
    }
    return ret;
}

Messenger::Messenger():m_impl(new Impl()){

}

Messenger::~Messenger(){

}

MessageListener& Messenger::spawn(){
    return m_impl->spawn();
}

bool Messenger::setMessageAndNotify(ControlMessage msg){
    return m_impl->setMessageAndNotify(msg);
}

class MessageListener::Impl{
public:
    Impl();

    ~Impl();

    Impl(const Impl& ) = delete;

    Impl& operator=(const Impl& ) = delete;

    bool push(ControlMessage msg);

    bool pop(ControlMessage* msg);

    bool tryPop(ControlMessage* msg);

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<ControlMessage> m_deque;
    const unsigned m_maxDepth = 8;
};

MessageListener::Impl::Impl(){

}

MessageListener::Impl::~Impl(){

}

bool MessageListener::Impl::push(ControlMessage msg){
    {
        std::lock_guard<std::mutex> lg(m_mutex);
        if(m_deque.size() > m_maxDepth){
            return false;
        }
        else{
            m_deque.push_back(msg);
        }
    }
    m_cv.notify_all();
}

bool MessageListener::Impl::pop(ControlMessage* msg){
    std::unique_lock<std::mutex> lk(m_mutex);
    if(m_deque.size() > 0){
        *msg = m_deque.front();
        m_deque.pop_front();
    }
    else{
        m_cv.wait(lk, [&](){ return m_deque.size() > 0; });
        *msg = m_deque.front();
        m_deque.pop_front();
    }
    return true;
}

bool MessageListener::Impl::tryPop(ControlMessage* msg){
    std::lock_guard<std::mutex> lg(m_mutex);
    if(m_deque.size() > 0){
        *msg = m_deque.front();
        m_deque.pop_front();
        return true;
    }
    else{
        return false;
    }
}

MessageListener::MessageListener():m_impl(new Impl()){

}

MessageListener::~MessageListener(){

}

bool MessageListener::push(ControlMessage msg){
    return m_impl->push(msg);
}

bool MessageListener::pop(ControlMessage* msg){
    return m_impl->pop(msg);
}

bool MessageListener::tryPop(ControlMessage* msg){
    return m_impl->tryPop(msg);
}

