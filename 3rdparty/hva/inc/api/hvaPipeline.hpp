#ifndef HVA_HVAPIPELINE_HPP
#define HVA_HVAPIPELINE_HPP

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#include <inc/util/hvaUtil.hpp>
#include <inc/util/hvaGraphMeta.hpp>
#include <inc/api/hvaNode.hpp>
#include <inc/api/hvaStream.hpp>
#include <inc/api/hvaCompiled.hpp>
#include <inc/api/hvaBlob.hpp>
#include <inc/api/hvaExecutor.hpp>
#include <3rdparty/ade/include/ade/graph.hpp>
#include <3rdparty/ade/include/ade/typed_graph.hpp>
#include <inc/api/hvaEvent.hpp>
#include <inc/api/hvaEventManager.hpp>

#include <TestConfig.h>

namespace hva{

class hvaOutPort_t;
class hvaInPort_t;
class hvaNode_t;

using hvaTGraph_t = ade::TypedGraph<NodeMeta, NatureMeta, NodeNameMeta, IsCompiledMeta>;

class hvaExecutor_t;

class hvaPipeline_t{
public:    
    /**
    * @brief 
    *
    * @param 
    * @param 
    * @return 
    *
    * @auther KL 
    * 
    */
    hvaPipeline_t();

    /**
    * @brief 
    *
    * @param 
    * @param 
    * @return 
    *
    * @auther KL 
    * 
    */
    hvaNode_t& addNode(std::shared_ptr<hvaNode_t> node, std::string name);

    hvaNode_t& setSource(std::shared_ptr<hvaNode_t> srcNode, std::string name);

    void linkNode(std::string prevNode, std::size_t prevNodePortIdx, std::string currNode, std::size_t currNodePortIdx, hvaConvertFunc func = {});

    void prepare();

    void start();

    void stop();

    void push(hvaBlob_t data);

    hvaStatus_t sendToPort(std::shared_ptr<hvaBlob_t> data, std::string nodeName, std::size_t portId, ms timeout = ms(1000));

    hvaStatus_t registerEvent(hvaEvent_t event);

    hvaStatus_t registerCallback(hvaEvent_t event, hvaEventHandlerFunc callback);

    hvaStatus_t emitEvent(hvaEvent_t event, void* data);

    hvaStatus_t waitForEvent(hvaEvent_t event);
#ifdef KL_TEST
public:
#else
private:
#endif
    void linkNode(hvaNode_t* prev, std::size_t prevNodePortIdx, hvaNode_t* next, std::size_t currNodePortIdx, hvaConvertFunc func);

    hvaExecutor_t& addExecutor(std::size_t duplicateNum, ms loopingInterval, const hvaBatchingConfig_t* batchingConfig);

    void addWorkerToExecutor(const ade::NodeHandle& nh, hvaExecutor_t& ect, std::string parentName);

    ade::Graph m_graph;
    std::unique_ptr<hvaTGraph_t> m_pTGraph;
    std::vector<std::unique_ptr<hvaExecutor_t>> m_vpExtor;
    std::unordered_map<std::string,ade::NodeHandle> m_nhMap;
    std::vector<std::string> m_vSource;
    std::unique_ptr<hvaEventManager_t> m_pEventMng;
};

}

#endif //#ifndef HVA_HVAPIPELINE_HPP