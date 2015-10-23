/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2015] Realm Inc
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of Realm Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Realm Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Realm Incorporated.
 *
 **************************************************************************/

#pragma once
#ifndef REALM_UTIL_OPTIONAL_HPP
#define REALM_UTIL_OPTIONAL_HPP

#include <realm/util/features.h>

#include <stdexcept> // std::logic_error
#include <functional> // std::less

namespace realm {
namespace util {

template <class T> class Optional;

// some() should be the equivalent of the proposed C++17 `make_optional`.
template <class T, class... Args> Optional<T> some(Args&&...);
template <class T> struct Some;

// Note: Should conform with the future std::nullopt_t and std::in_place_t.
struct None { constexpr explicit None(int) {} };
static constexpr None none { 0 };
struct InPlace { constexpr InPlace() {} };
static constexpr InPlace in_place;

// Note: Should conform with the future std::bad_optional_access.
struct BadOptionalAccess : std::logic_error {
    explicit BadOptionalAccess(const std::string& what_arg) : std::logic_error(what_arg) {}
    explicit BadOptionalAccess(const char* what_arg) : std::logic_error(what_arg) {}
};

} // namespace util

namespace _impl {

template <class T, bool=std::is_trivially_destructible<T>::value> struct OptionalStorage;

// FIXME: Callers should switch to std::move when we adopt C++14
template <class T> inline constexpr typename std::remove_reference<T>::type&& constexpr_move(T&& t) noexcept
{
    return static_cast<typename std::remove_reference<T>::type&&>(t);
}

// FIXME: Callers should switch to std::forward when we adopt C++14
template <class T> inline constexpr T&& constexpr_forward(typename std::remove_reference<T>::type& t) noexcept
{
    return static_cast<T&&>(t);
}

// FIXME: Callers should switch to std::forward when we adopt C++14
template <class T> inline constexpr T&& constexpr_forward(typename std::remove_reference<T>::type&& t) noexcept
{
    static_assert(!std::is_lvalue_reference<T>::value, "Can't forward rvalue as lvalue.");
    return static_cast<T&&>(t);
}

template <class T, class U>
class TypeIsAssignableToOptional {
    // Constraints from [optional.object.assign.18]
    static const bool value = (std::is_same<typename std::remove_reference<U>::type, T>::value
                               && std::is_constructible<T, U>::value
                               && std::is_assignable<T&, U>::value);
};

} // namespace _impl

namespace util {

// Note: Should conform with the future std::optional.
template <class T>
class Optional : private _impl::OptionalStorage<T> {
public:
    using value_type = T;

    constexpr Optional();
    constexpr Optional(None);
    Optional(Optional<T>&& other);
    Optional(const Optional<T>& other);

    constexpr Optional(T&& value);
    constexpr Optional(const T& value);

    template <class... Args>
    constexpr Optional(InPlace tag, Args&&...);
    // FIXME: std::optional specifies an std::initializer_list constructor overload as well.

    Optional<T>& operator=(None);
    Optional<T>& operator=(Optional<T>&& other);
    Optional<T>& operator=(const Optional<T>& other);

    template <class U, class = typename std::enable_if<_impl::TypeIsAssignableToOptional<T, U>::value>::type>
    Optional<T>& operator=(U&& value);

    explicit constexpr operator bool() const;
    constexpr const T& value() const; // Throws
    T& value(); // Throws, FIXME: Can be constexpr with C++14
    constexpr const T& operator*() const; // Throws
    T& operator*(); // Throws, FIXME: Can be constexpr with C++14
    constexpr const T* operator->() const; // Throws
    T* operator->(); // Throws, FIXME: Can be constexpr with C++14

    template <class U>
    constexpr T value_or(U&& value) const&;

    template <class U>
    T value_or(U&& value) &&;

    void swap(Optional<T>& other); // FIXME: Add noexcept() clause

    template <class... Args>
    void emplace(Args&&...);
    // FIXME: std::optional specifies an std::initializer_list overload for `emplace` as well.
private:
    using Storage = _impl::OptionalStorage<T>;
    using Storage::m_engaged;
    using Storage::m_value;

    constexpr bool is_engaged() const { return m_engaged; }
    void set_engaged(bool b) { m_engaged = b; }
    void clear();
};

/// An Optional<void> is functionally equivalent to a bool.
/// Note: C++17 does not (yet) specify this specialization, but it is convenient
/// as a "safer bool", especially in the presence of `fmap`.
/// Disabled for compliance with std::optional.
// template <>
// class Optional<void> {
// public:
//     Optional() {}
//     Optional(None) {}
//     Optional(Optional<void>&&) = default;
//     Optional(const Optional<void>&) = default;
//     explicit operator bool() const { return m_engaged; }
// private:
//     bool m_engaged = false;
//     friend struct Some<void>;
// };

/// An Optional<T&> is a non-owning nullable pointer that throws on dereference.
// FIXME: Visual Studio 2015's constexpr support isn't sufficient to allow Optional<T&> to compile
// in constexpr contexts.
template <class T>
class Optional<T&> {
public:
    using value_type = T&;
    using target_type = typename std::decay<T>::type;

