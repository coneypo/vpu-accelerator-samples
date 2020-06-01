#ifndef MESSENGER_HPP
#define MESSENGER_HPP

#include <memory>

class MessageListener; 

enum ControlMessage{
    EMPTY = 0,
    ADDR_RECVED,
    STOP_RECVED
};

// cross thread comm utility class, where messenger feeds message and listeners retrieve them
class Messenger{
public:
    Messenger();

    Messenger(const Messenger& ) = delete;

    Messenger& operator=(const Messenger& ) = delete;

    ~Messenger();

    /**
    * @brief spawn a listener associated with this messenger
    * 
    * @param void
    * @return reference to a message listener spawned
    * 
    */
    MessageListener& spawn();

    /**
    * @brief set a message and notify all the listeners
    * 
    * @param msg message fed
    * @return success status
    * 
    */
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

    /**
    * @brief pop a message from listener in a blocking manner
    * 
    * @param msg message being poped
    * @return success status
    * 
    */
    bool pop(ControlMessage* msg);

    /**
    * @brief try pop a message from listener without blocking
    * 
    * @param msg message being poped if any
    * @return true if message is retrieved. false if no available messages
    * 
    */
    bool tryPop(ControlMessage* msg);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};


#endif //#ifndef MESSENGER_HPP