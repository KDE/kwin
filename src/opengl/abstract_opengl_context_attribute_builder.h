/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include <QDebug>
#include <kwin_export.h>

namespace KWin
{

class KWIN_EXPORT AbstractOpenGLContextAttributeBuilder
{
public:
    virtual ~AbstractOpenGLContextAttributeBuilder()
    {
    }

    void setVersion(int major, int minor = 0)
    {
        m_versionRequested = true;
        m_majorVersion = major;
        m_minorVersion = minor;
    }

    bool isVersionRequested() const
    {
        return m_versionRequested;
    }

    int majorVersion() const
    {
        return m_majorVersion;
    }

    int minorVersion() const
    {
        return m_minorVersion;
    }

    void setRobust(bool robust)
    {
        m_robust = robust;
    }

    bool isRobust() const
    {
        return m_robust;
    }

    void setForwardCompatible(bool forward)
    {
        m_forwardCompatible = forward;
    }

    bool isForwardCompatible() const
    {
        return m_forwardCompatible;
    }

    void setCoreProfile(bool core)
    {
        m_coreProfile = core;
        if (m_coreProfile) {
            setCompatibilityProfile(false);
        }
    }

    bool isCoreProfile() const
    {
        return m_coreProfile;
    }

    void setCompatibilityProfile(bool compatibility)
    {
        m_compatibilityProfile = compatibility;
        if (m_compatibilityProfile) {
            setCoreProfile(false);
        }
    }

    bool isCompatibilityProfile() const
    {
        return m_compatibilityProfile;
    }

    void setResetOnVideoMemoryPurge(bool reset)
    {
        m_resetOnVideoMemoryPurge = reset;
    }

    bool isResetOnVideoMemoryPurge() const
    {
        return m_resetOnVideoMemoryPurge;
    }

    void setHighPriority(bool highPriority)
    {
        m_highPriority = highPriority;
    }

    bool isHighPriority() const
    {
        return m_highPriority;
    }

    virtual std::vector<int> build() const = 0;

    QDebug operator<<(QDebug dbg) const;

private:
    bool m_versionRequested = false;
    int m_majorVersion = 0;
    int m_minorVersion = 0;
    bool m_robust = false;
    bool m_forwardCompatible = false;
    bool m_coreProfile = false;
    bool m_compatibilityProfile = false;
    bool m_resetOnVideoMemoryPurge = false;
    bool m_highPriority = false;
};

inline QDebug operator<<(QDebug dbg, const AbstractOpenGLContextAttributeBuilder *attribs)
{
    return attribs->operator<<(dbg);
}

}