    constexpr Optional() {}
    constexpr Optional(None) : Optional() {}
    Optional(const Optional<T&>& other) = default;
    template <class U>
    Optional(const Optional<U&>& other) : m_ptr(other.m_ptr) {}
    template <class U>
    Optional(std::reference_wrapper<U> ref) : m_ptr(&ref.get()) {}

    constexpr Optional(T& value) : m_ptr(&value) {}
    Optional(T&& value) = delete; // Catches accidental references to rvalue temporaries.

    Optional<T&>& operator=(None) { m_ptr = nullptr; return *this; }
    Optional<T&>& operator=(const Optional<T&>& other) { m_ptr = other.m_ptr; return *this; }

    template <class U>
    Optional<T&>& operator=(std::reference_wrapper<U> ref) { m_ptr = &ref.get(); return *this; }

    explicit constexpr operator bool() const { return m_ptr; }
    constexpr const target_type& value() const; // Throws
    target_type& value(); // Throws
    constexpr const target_type& operator*() const { return value(); }
    target_type& operator*() { return value(); }
    constexpr const target_type* operator->() const { return &value(); }
    target_type* operator->() { return &value(); }

    void swap(Optional<T&> other); // FIXME: Add noexcept() clause
private:
    T* m_ptr = nullptr;

    template <class U> friend class Optional;
};

/// Implementation:

template <class T>
struct Some {
    template <class... Args>
    static Optional<T> some(Args&&... args)
    {
        return Optional<T>{std::forward<Args>(args)...};
    }
};

/// Disabled for compliance with std::optional.
// template <>
// struct Some<void> {
//     static Optional<void> some()
//     {
//         Optional<void> opt;
//         opt.m_engaged = true;
//         return opt;
//     }
// };

template <class T, class... Args>
Optional<T> some(Args&&... args)
{
    return Some<T>::some(std::forward<Args>(args)...);
}


template <class T>
constexpr Optional<T>::Optional(): Storage(none)
{
}

template <class T>
constexpr Optional<T>::Optional(None): Storage(none)
{
}

template <class T>
Optional<T>::Optional(Optional<T>&& other): Storage(none)
{
    if (other.m_engaged) {
        new(&m_value) T(std::move(other.m_value));
        m_engaged = true;
    }
}

template <class T>
Optional<T>::Optional(const Optional<T>& other): Storage(none)
{
    if (other.m_engaged) {
        new(&m_value) T(other.m_value);
        m_engaged = true;
    }
}

template <class T>
constexpr Optional<T>::Optional(T&& value): Storage(_impl::constexpr_move(value))
{
}

template <class T>
constexpr Optional<T>::Optional(const T& value): Storage(value)
{
}

template <class T>
template <class... Args>
constexpr Optional<T>::Optional(InPlace, Args&&... args): Storage(std::forward<Args>(args)...)
{
}

template <class T>
void Optional<T>::clear()
{
    if (m_engaged) {
        m_value.~T();
        m_engaged = false;
    }
}

template <class T>
Optional<T>& Optional<T>::operator=(None)
{
    clear();
    return *this;
}

template <class T>
Optional<T>& Optional<T>::operator=(Optional<T>&& other)
{
    if (m_engaged) {
        if (other.m_engaged) {
            m_value = std::move(other.m_value);
        }
        else {
            clear();
        }
    }
    else {
        if (other.m_engaged) {
            new(&m_value) T(std::move(other.m_value));
            m_engaged = true;
        }
    }
    return *this;
}

template <class T>
Optional<T>& Optional<T>::operator=(const Optional<T>& other)
{
    if (m_engaged) {
        if (other.m_engaged) {
            m_value = other.m_value;
        }
        else {
            clear();
        }
    }
    else {
        if (other.m_engaged) {
            new(&m_value) T(other.m_value);
            m_engaged = true;
        }
    }
    return *this;
}

template <class T>
template <class U, class>
Optional<T>& Optional<T>::operator=(U&& value)
{
    if (m_engaged) {
        m_value = std::forward<U>(value);
    }
    else {
        new(&m_value) T(std::forward<U>(value));
        m_engaged = true;
    }
    return *this;
}

template <class T>
constexpr Optional<T>::operator bool() const
{
    return m_engaged;
}

template <class T>
constexpr const T& Optional<T>::value() const
{
    return m_engaged ? m_value : (throw BadOptionalAccess{"bad optional access"}, m_value);
}

template <class T>
T& Optional<T>::value()
{
    if (!m_engaged) {
        throw BadOptionalAccess{"bad optional access"};
    }
    return m_value;
}

template <class T>
constexpr const typename Optional<T&>::target_type& Optional<T&>::value() const
{
    return m_ptr ? *m_ptr : (throw BadOptionalAccess{"bad optional access"}, *m_ptr);
}

template <class T>
typename Optional<T&>::target_type& Optional<T&>::value()
{
    if (!m_ptr) {
        throw BadOptionalAccess{"bad optional access"};
    }
    return *m_ptr;
}

template <class T>
constexpr const T& Optional<T>::operator*() const
{
    // Note: This differs from std::optional, which doesn't throw.
    return value();
}

template <class T>
T& Optional<T>::operator*()
{
    // Note: This differs from std::optional, which doesn't throw.
    return value();
}

template <class T>
constexpr const T* Optional<T>::operator->() const
{
    // Note: This differs from std::optional, which doesn't throw.
    return &value();
}

template <class T>
T* Optional<T>::operator->()
{
    // Note: This differs from std::optional, which doesn't throw.
    return &value();
}

template <class T>
template <class U>
constexpr T Optional<T>::value_or(U&& otherwise) const&
{
    return m_engaged ? T{m_value} : T{_impl::constexpr_forward<U>(otherwise)};
}

template <class T>
template <class U>
T Optional<T>::value_or(U&& otherwise) &&
{
    if (is_engaged()) {
        return T(std::move(m_value));
    }
    else {
        return T(std::forward<U>(otherwise));
    }
}

template <class T>
void Optional<T>::swap(Optional<T>& other)
{
    // FIXME: This might be optimizable.
    Optional<T> tmp = std::move(other);
    other = std::move(*this);
    *this = std::move(tmp);
}

template <class T>
template <class... Args>
void Optional<T>::emplace(Args&&... args)
{
    clear();
    new(&m_value) T(std::forward<Args>(args)...);
    m_engaged = true;
}


template <class T>
constexpr Optional<typename std::decay<T>::type>
make_optional(T&& value)
{
    using Type = typename std::decay<T>::type;
    return some<Type>(std::forward<T>(value));
}

template <class T>
bool operator==(const Optional<T>& lhs, const Optional<T>& rhs)
{
    if (!lhs && !rhs) { return true; }
    if (lhs && rhs) { return *lhs == *rhs; }
    return false;
}

template <class T>
bool operator<(const Optional<T>& lhs, const Optional<T>& rhs)
{
    if (!rhs) { return false; }
    if (!lhs) { return true; }
    return std::less<T>{}(*lhs, *rhs);
}

template <class T>
bool operator==(const Optional<T>& lhs, None)
{
    return !bool(lhs);
}

template <class T>
bool operator<(const Optional<T>& lhs, None)
{
    static_cast<void>(lhs);
    return false;
}

template <class T>
bool operator==(None, const Optional<T>& rhs)
{
    return !bool(rhs);
}

template <class T>
bool operator<(None, const Optional<T>& rhs)
{
    return bool(rhs);
}

template <class T>
bool operator==(const Optional<T>& lhs, const T& rhs)
{
    return lhs ? *lhs == rhs : false;
}

template <class T>
bool operator<(const Optional<T>& lhs, const T& rhs)
{
    return lhs ? std::less<T>{}(*lhs, rhs) : true;
}

template <class T>
bool operator==(const T& lhs, const Optional<T>& rhs)
{
    return rhs ? lhs == *rhs : false;
}

template <class T>
bool operator<(const T& lhs, const Optional<T>& rhs)
{
    return rhs ? std::less<T>{}(lhs, *rhs) : false;
}

template <class T, class F>
auto operator>>(Optional<T> lhs, F&& rhs) -> decltype(fmap(lhs, std::forward<F>(rhs)))
{
    return fmap(lhs, std::forward<F>(rhs));
}

} // namespace util

namespace _impl {

// T is trivially destructible.
template <class T>
struct OptionalStorage<T, true> {
    union {
        T m_value;
        char m_null_state;
    };
    bool m_engaged = false;

    constexpr OptionalStorage(realm::util::None) : m_null_state() { }
    constexpr OptionalStorage(T&& value) : m_value(constexpr_move(value)), m_engaged(true) { }

    template <class... Args>
    constexpr OptionalStorage(Args&&... args): m_value(args...), m_engaged(true) { }
};

// T is not trivially destructible.
template <class T>
struct OptionalStorage<T, false> {
    union {
        T m_value;
        char m_null_state;
    };
    bool m_engaged = false;

    constexpr OptionalStorage(realm::util::None) : m_null_state() { }
    constexpr OptionalStorage(T&& value) : m_value(constexpr_move(value)), m_engaged(true) { }

    template <class... Args>
    constexpr OptionalStorage(Args&&... args): m_value(args...), m_engaged(true) { }

    ~OptionalStorage()
    {
        if (m_engaged)
            m_value.~T();
    }
};

} // namespace _impl

using util::none;

} // namespace realm

#endif // REALM_UTIL_OPTIONAL_HPP
