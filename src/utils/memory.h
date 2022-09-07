/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <utility>

template<typename T>
class RefPtr
{
public:
    RefPtr()
        : m_ptr(nullptr)
    {
    }
    RefPtr(T *ptr)
        : m_ptr(ptr)
    {
        if (m_ptr) {
            m_ptr->ref();
        }
    }
    RefPtr(const RefPtr &other)
        : m_ptr(other.m_ptr)
    {
        if (m_ptr) {
            m_ptr->ref();
        }
    }
    RefPtr(RefPtr &&other)
        : m_ptr(std::exchange(other.m_ptr, nullptr))
    {
    }
    ~RefPtr()
    {
        if (m_ptr) {
            m_ptr->unref();
        }
    }

    T &operator*() const
    {
        return *m_ptr;
    }

    T *operator->() const
    {
        return m_ptr;
    }

    bool operator!() const
    {
        return m_ptr;
    }

    operator bool() const
    {
        return m_ptr;
    }

    T *get() const
    {
        return m_ptr;
    }

    RefPtr &operator=(T *value)
    {
        reset(value);
        return *this;
    }

    RefPtr &operator=(const RefPtr &other)
    {
        reset(other.m_ptr);
        return *this;
    }

    RefPtr &operator=(RefPtr &&other)
    {
        if (m_ptr) {
            m_ptr->unref();
        }
        m_ptr = std::exchange(other.m_ptr, nullptr);
        return *this;
    }

    void reset(T *value = nullptr)
    {
        if (m_ptr != value) {
            if (value) {
                value->ref();
            }
            if (m_ptr) {
                m_ptr->unref();
            }
            m_ptr = value;
        }
    }

private:
    T *m_ptr;
};

template<typename A, typename B>
inline bool operator==(const RefPtr<A> &a, const RefPtr<B> &b)
{
    return a.get() == b.get();
}

template<typename A, typename B>
inline bool operator!=(const RefPtr<A> &a, const RefPtr<B> &b)
{
    return a.get() != b.get();
}

template<typename T, typename B>
inline bool operator==(T *a, const RefPtr<B> &b)
{
    return a == b.get();
}

template<typename T, typename B>
inline bool operator!=(T *a, const RefPtr<B> &b)
{
    return a != b.get();
}

template<typename A, typename T>
inline bool operator==(const RefPtr<A> &a, T *b)
{
    return a.get() == b;
}

template<typename A, typename T>
inline bool operator!=(const RefPtr<A> &a, T *b)
{
    return a.get() != b;
}
