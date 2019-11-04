#ifndef HVA_HVAEXECUTOR_HPP
#define HVA_HVAEXECUTOR_HPP

#include <memory>
#include <vector>
#include <thread>

#include <inc/api/hvaNode.hpp>
#include <inc/util/hvaGraphMeta.hpp>
#include <inc/util/hvaUtil.hpp>
#include <3rdparty/ade/include/ade/graph.hpp>
#include <3rdparty/ade/include/ade/typed_graph.hpp>

namespace hva{

// class hvaNode_t;
class hvaNodeWorker_t;

using hvaEctTGraph_t = ade::TypedGraph<NodeWorkerMeta, NodeNameMeta>;

// todo: make executor separated from executor base, where executor base
//  stores the graph and executor stores only an ordered list

class hvaExecutor_t{
public:

    hvaExecutor_t();

    // obsoleted hvaExecutor_t(std::size_t duplicateNum, std::size_t threadPerBatch);
    hvaExecutor_t(std::size_t duplicateNum, ms loopingInterval, const hvaBatchingConfig_t* batchingConfig = nullptr);

    hvaExecutor_t(const hvaExecutor_t& ect);

    hvaNodeWorker_t& addNode(const hvaNode_t& node, std::string name);

    void linkNode(std::string prevNodeName, std::string currNodeName);

    std::size_t getDuplicateNum() const;

    void setBatchIdx(std::size_t batchIdx);

    std::size_t getThreadNumPerBatch() const;

    unsigned getBatchingPolicy() const;

    ms getLoopingInterval() const;

    void generateSorted();

    void init();

    void start();

    void stop();

#ifdef KL_TEST
public:
#else
private:
#endif
    void traverse(std::unordered_set<ade::Node*>& traversed, const ade::NodeHandle& node);

    ade::Graph m_graph;
    std::unique_ptr<hvaEctTGraph_t> m_pTGraph;
    std::unordered_map<std::string,ade::NodeHandle> m_nhMap;
    std::vector<ade::NodeHandle> m_sorted;
    std::size_t m_duplicateNum;
    // std::size_t m_threadPerBatch;
    std::size_t m_batchIdx;
    const hvaBatchingConfig_t* m_batchingConfig;
    ms m_loopingInterval;
    std::thread m_execThread;
    std::string m_srcNodeName;
    volatile bool m_stop;
};

}

#endif //HVA_HVAEXECUTOR_HPP