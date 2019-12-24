//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_SPEEDRADAR_H
#define HDDLUNITE_SPEEDRADAR_H

#include <memory>
#include <vector>

namespace HddlUnite {
class SpeedRadar {
public:
    static constexpr uint64_t KiloByte { 1024 };
    static constexpr uint64_t MegaByte { 1024 * KiloByte };
    static constexpr uint64_t GigaByte { 1024 * MegaByte };

    class Entity {
    public:
        using Ptr = std::shared_ptr<Entity>;
        virtual ~Entity() = default;
        virtual void prepareCommunication() = 0;
        virtual uint64_t write(const char* data, uint64_t dataLen) = 0;
        virtual uint64_t read(const char* data, uint64_t dataLen) = 0;
    };

    struct TestCase {
        uint64_t numLoops;
        uint64_t fragmentSize;
    };

    SpeedRadar();
    ~SpeedRadar();

    void setServerEntity(Entity::Ptr sender) noexcept;
    void setClientEntity(Entity::Ptr receiver) noexcept;
    void setTestCase(const std::vector<TestCase>& testCase) noexcept;
    void evaluateBandwidth();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
}

#endif //HDDLUNITE_SPEEDRADAR_H
