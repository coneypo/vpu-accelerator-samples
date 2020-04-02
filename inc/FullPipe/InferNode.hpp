#ifndef INFERNODE_HPP
#define INFERNODE_HPP

#include <hvaPipeline.hpp>
#include <string>
#include <common.hpp>
#include "unite_helper.hpp" 
#include "hddl2plugin_helper.hpp"

class InferNode : public hva::hvaNode_t{
public:
    InferNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum,
              WorkloadID id, std::string graphPath, std::string mode, HDDL2pluginHelper_t::PostprocPtr_t postproc);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

private:
    WorkloadID m_id;
    std::string m_graphPath;
    std::string m_mode;
    HDDL2pluginHelper_t::PostprocPtr_t m_postproc;
};

class InferNodeWorker : public hva::hvaNodeWorker_t{
public:
    InferNodeWorker(hva::hvaNode_t *parentNode, WorkloadID id, std::string graphPath, std::string mode, HDDL2pluginHelper_t::PostprocPtr_t postproc);

    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

    HDDL2pluginHelper_t m_helperHDDL2;
    std::vector<InfoROI_t> m_vecROI;
    std::string m_mode;

    float m_fps {0.0f};
    float m_durationAve {0.0f};
    uint64_t m_cntFrame {0ul};

private:
    std::vector<std::shared_ptr<hva::hvaBlob_t>> m_vecBlobInput;
};

#endif //#ifndef INFERNODE_HPP