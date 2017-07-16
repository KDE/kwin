/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#pragma once
#include <QDebug>
#include <kwin_export.h>

namespace KWin
{

class KWIN_EXPORT AbstractOpenGLContextAttributeBuilder
{
public:
    virtual ~AbstractOpenGLContextAttributeBuilder() {
    }

    void setVersion(int major, int minor = 0) {
        m_versionRequested = true;
        m_majorVersion = major;
        m_minorVersion = minor;
    }

    bool isVersionRequested() const {
        return m_versionRequested;
    }

    int majorVersion() const {
        return m_majorVersion;
    }

    int minorVersion() const {
        return m_minorVersion;
    }

    void setRobust(bool robust) {
        m_robust = robust;
    }

    bool isRobust() const {
        return m_robust;
    }

    void setForwardCompatible(bool forward) {
        m_forwardCompatible = forward;
    }

    bool isForwardCompatible() const {
        return m_forwardCompatible;
    }

    void setCoreProfile(bool core) {
        m_coreProfile = core;
        if (m_coreProfile) {
            setCompatibilityProfile(false);
        }
    }

    bool isCoreProfile() const {
        return m_coreProfile;
    }

    void setCompatibilityProfile(bool compatibility) {
        m_compatibilityProfile = compatibility;
        if (m_compatibilityProfile) {
            setCoreProfile(false);
        }
    }

    bool isCompatibilityProfile() const {
        return m_compatibilityProfile;
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
};

inline QDebug operator<<(QDebug dbg, const AbstractOpenGLContextAttributeBuilder *attribs)
{
    return attribs->operator<<(dbg);
}

}
