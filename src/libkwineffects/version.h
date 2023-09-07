/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "libkwineffects/kwinglutils_export.h"

#include <QByteArrayView>

namespace KWin
{

class KWINGLUTILS_EXPORT Version
{
public:
    Version(uint32_t major, uint32_t minor, uint32_t patch = 0);
    Version() = default;

    auto operator<=>(const Version &other) const = default;
    bool isValid() const;
    uint32_t major() const;
    uint32_t minor() const;
    uint32_t patch() const;

    static Version parseString(QByteArrayView versionString);

private:
    uint32_t m_major = 0;
    uint32_t m_minor = 0;
    uint32_t m_patch = 0;
};

}
