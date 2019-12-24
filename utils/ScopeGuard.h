//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//
#ifndef HDDLUNITE_SCOPEGUARD_H
#define HDDLUNITE_SCOPEGUARD_H

#include <functional>
#include <future>
#include <list>
#include <memory>

namespace HddlUnite {
class ScopeGuard {
public:
    ScopeGuard() = default;

    ~ScopeGuard()
    {
        if (m_active) {
            for (auto& callback : m_callbacks) {
                callback();
            }
        }
    }

    void dismiss()
    {
        m_active = false;
    }

    template <typename Func>
    void add(Func&& func)
    {
        m_callbacks.push_front([=]() { func(); });
    }

    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const NonCopyable&) = delete;

    ScopeGuard(ScopeGuard&& rhs) noexcept
        : m_active(rhs.m_active)
        , m_callbacks(std::move(rhs.m_callbacks))
    {
        rhs.dismiss();
    }

    ScopeGuard& operator=(ScopeGuard&& rhs) noexcept
    {
        m_active = rhs.m_active;
        m_callbacks = std::move(rhs.m_callbacks);
        rhs.dismiss();
        return *this;
    }

private:
    bool m_active { true };
    std::list<std::function<void()>> m_callbacks;
};

template <typename Func>
ScopeGuard makeScopeGuard(Func&& func)
{
    ScopeGuard guard;
    guard.add(std::forward<Func>(func));
    return guard;
}
}

#endif //HDDLUNITE_SCOPEGUARD_H
