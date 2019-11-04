#ifndef HVA_HVABUF_HPP
#define HVA_HVABUF_HPP

#include <chrono>
#include <memory>
#include <type_traits>

#include <iostream>

#include <inc/api/hvaDevice.hpp>
#include <inc/util/hvaUtil.hpp>

namespace hva{

template<typename T, typename META_T = void>
class hvaBuf_t{
template<typename OtherT, typename OtherMeta_T> friend class hvaBuf_t;
public:
    hvaBuf_t(T* ptr, std::size_t size, META_T* metaPtr = nullptr,std::function<void(T*, META_T*)> d = {});

    hvaBuf_t(const hvaBuf_t<T, META_T>& buf) = delete;

    hvaBuf_t<T,META_T>& operator=(const hvaBuf_t<T,META_T>& buf) = delete;

    hvaBuf_t(hvaBuf_t<T,META_T>&& buf);

    hvaBuf_t& operator=(hvaBuf_t<T,META_T>&& buf);

    hvaBuf_t();

    ~hvaBuf_t();

    hvaBuf_t<T, META_T>* clone(const hvaBuf_t<T, META_T>& buf) const;

    std::string getKeyString() const;

    int getUID() const;

    template<typename OtherT, typename OtherMeta_T>
    bool convertTo(std::string keyString, hvaBuf_t<OtherT,OtherMeta_T>** otherBuf) const;

    T* getPtr() const;

    static void dummy();

    void setMeta(META_T* const & pMeta);

    META_T* getMeta() const;

    std::size_t getSize() const;

