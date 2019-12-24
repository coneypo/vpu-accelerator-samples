//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <chrono>
#include <numeric>
#include <vector>

#include "SpeedRadarHelper.h"
#include "utils/HLog.h"
#include "utils/SpeedRadar.h"
#include "utils/TimeUtils.h"

namespace HddlUnite {
static std::string makeMoreReadable(uint64_t numBytes)
{
    std::stringstream sstream;

    if (numBytes < SpeedRadar::KiloByte) {
        sstream << numBytes;
    } else if (numBytes >= SpeedRadar::KiloByte && numBytes < SpeedRadar::MegaByte) {
        sstream << (1.0 * numBytes / SpeedRadar::KiloByte) << 'K';
    } else if (numBytes >= SpeedRadar::MegaByte && numBytes < SpeedRadar::GigaByte) {
        sstream << (1.0 * numBytes / SpeedRadar::MegaByte) << 'M';
    } else if (numBytes > SpeedRadar::GigaByte) {
        sstream << (1.0 * numBytes / SpeedRadar::GigaByte) << 'G';
    }

    return sstream.str();
}

class SpeedRadar::Impl {
public:
    void setServerEntity(Entity::Ptr server) noexcept
    {
        m_serverEntity = server;
    }

    void setClientEntity(Entity::Ptr client) noexcept
    {
        m_clientEntity = client;
    }

    void setTestCase(const std::vector<TestCase>& testCase) noexcept
    {
        m_testCase = testCase;
    }

    void evaluateBandwidth()
    {
        SpeedRadarHelper helper;

        helper.setParentProcessRoutine([this] { executeParentProcessRoutine(); });
        helper.setChildProcessRoutine([this] { executeChildProcessRoutine(); });
        helper.run();
    }

private:
    struct PerfData {
        uint64_t numLoops { 0 };
        uint64_t fragmentSize { 0 };
        float maxDuration { 0 };
        float minDuration { 0 };
        float avgDuration { 0 };
        float bandWidth { 0 };

        template <class Archive>
        void serialize(Archive& archive, const unsigned int version)
        {
            archive& numLoops;
            archive& fragmentSize;
            archive& maxDuration;
            archive& minDuration;
            archive& avgDuration;
            archive& bandWidth;
            UNUSED(version);
        }

        void display() const noexcept
        {
            std::cout << "numLoops=" << numLoops << ' '
                      << "fragmentSize=" << makeMoreReadable(fragmentSize) << "B "
                      << "bandWidth=" << makeMoreReadable(bandWidth) << "B/s "
                      << "maxDuration=" << maxDuration << "ms "
                      << "minDuration=" << minDuration << "ms "
                      << "avgDuration=" << avgDuration << "ms" << std::endl;
        }
    };

    struct PerfDataVec {
        std::vector<PerfData> perfData;

        template <class Archive>
        void serialize(Archive& archive, const unsigned int version)
        {
            archive& perfData;
            UNUSED(version);
        }

        void display(const std::string prefix) const noexcept
        {
            std::cout << '\n'
                      << prefix << std::endl;

            for (auto& perf : perfData) {
                perf.display();
            }
        }
    };

    void executeParentProcessRoutine() noexcept
    {
        try {
            m_clientEntity->prepareCommunication();
            auto sendPerfData = collectSendPerfDataFromLocal();
            auto recvPerfData = collectRecvPerfDataFromChildProcess();
            sendPerfData.display("Send Performance:");
            recvPerfData.display("Receive Performance:");
            m_sendPerfData = std::move(sendPerfData);
            m_recvPerfData = std::move(recvPerfData);
        } catch (std::exception& exception) {
            HError("Error: %s", exception.what());
        }
    }

    PerfDataVec collectSendPerfDataFromLocal() noexcept
    {
        PerfDataVec perfDataVec;
        for (auto& entry : m_testCase) {
            try {
                auto perfData = collectSendPerfData(entry.numLoops, entry.fragmentSize);
                perfDataVec.perfData.push_back(std::move(perfData));
            } catch (std::exception& exception) {
                HError("Error: TestCase(numLoops=%lu fragmentSize=%lu) failed, error=%s", entry.numLoops, entry.fragmentSize, exception.what());
            }
        }
        return perfDataVec;
    }

    PerfDataVec collectRecvPerfDataFromChildProcess() noexcept
    {
        uint64_t strSize { 0 };
        auto readBytes = m_clientEntity->read((char*)&strSize, sizeof(uint64_t));
        if (readBytes != sizeof(uint64_t)) {
            HError("Error: receive perfData to parent process failed.");
            return {};
        }

        std::string str(strSize, '\0');
        readBytes = m_clientEntity->read(&str[0], strSize);
        if (readBytes != strSize) {
            HError("Error: receive perfData to parent process failed.");
            return {};
        }

        std::stringstream sstream;
        sstream << str;
        boost::archive::text_iarchive iarchive(sstream);
        PerfDataVec perfDataVec;
        iarchive& perfDataVec;

        return perfDataVec;
    }

