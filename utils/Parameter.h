//
// Copyright (C) 2018-2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//
//
//Copyright (C) 2019 Intel Corporation
//
//SPDX-License-Identifier: MIT
//

#pragma once
#include <algorithm>
#include <cctype>
#include <exception>
#include <iterator>
#include <map>
#include <string>
#include <tuple>
#include <typeinfo>
#include <utility>
#include <vector>

#ifndef HDDLUNITE_API
#ifdef WIN32
#ifdef BUILD_HDDL_API
#define HDDLUNITE_API __declspec(dllexport)
#else
#define HDDLUNITE_API __declspec(dllimport)
#endif
#else
#define HDDLUNITE_API
#endif
#endif

namespace HddlUnite {
class Exception : public std::exception {
public:
    /**
     * @brief A constructor. Creates an object from a specific file and line
     * @param filename File where exception has been thrown
     * @param line Line of the exception emitter
     */
    Exception(const char* errorDesc, std::string filename, const int line)
        : m_errorDesc(errorDesc)
        , m_file(std::move(filename))
        , m_line(line)
    {
        m_showMessage = m_file + std::to_string(m_line) + m_errorDesc;
    }

    /**
     * @brief A C++ std::exception API member
     * @return An exception description with a file name and file line
     */
    const char* what() const noexcept override
    {
        return m_showMessage.c_str();
    }

private:
    std::string m_showMessage;
    std::string m_errorDesc;
    std::string m_file;
    int m_line;
};

#define HDDLException(error) throw Exception(error, __FILE__, __LINE__)

/**
 * @brief This class represents an object to work with different parameters
 */
class HDDLUNITE_API Parameter {
public:
    /**
     * @brief Default constructor
     */
    Parameter() = default;

    /**
     * @brief Move constructor
     * @param parameter Parameter object
     */
    Parameter(Parameter&& parameter) noexcept
    {
        std::swap(ptr, parameter.ptr);
    }

    /**
     * @brief Copy constructor
     * @param parameter Parameter object
     */
    Parameter(const Parameter& parameter) { *this = parameter; }

    /**
     * @brief Constructor creates parameter with object
     * @tparam T Parameter type
     * @tparam U Identity type-transformation
     * @param parameter object
     */
    template <class T, typename = typename std::enable_if<!std::is_same<typename std::decay<T>::type, Parameter>::value>::type>
    Parameter(T&& parameter)
    {
        static_assert(!std::is_same<typename std::decay<T>::type, Parameter>::value, "To prevent recursion");
        ptr = new RealData<typename std::decay<T>::type>(std::forward<T>(parameter));
    }

    /**
     * @brief Constructor creates string parameter from char *
     * @param str char array
     */
    Parameter(const char* str)
        : Parameter(std::string(str))
    {
    }

    /**
     * @brief Destructor
     */
    virtual ~Parameter() { clear(); }

    /**
     * Copy operator for Parameter
     * @param parameter Parameter object
     * @return Parameter
     */
    Parameter& operator=(const Parameter& parameter)
    {
        if (this == &parameter) {
            return *this;
        }
        clear();
        if (!parameter.empty())
            ptr = parameter.ptr->copy();
        return *this;
    }

    /**
     * Remove a value from parameter
     */
    void clear()
    {
        delete ptr;
        ptr = nullptr;
    }

    /**
     * Checks that parameter contains a value
     * @return false if parameter contains a value else false
     */
    bool empty() const noexcept { return nullptr == ptr; }

    /**
     * Checks the type of value
     * @tparam T Type of value
     * @return true if type of value is correct
     */
    template <class T>
    bool is() const
    {
        return empty() ? false : ptr->is(typeid(T));
    }

    /**
     * Dynamic cast to specified type
     * @tparam T type
     * @return casted object
     */
    template <typename T>
    T&& as() && { return std::move(dyn_cast<T>(ptr)); }