    void setSize(std::size_t size);

private:
    std::function<void(T*, META_T*)> m_deleter;
    std::size_t m_bufSize;
    T* m_buf;
    META_T* m_pMeta;
};



template<typename T, typename META_T>
hvaBuf_t<T,META_T>::hvaBuf_t(T* ptr, std::size_t size, META_T* metaPtr, std::function<void(T*, META_T*)> d)
        :m_bufSize(size),m_pMeta(metaPtr), m_deleter(d),m_buf(ptr){
    if(std::is_same<T, void>::value || std::is_same<META_T, void>::value)
        HVA_ASSERT(m_deleter);
}

template<typename T, typename META_T>
hvaBuf_t<T,META_T>::hvaBuf_t() 
        :m_bufSize(0),m_pMeta(nullptr), m_deleter(std::function<void(T*, META_T*)>{}),m_buf(nullptr){ }

// template<typename T, typename META_T>
// hvaBuf_t<T,META_T>::hvaBuf_t(const hvaBuf_t<T, META_T>& buf)
//         :hvaBufBase_t(buf.getSize()),m_buf(buf.m_buf), m_pMeta(nullptr){
//     if(!std::is_same<META_T, void>::value){
//         if(buf.m_pMeta){
//             m_pMeta = new META_T(*buf.m_pMeta);
//         }
//     }
// }

template<typename T, typename META_T>
hvaBuf_t<T,META_T>::hvaBuf_t(hvaBuf_t<T,META_T>&& buf)
        :m_bufSize(buf.m_bufSize),m_pMeta(buf.m_pMeta), m_deleter(buf.m_deleter),m_buf(buf.m_buf){
    buf.m_bufSize = 0;
    buf.m_buf = nullptr;
    buf.m_pMeta = nullptr;
    buf.m_deleter = std::function<void(T*, META_T*)>{};
}

/**
* @brief move operator for hvaBuf_t. May cause trouble here. set it to =delete if neccessary
*
* @param 
* @param 
* @return 
*
* @auther KL 
* 
*/
template<typename T, typename META_T>
hvaBuf_t<T,META_T>& hvaBuf_t<T,META_T>::operator=(hvaBuf_t<T,META_T>&& buf){
    if(this != &buf){
        if(m_deleter){
            m_deleter(m_buf, m_pMeta);
        }
        else{
            if(m_buf) delete m_buf;
            if(m_pMeta) delete m_pMeta;
        }
        m_bufSize = buf.m_bufSize;
        m_buf = buf.m_buf;
        m_pMeta = buf.m_pMeta;
        m_deleter = buf.m_deleter;

        buf.m_bufSize = 0;
        buf.m_buf = nullptr;
        buf.m_pMeta = nullptr;
        buf.m_deleter = std::function<void(T*, META_T*)>{};
    }
    return *this;
}

template<typename T, typename META_T>
hvaBuf_t<T,META_T>::~hvaBuf_t(){
    if(m_deleter){
        m_deleter(m_buf, m_pMeta);
    }
    else{
        // under no cases that in this clause m_buf
        //  and m_pMeta can be of void*
        if(m_buf) delete m_buf;
        if(m_pMeta) delete m_pMeta;
    }
}

template<typename T, typename META_T>
hvaBuf_t<T, META_T>* hvaBuf_t<T,META_T>::clone(const hvaBuf_t<T, META_T>& buf) const{
    hvaBuf_t<T, META_T>* ret = new hvaBuf_t<T, META_T>();
    if(buf.m_buf) ret->m_buf = new T{*buf.m_buf};
    if(buf.m_pMeta) ret->m_pMeta = new META_T{*buf.m_pMeta};
    ret->m_deleter = buf.m_deleter;
    ret->m_bufSize = buf.m_bufSize;
    return ret;
}

/**
* @brief Every specialization of hvaBuf_t should implement this otherwise return 
*   a default typeid().name() 
*
* @param 
* @param 
* @return 
*
* @auther KL 
* 
*/

template<typename T, typename META_T>
void hvaBuf_t<T,META_T>::dummy(){

}

template<typename T, typename META_T>
std::string hvaBuf_t<T,META_T>::getKeyString() const{
    return typeid(T).name();
}

template<typename T, typename META_T>
int hvaBuf_t<T,META_T>::getUID() const{
    return reinterpret_cast<int64_t>(&dummy);
}

/**
* @brief Every specialization of hvaBuf_t should its own set of further specialization
*   on this convert function. If not supported it should return false.
*
* @param 
* @param 
* @return 
*
* @auther KL 
* 
*/

template<typename T, typename META_T>
template<typename OtherT, typename OtherMeta_T>
bool hvaBuf_t<T,META_T>::convertTo(std::string keyString, hvaBuf_t<OtherT,OtherMeta_T>** otherBuf) const{
    return false;
}

template<typename T, typename META_T>
T* hvaBuf_t<T,META_T>::getPtr() const{
    return m_buf;
}

template<typename T, typename META_T>
void hvaBuf_t<T,META_T>::setMeta(META_T* const & pMeta){
    HVA_ASSERT(!(std::is_same<META_T, void>::value));
    m_pMeta = pMeta;
}

template<typename T, typename META_T>
META_T* hvaBuf_t<T,META_T>::getMeta() const{
    return m_pMeta;
}

template<typename T, typename META_T>
std::size_t hvaBuf_t<T,META_T>::getSize() const{
    return m_bufSize;
}

template<typename T, typename META_T>
void hvaBuf_t<T,META_T>::setSize(std::size_t size){
    m_bufSize = size;
}


template<typename T>
class hvaBuf_t<T, void>{
template<typename OtherT, typename OtherMeta_T> friend class hvaBuf_t;
public:
    hvaBuf_t(T* ptr, std::size_t size, std::function<void(T*)> d = {});

    hvaBuf_t(const hvaBuf_t<T>& buf) = delete;

    hvaBuf_t<T>& operator=(const hvaBuf_t<T>& buf) = delete;

    hvaBuf_t(hvaBuf_t<T>&& buf);

    hvaBuf_t& operator=(hvaBuf_t<T>&& buf);

    hvaBuf_t();

    ~hvaBuf_t();

    hvaBuf_t<T>* clone(const hvaBuf_t<T>& buf) const;

    std::string getKeyString() const;

    int getUID() const;

    template<typename OtherT>
    bool convertTo(std::string keyString, hvaBuf_t<OtherT>** otherBuf) const;

    T* getPtr() const;

    static void dummy();

    std::size_t getSize() const;