    PerfData collectSendPerfData(uint64_t numLoops, uint64_t fragmentSize)
    {
        std::vector<char> buffers(fragmentSize);
        std::vector<float> duration(numLoops);

        for (uint64_t i = 0; i < numLoops; ++i) {
            auto start = std::chrono::steady_clock::now();
            auto writeBytes = m_clientEntity->write(&buffers[0], fragmentSize);
            if (writeBytes != fragmentSize) {
                HError("Error: write data failed, writeBytes=%lu expectBytes=%lu", writeBytes, fragmentSize);
                throw std::runtime_error("write failed");
            }
            auto end = std::chrono::steady_clock::now();
            duration[i] = TimeUtils::getTimeDiffInMs(start, end);
        }

        PerfData perfData;
        perfData.numLoops = numLoops;
        perfData.fragmentSize = fragmentSize;
        perfData.maxDuration = *std::max_element(duration.begin(), duration.end());
        perfData.minDuration = *std::min_element(duration.begin(), duration.end());
        perfData.avgDuration = std::accumulate(duration.begin(), duration.end(), 0.0) / numLoops;
        perfData.bandWidth = 1000.0 * fragmentSize / perfData.avgDuration;

        return perfData;
    }

    void executeChildProcessRoutine() noexcept
    {
        PerfDataVec perfDataVec;

        try {
            m_serverEntity->prepareCommunication();
            for (auto& entry : m_testCase) {
                perfDataVec.perfData.push_back(collectRecvPerfData(entry.numLoops, entry.fragmentSize));
            }
        } catch (...) {
        }

        sendPerfDataToParentProcess(perfDataVec);
    }

    PerfData collectRecvPerfData(uint64_t numLoops, uint64_t fragmentSize)
    {
        std::vector<char> buffers(fragmentSize);
        std::vector<float> duration(numLoops);

        for (uint64_t i = 0; i < numLoops; ++i) {
            auto start = std::chrono::steady_clock::now();
            auto readBytes = m_serverEntity->read(&buffers[0], fragmentSize);
            if (readBytes != fragmentSize) {
                HError("Error: read data failed, readBytes=%lu expectBytes=%lu", readBytes, fragmentSize);
                throw std::runtime_error("write failed");
            }
            auto end = std::chrono::steady_clock::now();
            duration[i] = TimeUtils::getTimeDiffInMs(start, end);
        }

        PerfData perfData;
        perfData.numLoops = numLoops;
        perfData.fragmentSize = fragmentSize;
        perfData.maxDuration = *std::max_element(duration.begin(), duration.end());
        perfData.minDuration = *std::min_element(duration.begin(), duration.end());
        perfData.avgDuration = std::accumulate(duration.begin(), duration.end(), 0.0) / numLoops;
        perfData.bandWidth = 1000.0 * fragmentSize / perfData.avgDuration;

        return perfData;
    }

    void sendPerfDataToParentProcess(PerfDataVec& perfData) noexcept
    {
        std::stringstream sstream;
        boost::archive::text_oarchive oarchive(sstream);
        oarchive& perfData;

        auto str = sstream.str();
        uint64_t strSize = str.size();

        auto writeBytes = m_serverEntity->write((const char*)&strSize, sizeof(uint64_t));
        if (writeBytes != sizeof(uint64_t)) {
            HError("Error: send perfData to parent process failed.");
            return;
        }

        writeBytes = m_serverEntity->write(&str[0], str.size());
        if (writeBytes != str.size()) {
            HError("Error: send perfData to parent process failed.");
        }
    }

private:
    Entity::Ptr m_serverEntity;
    Entity::Ptr m_clientEntity;
    std::vector<TestCase> m_testCase;
    PerfDataVec m_sendPerfData;
    PerfDataVec m_recvPerfData;
};

SpeedRadar::SpeedRadar()
    : m_impl(new Impl())
{
}

SpeedRadar::~SpeedRadar()
{
}

void SpeedRadar::setServerEntity(Entity::Ptr server) noexcept
{
    m_impl->setServerEntity(std::move(server));
}

void SpeedRadar::setClientEntity(Entity::Ptr client) noexcept
{
    m_impl->setClientEntity(std::move(client));
}

void SpeedRadar::setTestCase(const std::vector<TestCase>& testCase) noexcept
{
    m_impl->setTestCase(testCase);
}

void SpeedRadar::evaluateBandwidth()
{
    m_impl->evaluateBandwidth();
}
}
