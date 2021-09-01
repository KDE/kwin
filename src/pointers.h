/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QSharedPointer>
#include <memory>

/// a class that ensures a pointer is never null,
/// forbids assigning nullptr to it at compiletime,
/// and crashes at assignment point instead of usage
/// point, for easier debugging
template<typename T>
class NeverNull
{

    template<typename TT, typename = void>
    struct can_nullptr : std::false_type {};

    template<typename TT>
    struct can_nullptr<
        TT,
        std::enable_if_t<std::is_convertible<decltype(std::declval<TT>() != nullptr), bool>::value>
    > : std::true_type
    {};

    T m_pointer;

    // With modern compilers and most pointer types,
    // inlining should hopefully get it down to a simple 1231231092381 == 0
    // instruction at the assembly level.
    //
    // On modern CPUs, a branch that isn't taken has performance overhead neglible
    // enough thanks to branch prediction that we essentially get free null pointer
    // checks in terms of performance.
    //
    // Might as well have the extra safety.
    inline void ensureNotNull() const
    {
        if (Q_UNLIKELY(m_pointer == nullptr)) {
            qFatal("nullptr was assigned to m_pointer, or m_pointer became null otherwise (QPointer's watched object getting deleted)");
        }
    }

public:

    static_assert(can_nullptr<T>::value, "T is not a type that can be checked against nullptr, probably not a pointer type");

    // well, obviously we gotta block people from assigning a nullptr to this
    NeverNull(std::nullptr_t) = delete;
    NeverNull& operator=(std::nullptr_t) = delete;

    NeverNull(const NeverNull& other) = default;

    template<typename U>
    inline constexpr NeverNull(U&& u) : m_pointer(std::forward<U>(u))
    {
        ensureNotNull();
    }

    template<typename U>
    inline constexpr NeverNull(T t) : m_pointer(std::move(t))
    {
        ensureNotNull();
    }

    inline constexpr std::conditional_t<
        // if T is copy constructable... (like a plain pointer or a shared pointer)
        std::is_copy_constructible_v<T>,
        // then we just return T
        T,
        // otherwise we return a reference to it since it's something like a unique_ptr
        const T&
    > get() const {
        // something like a QPointer can become nullptr after being assigned,
        // so we do another sanity check here
        ensureNotNull();

        return m_pointer;
    }

    inline constexpr operator T() const { return get(); }

    // decltype(auto) is a funky way of having compiler infer
    // return type for us

    inline constexpr decltype(auto) operator->() const { return get(); }
    inline constexpr decltype(auto) operator*() const { return *get(); }

};

/// Because NeverNull<QSharedPointer<T>> can get unwieldy
template<typename T>
using NN = NeverNull<T>;
