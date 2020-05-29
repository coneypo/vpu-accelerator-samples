#ifndef MESSENGER_HPP
#define MESSENGER_HPP

#include <memory>

class MessageListener; 

enum ControlMessage{
    EMPTY = 0,
    ADDR_RECVED,
    STOP_RECVED
};

class Messenger{
public:
    Messenger();

    Messenger(const Messenger& ) = delete;

    Messenger& operator=(const Messenger& ) = delete;

    ~Messenger();

    MessageListener& spawn();

    bool setMessageAndNotify(ControlMessage msg);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

class MessageListener{
public:
    MessageListener();

    MessageListener(const MessageListener& ) = delete;

    MessageListener& operator=(const MessageListener& ) = delete;

    ~MessageListener();

    bool push(ControlMessage msg);

    bool pop(ControlMessage* msg);

    bool tryPop(ControlMessage* msg);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};


#endif //#ifndef MESSENGER_HPP