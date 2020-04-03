#ifndef INFERNODE_UNITE_HPP
#define INFERNODE_UNITE_HPP

#include <hvaPipeline.hpp>
#include <string>
#include <common.hpp>
#include "unite_helper.hpp"

class InferNode_unite : public hva::hvaNode_t{
public:
    InferNode_unite(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, 
    WorkloadID id, std::string graphPath, std::string graphName, int32_t inputSizeNN, int32_t outputSize);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

private:
    WorkloadID id;
    std::string graphName;
    std::string graphPath;
    int32_t inputSizeNN;
    int32_t outputSize;

};

class InferNodeWorker_unite : public hva::hvaNodeWorker_t{
public:
    InferNodeWorker_unite(hva::hvaNode_t* parentNode,
    WorkloadID id, std::string graphName, std::string graphPath, int32_t inputSizeNN, int32_t outputSize);

    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

    hva::UniteHelper m_uniteHelper;
    std::vector<InfoROI_t> m_vecROI;
    std::string m_mode;

    float m_fps{0.0f};
    float m_durationAve{0.0f};
    uint64_t m_cntFrame{0ul};

private:

    std::vector<std::shared_ptr<hva::hvaBlob_t>> m_vecBlobInput;
};

#endif //#ifndef INFERNODE_HPP