    /**
     * Dynamic cast to specified type
     * @tparam T type
     * @return casted object
     */
    template <class T>
    T& as() & { return dyn_cast<T>(ptr); }

    /**
     * Dynamic cast to specified type
     * @tparam T type
     * @return casted object
     */
    template <class T>
    const T& as() const& { return dyn_cast<T>(ptr); }

    /**
     * Dynamic cast to specified type
     * @tparam T type
     * @return casted object
     */
    template <class T>
    operator T &&() &&
    {
        return std::move(dyn_cast<typename std::remove_cv<T>::type>(ptr));
    }

    /**
     * Dynamic cast to specified type
     * @tparam T type
     * @return casted object
     */
    template <class T>
    operator T&() &
    {
        return dyn_cast<typename std::remove_cv<T>::type>(ptr);
    }

    /**
     * Dynamic cast to specified type
     * @tparam T type
     * @return casted object
     */
    template <class T>
    operator const T&() const&
    {
        return dyn_cast<typename std::remove_cv<T>::type>(ptr);
    }

    /**
     * Dynamic cast to specified type
     * @tparam T type
     * @return casted object
     */
    template <class T>
    operator T&() const&
    {
        return dyn_cast<typename std::remove_cv<T>::type>(ptr);
    }

    /**
     * @brief The comparison operator for the Parameter
     * @param rhs object to compare
     * @return true if objects are equal
     */
    bool operator==(const Parameter& rhs) const { return *ptr == *(rhs.ptr); }
    /**
     * @brief The comparison operator for the Parameter
     * @param rhs object to compare
     * @return true if objects aren't equal
     */
    bool operator!=(const Parameter& rhs) const { return !(*this == rhs); }

private:
    template <class T, class EqualTo>
    struct CheckOperatorEqual {
        template <class U, class V>
        static auto test(U*) -> decltype(std::declval<U>() == std::declval<V>())
        {
            return false;
        }

        template <typename, typename>
        static auto test(...) -> std::false_type
        {
            return {};
        }

        using type =
            typename std::is_same<bool, decltype(test<T, EqualTo>(nullptr))>::type;
    };

    template <class T, class EqualTo = T>
    struct HasOperatorEqual : CheckOperatorEqual<T, EqualTo>::type {
    };

    struct Any {
        virtual ~Any() = default;
        virtual bool is(const std::type_info&) const = 0;
        virtual Any* copy() const = 0;
        virtual bool operator==(const Any& rhs) const = 0;
    };

    template <class T>
    struct RealData : Any, std::tuple<T> {
        using std::tuple<T>::tuple;

        bool is(const std::type_info& id) const override { return id == typeid(T); }
        Any* copy() const override { return new RealData { get() }; }

        T& get() &
        {
            return std::get<0>(*static_cast<std::tuple<T>*>(this));
        }

        const T& get() const&
        {
            return std::get<0>(*static_cast<const std::tuple<T>*>(this));
        }

        template <class U>
        typename std::enable_if<!HasOperatorEqual<U>::value, bool>::type
        equal(const Any&, const Any&) const
        {
            HDDLException("Parameter doesn't contain equal operator");
        }

        template <class U>
        typename std::enable_if<HasOperatorEqual<U>::value, bool>::type
        equal(const Any& left, const Any& rhs) const
        {
            return dyn_cast<U>(&left) == dyn_cast<U>(&rhs);
        }

        bool operator==(const Any& rhs) const override
        {
            return rhs.is(typeid(T)) && equal<T>(*this, rhs);
        }
    };

    template <typename T>
    static T& dyn_cast(Any* obj)
    {
        if (obj == nullptr)
            HDDLException("Parameter is empty!");
        return dynamic_cast<RealData<T>&>(*obj).get();
    }

    template <typename T>
    static const T& dyn_cast(const Any* obj)
    {
        if (obj == nullptr)
            HDDLException("Parameter is empty!");
        return dynamic_cast<const RealData<T>&>(*obj).get();
    }

    Any* ptr = nullptr;
};

} // namespace HddlUnite
