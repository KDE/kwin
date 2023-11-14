/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"

#include <QByteArray>
#include <QString>

namespace KWin
{

class KWIN_EXPORT Version
{
public:
    Version(uint32_t major, uint32_t minor, uint32_t patch = 0);
    Version() = default;

    // clang-format off
    auto operator<=> (const Version &other) const = default;
    // clang-format on
    bool isValid() const;
    uint32_t majorVersion() const;
    uint32_t minorVersion() const;
    uint32_t patchVersion() const;

    QString toString() const;
    QByteArray toByteArray() const;

    static Version parseString(QByteArrayView versionString);

private:
    uint32_t m_major = 0;
    uint32_t m_minor = 0;
    uint32_t m_patch = 0;
};

}
