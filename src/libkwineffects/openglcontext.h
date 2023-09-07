/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "libkwineffects/kwinglutils_export.h"
#include "version.h"

#include <stdint.h>
#include <string_view>

#include <QByteArray>
#include <QSet>

namespace KWin
{

class KWINGLUTILS_EXPORT OpenGlContext
{
public:
    explicit OpenGlContext();
    virtual ~OpenGlContext() = default;

    bool hasVersion(const Version &version) const;

    QByteArrayView openglVersionString() const;
    Version openglVersion() const;
    QByteArrayView vendor() const;
    QByteArrayView renderer() const;
    bool isOpenglES() const;
    bool hasOpenglExtension(QByteArrayView name) const;
    bool isSoftwareRenderer() const;
    /**
     * checks whether or not this context supports all the features that KWin requires
     */
    bool checkSupported() const;

protected:
    const QByteArrayView m_versionString;
    const Version m_version;
    const QByteArrayView m_vendor;
    const QByteArrayView m_renderer;
    const bool m_isOpenglES;
    const QSet<QByteArray> m_extensions;
};

}
