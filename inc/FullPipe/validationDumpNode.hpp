#ifndef VALIDATION_DUMP_NODE_HPP
#define VALIDAITON_DUMP_NODE_HPP

#include <hvaPipeline.hpp>
#include <string>
#include <common.hpp>
#include <fstream>

class ValidationDumpNode : public hva::hvaNode_t{
public:
    ValidationDumpNode(std::size_t inPortNum, std::size_t outPortNum, std::size_t totalThreadNum, std::string networkName);

    virtual std::shared_ptr<hva::hvaNodeWorker_t> createNodeWorker() const override;

private:
    std::string m_networkName;
};

class ValidationDumpNodeWorker : public hva::hvaNodeWorker_t{
public:
    ValidationDumpNodeWorker(hva::hvaNode_t* parentNode, std::string networkName);

    virtual void process(std::size_t batchIdx) override;

    virtual void init() override;

    virtual void deinit() override;

    virtual ~ValidationDumpNodeWorker();
private:
    std::string m_networkName;
    std::ofstream m_of;
};

#endif //#ifndef VALIDATION_DUMP_NODE_HPP
