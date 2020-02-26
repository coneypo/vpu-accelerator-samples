#ifndef HVA_HVABLOB_HPP
#define HVA_HVABLOB_HPP

#include <inc/api/hvaBuf.hpp>

#include <functional>
#include <condition_variable>
#include <mutex>
#include <iostream>
#include <type_traits>
#include <utility>

namespace hva{

// class hvaBlob_t{
// public:
// std::condition_variable m_cvReady;
// std::mutex m_mutex;

// inline bool isReady(){

// }

// private:
// void* m_pData;
// bool m_bReady;
// };

template<bool, typename T1, typename T2>
struct _deleterHelper{
    typedef T2 type;
};

template<typename T1, typename T2>
struct _deleterHelper<true, T1, T2>{
    typedef T1 type;
};

class hvaBufContainerHolderBase_t{
public:
    virtual ~hvaBufContainerHolderBase_t();
};

template<typename T, typename META_T = void>
class hvaBufContainerHolder_t : public hvaBufContainerHolderBase_t{
public:
    hvaBufContainerHolder_t(hvaBuf_t<T,META_T>* buf);

    hvaBufContainerHolder_t(std::shared_ptr<hvaBuf_t<T,META_T>> buf);

    hvaBufContainerHolder_t() = delete;

    std::shared_ptr<hvaBuf_t<T,META_T>> m_buf;

};

class hvaBlob_t{
public:
    hvaBlob_t();

    ~hvaBlob_t();

    template<typename T, typename META_T = void>
    void push(hvaBuf_t<T, META_T>* buf);

    template<typename T, typename META_T = void>
    void push(std::shared_ptr<hvaBuf_t<T, META_T>> buf);

    template<typename T, typename META_T = void>
    std::shared_ptr<hvaBuf_t<T,META_T>> get(std::size_t idx);

    /* option 1 */
    // template<typename T>
    // std::shared_ptr<hvaBuf_t<T>> emplace(T* ptr, std::size_t size, std::function<void(T*)> d = {});

    // template<typename T, typename META_T>
    // std::shared_ptr<hvaBuf_t<T,META_T>> emplace(T* ptr, std::size_t size, META_T* metaPtr, std::function<void(T*, META_T*)> d = {});

    /* option 2 */
    template<typename T, typename META_T = void, typename std::enable_if<std::is_same<META_T,void>::value,int>::type = 0>
    std::shared_ptr<hvaBuf_t<T,META_T>> emplace(T* ptr, std::size_t size, META_T* metaPtr = nullptr, std::function<typename _deleterHelper<std::is_same<META_T,void>::value,void(T*),void(T*,META_T*)>::type> d = {});

    template<typename T, typename META_T = void, typename std::enable_if<!std::is_same<META_T,void>::value,int>::type = 0>
    std::shared_ptr<hvaBuf_t<T,META_T>> emplace(T* ptr, std::size_t size, META_T* metaPtr = nullptr, std::function<typename _deleterHelper<std::is_same<META_T,void>::value,void(T*),void(T*,META_T*)>::type> d = {});

    // template<typename T, typename META_T = void, typename F = void(T*, META_T*), typename = void>
    // std::shared_ptr<hvaBuf_t<T,META_T>> emplace(T* ptr, std::size_t size, META_T* metaPtr = nullptr, std::function<F> d = {});

    // template<typename T, typename META_T = void, typename F = void(T*), typename std::enable_if<std::is_same<META_T,void>::value, int>::type = 0>
    // std::shared_ptr<hvaBuf_t<T,META_T>> emplace(T* ptr, std::size_t size, META_T* metaPtr = nullptr, std::function<F> d = {});

    // template<typename... Args>
    // auto emplace(Args... args)-> typename std::shared_ptr<decltype(hvaBuf_t{args...})>;