    void setSize(std::size_t size);

private:
    std::function<void(T*)> m_deleter;
    std::size_t m_bufSize;
    T* m_buf;
};


template<typename T>
hvaBuf_t<T>::hvaBuf_t(T* ptr, std::size_t size, std::function<void(T*)> d)
        :m_bufSize(size), m_deleter(d),m_buf(ptr){
    if(std::is_same<T, void>::value)
        HVA_ASSERT(m_deleter);
}

template<typename T>
hvaBuf_t<T>::hvaBuf_t() 
        :m_bufSize(0),m_deleter(std::function<void(T*)>{}),m_buf(nullptr){ }

template<typename T>
hvaBuf_t<T>::hvaBuf_t(hvaBuf_t<T>&& buf)
        :m_bufSize(buf.m_bufSize), m_deleter(buf.m_deleter),m_buf(buf.m_buf){
    buf.m_bufSize = 0;
    buf.m_buf = nullptr;
    buf.m_deleter = std::function<void(T*)>{};
}

/**
* @brief move operator for hvaBuf_t. May cause trouble here. set it to =delete if neccessary
*
* @param 
* @param 
* @return 
*
* @auther KL 
* 
*/
template<typename T>
hvaBuf_t<T>& hvaBuf_t<T>::operator=(hvaBuf_t<T>&& buf){
    if(this != &buf){
        if(m_deleter){
            m_deleter(m_buf);
        }
        else{
            if(m_buf) delete m_buf;
        }
        m_bufSize = buf.m_bufSize;
        m_buf = buf.m_buf;
        m_deleter = buf.m_deleter;

        buf.m_bufSize = 0;
        buf.m_buf = nullptr;
        buf.m_deleter = std::function<void(T*)>{};
    }
    return *this;
}

template<typename T>
hvaBuf_t<T>::~hvaBuf_t(){
    if(m_deleter){
        m_deleter(m_buf);
    }
    else{
        if(m_buf) delete m_buf;
    }
}

template<typename T>
hvaBuf_t<T>* hvaBuf_t<T>::clone(const hvaBuf_t<T>& buf) const{
    hvaBuf_t<T>* ret = new hvaBuf_t<T>();
    if(buf.m_buf) ret->m_buf = new T{*buf.m_buf};
    ret->m_deleter = buf.m_deleter;
    ret->m_bufSize = buf.m_bufSize;
    return ret;
}

/**
* @brief Every specialization of hvaBuf_t should implement this otherwise return 
*   a default typeid().name() 
*
* @param 
* @param 
* @return 
*
* @auther KL 
* 
*/

template<typename T>
void hvaBuf_t<T>::dummy(){

}

template<typename T>
std::string hvaBuf_t<T>::getKeyString() const{
    return typeid(T).name();
}

template<typename T>
int hvaBuf_t<T>::getUID() const{
    return reinterpret_cast<int64_t>(&dummy);
}

/**
* @brief Every specialization of hvaBuf_t should its own set of further specialization
*   on this convert function. If not supported it should return false.
*
* @param 
* @param 
* @return 
*
* @auther KL 
* 
*/

template<typename T>
template<typename OtherT>
bool hvaBuf_t<T>::convertTo(std::string keyString, hvaBuf_t<OtherT>** otherBuf) const{
    return false;
}

template<typename T>
T* hvaBuf_t<T>::getPtr() const{
    return m_buf;
}

template<typename T>
std::size_t hvaBuf_t<T>::getSize() const{
    return m_bufSize;
}

template<typename T>
void hvaBuf_t<T>::setSize(std::size_t size){
    m_bufSize = size;
}

#define SET_KEYSTRING(T, NAME) \
    namespace hva{ \
    template<> \
    std::string hva::hvaBuf_t<T>::getKeyString() const { return #NAME; }; \
    }

#define SET_KEYSTRING_WITH_META(T, META_T, NAME) \
    namespace hva{ \
    template<> \
    std::string hva::hvaBuf_t<T,META_T>::getKeyString() const { return #NAME; }; \
    }
}
#endif // HVA_HVABUF_HPP