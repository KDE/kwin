/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "version.h"

#include <QChar>
#include <QList>

namespace KWin
{

Version::Version(uint32_t major, uint32_t minor, uint32_t patch)
    : m_major(major)
    , m_minor(minor)
    , m_patch(patch)
{
}

bool Version::isValid() const
{
    return m_major > 0 || m_minor > 0 || m_patch > 0;
}

uint32_t Version::major() const
{
    return m_major;
}

uint32_t Version::minor() const
{
    return m_minor;
}

uint32_t Version::patch() const
{
    return m_patch;
}

Version Version::parseString(QByteArrayView versionString)
{
    // Skip any leading non digit
    int start = 0;
    while (start < versionString.length() && !QChar::fromLatin1(versionString[start]).isDigit()) {
        start++;
    }

    // Strip any non digit, non '.' characters from the end
    int end = start;
    while (end < versionString.length() && (versionString[end] == '.' || QChar::fromLatin1(versionString[end]).isDigit())) {
        end++;
    }

    const QByteArray result = versionString.toByteArray().mid(start, end - start);
    const QList<QByteArray> tokens = result.split('.');
    if (tokens.empty()) {
        return Version(0, 0, 0);
    }
    const uint64_t major = tokens.at(0).toInt();
    const uint64_t minor = tokens.count() > 1 ? tokens.at(1).toInt() : 0;
    const uint64_t patch = tokens.count() > 2 ? tokens.at(2).toInt() : 0;

    return Version(major, minor, patch);
}

QString Version::toString() const
{
    if (m_patch == 0) {
        return QString::number(m_major) + '.' + QString::number(m_minor);
    } else {
        return QString::number(m_major) + '.' + QString::number(m_minor) + '.' + QString::number(m_patch);
    }
}

QByteArray Version::toByteArray() const
{
    if (m_patch == 0) {
        return QByteArray::number(m_major) + '.' + QByteArray::number(m_minor);
    } else {
        return QByteArray::number(m_major) + '.' + QByteArray::number(m_minor) + '.' + QByteArray::number(m_patch);
    }
}
}