    // std::vector<std::shared_ptr<hvaBufBase_t>> vpBuf; 
    std::vector<hvaBufContainerHolderBase_t*> vBuf;
    std::chrono::milliseconds timestamp;
    int streamId;
    int frameId;
    int typeId;
    int ctx;

};

template<typename T, typename META_T>
hvaBufContainerHolder_t<T,META_T>::hvaBufContainerHolder_t(hvaBuf_t<T,META_T>* buf)
        :m_buf{buf}{

}

template<typename T, typename META_T>
hvaBufContainerHolder_t<T,META_T>::hvaBufContainerHolder_t(std::shared_ptr<hvaBuf_t<T,META_T>> buf)
        :m_buf{buf}{

}

template<typename T, typename META_T>
void hvaBlob_t::push(hvaBuf_t<T, META_T>* buf){
    vBuf.push_back(new hvaBufContainerHolder_t<T,META_T>{buf});
}

template<typename T, typename META_T>
void hvaBlob_t::push(std::shared_ptr<hvaBuf_t<T, META_T>> buf){
    vBuf.push_back(new hvaBufContainerHolder_t<T,META_T>{buf});
}

template <typename T, typename META_T>
std::shared_ptr<hvaBuf_t<T,META_T>> hvaBlob_t::get(std::size_t idx){
    return dynamic_cast<hvaBufContainerHolder_t<T,META_T>*>(vBuf[idx])->m_buf;
}

// template<typename T>
// std::shared_ptr<hvaBuf_t<T>> hvaBlob_t::emplace(T* ptr, std::size_t size, std::function<void(T*)> d){
//     push(new hvaBuf_t<T>(ptr, size, d));
//     return dynamic_cast<hvaBufContainerHolder_t<T>*>(vBuf.back())->m_buf;
// }

// template<typename T, typename META_T>
// std::shared_ptr<hvaBuf_t<T,META_T>> hvaBlob_t::emplace(T* ptr, std::size_t size, META_T* metaPtr, std::function<void(T*, META_T*)> d){
//     HVA_ASSERT((!std::is_same<META_T, void>::value))
//     push(new hvaBuf_t<T,META_T>(ptr,size,metaPtr,d));
//     return dynamic_cast<hvaBufContainerHolder_t<T,META_T>*>(vBuf.back())->m_buf;
// }

/* option 1 */
// template<typename T>
// std::shared_ptr<hvaBuf_t<T>> hvaBlob_t::emplace(T* ptr, std::size_t size, std::function<void(T*)> d){
//     push(new hvaBuf_t<T>(ptr,size,d));
//     return dynamic_cast<hvaBufContainerHolder_t<T,void>*>(vBuf.back())->m_buf;
    
// }

// template<typename T, typename META_T>
// std::shared_ptr<hvaBuf_t<T,META_T>> hvaBlob_t::emplace(T* ptr, std::size_t size, META_T* metaPtr, std::function<void(T*, META_T*)> d){
//     push(new hvaBuf_t<T,META_T>(ptr,size,metaPtr,d));
//     return dynamic_cast<hvaBufContainerHolder_t<T,META_T>*>(vBuf.back())->m_buf;
// }

/* option 2 */
#pragma optimize('', off)

// template<typename T, typename META_T>
// std::shared_ptr<hvaBuf_t<T,META_T>> hvaBlob_t::emplace(T* ptr, std::size_t size, META_T* metaPtr, std::function<typename _deleterHelper<std::is_same<META_T,void>::value,void(T*),void(T*,META_T*)>::type> d){
//     volatile bool temp = std::is_same<typename std::remove_reference<META_T>::type,void>::value;
//     if(temp){
//         // std::cout<<"META_T matches with void! META_T: "<<typeid(META_T)<<std::endl;
//         push(new hvaBuf_t<T,void>(ptr,size,d));
//         return dynamic_cast<hvaBufContainerHolder_t<T,void>*>(vBuf.back())->m_buf;
//     }
//     else
//     {
//         // std::cout<<"META_T does not match with void! META_T: "<<typeid(META_T)<<std::endl;
//         push(new hvaBuf_t<T,META_T>(ptr,size,metaPtr,d));
//         return dynamic_cast<hvaBufContainerHolder_t<T,META_T>*>(vBuf.back())->m_buf;
//     }
    
// }

template<typename T, typename META_T, typename std::enable_if<std::is_same<META_T,void>::value,int>::type>
std::shared_ptr<hvaBuf_t<T,META_T>> hvaBlob_t::emplace(T* ptr, std::size_t size, META_T* metaPtr, std::function<typename _deleterHelper<std::is_same<META_T,void>::value,void(T*),void(T*,META_T*)>::type> d){
    push(new hvaBuf_t<T,void>(ptr,size,d));
    return dynamic_cast<hvaBufContainerHolder_t<T,void>*>(vBuf.back())->m_buf;
}

template<typename T, typename META_T, typename std::enable_if<!std::is_same<META_T,void>::value,int>::type>
std::shared_ptr<hvaBuf_t<T,META_T>> hvaBlob_t::emplace(T* ptr, std::size_t size, META_T* metaPtr, std::function<typename _deleterHelper<std::is_same<META_T,void>::value,void(T*),void(T*,META_T*)>::type> d){
    push(new hvaBuf_t<T,META_T>(ptr,size,metaPtr,d));
    return dynamic_cast<hvaBufContainerHolder_t<T,META_T>*>(vBuf.back())->m_buf;
}


#pragma optimize('', on)

// template<typename T, typename META_T, typename F, typename>
// std::shared_ptr<hvaBuf_t<T,META_T>> hvaBlob_t::emplace(T* ptr, std::size_t size, META_T* metaPtr, std::function<F> d){
//     push(new hvaBuf_t<T,META_T>(ptr,size,metaPtr,d));
//     return dynamic_cast<hvaBufContainerHolder_t<T,META_T>*>(vBuf.back())->m_buf;
// }

// template<typename T, typename META_T, typename F, typename std::enable_if<std::is_same<META_T,void>::value, int>::type>
// std::shared_ptr<hvaBuf_t<T,META_T>> hvaBlob_t::emplace(T* ptr, std::size_t size, META_T* metaPtr, std::function<F> d){
//     push(new hvaBuf_t<T>(ptr,size,d));
//     return dynamic_cast<hvaBufContainerHolder_t<T,META_T>*>(vBuf.back())->m_buf;
// }

// template<typename T, typename META_T, typename F>
// std::shared_ptr<hvaBuf_t<T,META_T>> hvaBlob_t::emplace(T* ptr, std::size_t size, META_T* metaPtr, std::function<F> d){
//     if(std::is_same<META_T, void>::value){
//         push(new hvaBuf_t<T,void>(ptr,size,d));
//         return dynamic_cast<hvaBufContainerHolder_t<T,void>*>(vBuf.back())->m_buf;
//     }
//     else{
//         push(new hvaBuf_t<T,META_T>(ptr,size,metaPtr,d));
//         return dynamic_cast<hvaBufContainerHolder_t<T,META_T>*>(vBuf.back())->m_buf;
//     }
// }

/*
class hvaBufContainer_t{
public:
    hvaBufContainer_t():m_content(0) {};

    template <typename T, typename META_T = void>
    hvaBufContainer_t(const hvaBuf_t<T,META_T>& buf);

    hvaBufContainer_t(const hvaBufContainer_t& buf);

    ~hvaBufContainer_t();

    template <typename T, typename META_T = void>
    hvaBuf_t<T, META_T>& get() const;

private:
    std::shared_ptr<hvaBufContainerHolderBase_t> m_content;
};

class hvaBlob_t{
public:
    hvaBlob_t();

    template <typename T, typename META_T = void>
    void push(const hvaBuf_t<T, META_T>& buf);

    template <typename T, typename META_T = void>
    hvaBuf_t<T, META_T>& get(std::size_t idx);

    // std::vector<std::shared_ptr<hvaBufBase_t>> vpBuf; 
    std::vector<hvaBufContainer_t> vBuf;
    std::chrono::milliseconds timestamp;
    int streamId;
    int frameId;
    int typeId;
    int ctx;

};

template <typename T,typename META_T>
hvaBufContainerHolder_t<T,META_T>::hvaBufContainerHolder_t(const hvaBuf_t<T,META_T>& buf)
        :m_buf(buf){
    // std::cout<<"ContainerHolder created with T="<<typeid(T).name()<<std::endl;
    // m_buf = hvaBuf_t<T>(buf);
};

template <typename T, typename META_T>
hvaBufContainer_t::hvaBufContainer_t(const hvaBuf_t<T,META_T>& buf)
        :m_content(new hvaBufContainerHolder_t<T, META_T>(buf)){
    // m_content = new hvaBufContainerHolder_t<T>(buf);
    // std::cout<<"Container init with content type="<<typeid(m_content).name()<<" and T="<<typeid(T).name()<<std::endl;
};

template <typename T, typename META_T>
hvaBuf_t<T,META_T>& hvaBufContainer_t::get() const{
    // std::cout<<"Going to cast ContainerHolder T="<<typeid(T).name()<<" and content type="<<typeid(hvaBufContainerHolder_t<T>*).name()<<std::endl;
    auto temp = dynamic_cast<hvaBufContainerHolder_t<T,META_T>*>(m_content.get());
    return temp->m_buf;
};

template <typename T, typename META_T>
hvaBuf_t<T,META_T>& hvaBlob_t::get(std::size_t idx){
    // std::cout<<"Type in blob get T="<<typeid(T).name()<<std::endl;
    // std::cout.flush();
    return vBuf[idx].get<T,META_T>();
};

template <typename T, typename META_T>
void hvaBlob_t::push(const hvaBuf_t<T, META_T>& buf){
    // std::cout<<"Pusing buf val ="<<buf.getPtr()->val<<std::endl;
    vBuf.emplace_back(buf);
    // for(const auto& temp: vBuf){
    //     std::cout<<"TestE1: "<<temp.get<T>().getPtr()->val<<std::endl;
    // }
    // std::cout<<"TestE1: "<<vBuf[0].get<T>().getPtr()->val<<std::endl;
};
*/

}
#endif //#ifndef HVA_HVABLOB_HPP