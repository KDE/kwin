/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "edid.h"

#include "config-kwin.h"

#include <QFile>
#include <QStandardPaths>

#include <KLocalizedString>

namespace KWin
{

static bool verifyHeader(const uint8_t *data)
{
    if (data[0] != 0x0 || data[7] != 0x0) {
        return false;
    }

    return std::all_of(data + 1, data + 7,
                       [](uint8_t byte) {
                           return byte == 0xff;
                       });
}

static QSize parsePhysicalSize(const uint8_t *data)
{
    // Convert physical size from centimeters to millimeters.
    return QSize(data[0x15], data[0x16]) * 10;
}

static QByteArray parsePnpId(const uint8_t *data)
{
    // Decode PNP ID from three 5 bit words packed into 2 bytes:
    //
    // | Byte |        Bit                    |
    // |      | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
    // ----------------------------------------
    // |  1   | 0)| (4| 3 | 2 | 1 | 0)| (4| 3 |
    // |      | * |    Character 1    | Char 2|
    // ----------------------------------------
    // |  2   | 2 | 1 | 0)| (4| 3 | 2 | 1 | 0)|
    // |      | Character2|      Character 3  |
    // ----------------------------------------
    const uint offset = 0x8;

    char pnpId[4];
    pnpId[0] = 'A' + ((data[offset + 0] >> 2) & 0x1f) - 1;
    pnpId[1] = 'A' + (((data[offset + 0] & 0x3) << 3) | ((data[offset + 1] >> 5) & 0x7)) - 1;
    pnpId[2] = 'A' + (data[offset + 1] & 0x1f) - 1;
    pnpId[3] = '\0';

    return QByteArray(pnpId);
}

static QByteArray parseEisaId(const uint8_t *data)
{
    for (int i = 72; i <= 108; i += 18) {
        // Skip the block if it isn't used as monitor descriptor.
        if (data[i]) {
            continue;
        }
        if (data[i + 1]) {
            continue;
        }

        // We have found the EISA ID, it's stored as ASCII.
        if (data[i + 3] == 0xfe) {
            return QByteArray(reinterpret_cast<const char *>(&data[i + 5]), 13).trimmed();
        }
    }

    // If there isn't an ASCII EISA ID descriptor, try to decode PNP ID
    return parsePnpId(data);
}

static QByteArray parseMonitorName(const uint8_t *data)
{
    for (int i = 72; i <= 108; i += 18) {
        // Skip the block if it isn't used as monitor descriptor.
        if (data[i]) {
            continue;
        }
        if (data[i + 1]) {
            continue;
        }

        // We have found the monitor name, it's stored as ASCII.
        if (data[i + 3] == 0xfc) {
            return QByteArray(reinterpret_cast<const char *>(&data[i + 5]), 13).trimmed();
        }
    }

    return QByteArray();
}

static QByteArray parseSerialNumber(const uint8_t *data)
{
    for (int i = 72; i <= 108; i += 18) {
        // Skip the block if it isn't used as monitor descriptor.
        if (data[i]) {
            continue;
        }
        if (data[i + 1]) {
            continue;
        }

        // We have found the serial number, it's stored as ASCII.
        if (data[i + 3] == 0xff) {
            return QByteArray(reinterpret_cast<const char *>(&data[i + 5]), 13).trimmed();
        }
    }

    // Maybe there isn't an ASCII serial number descriptor, so use this instead.
    const uint32_t offset = 0xc;

    uint32_t serialNumber = data[offset + 0];
    serialNumber |= uint32_t(data[offset + 1]) << 8;
    serialNumber |= uint32_t(data[offset + 2]) << 16;
    serialNumber |= uint32_t(data[offset + 3]) << 24;
    if (serialNumber) {
        return QByteArray::number(serialNumber);
    }

    return QByteArray();
}

static QByteArray parseVendor(const uint8_t *data)
{
    const auto pnpId = parsePnpId(data);

    // Map to vendor name
    QFile pnpFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("hwdata/pnp.ids")));
    if (pnpFile.exists() && pnpFile.open(QIODevice::ReadOnly)) {
        while (!pnpFile.atEnd()) {
            const auto line = pnpFile.readLine();
            if (line.startsWith(pnpId)) {
                return line.mid(4).trimmed();
            }
        }
    }

    return {};
}

Edid::Edid()
{
}

Edid::Edid(const void *data, uint32_t size)
{
    m_raw.resize(size);
    memcpy(m_raw.data(), data, size);

    const uint8_t *bytes = static_cast<const uint8_t *>(data);

    if (size < 128) {
        return;
    }

    if (!verifyHeader(bytes)) {
        return;
    }

    m_physicalSize = parsePhysicalSize(bytes);
    m_eisaId = parseEisaId(bytes);
    m_monitorName = parseMonitorName(bytes);
    m_serialNumber = parseSerialNumber(bytes);
    m_vendor = parseVendor(bytes);

    m_isValid = true;
}

bool Edid::isValid() const
{
    return m_isValid;
}

QSize Edid::physicalSize() const
{
    return m_physicalSize;
}

QByteArray Edid::eisaId() const
{
    return m_eisaId;
}

QByteArray Edid::monitorName() const
{
    return m_monitorName;
}

QByteArray Edid::serialNumber() const
{
    return m_serialNumber;
}

QByteArray Edid::vendor() const
{
    return m_vendor;
}

QByteArray Edid::raw() const
{
    return m_raw;
}

QString Edid::manufacturerString() const
{
    QString manufacturer;
    if (!m_vendor.isEmpty()) {
        manufacturer = QString::fromLatin1(m_vendor);
    } else if (!m_eisaId.isEmpty()) {
        manufacturer = QString::fromLatin1(m_eisaId);
    }
    return manufacturer;
}

QString Edid::nameString() const
{
    if (!m_monitorName.isEmpty()) {
        QString m = QString::fromLatin1(m_monitorName);
        if (!m_serialNumber.isEmpty()) {
            m.append('/');
            m.append(QString::fromLatin1(m_serialNumber));
        }
        return m;
    } else if (!m_serialNumber.isEmpty()) {
        return QString::fromLatin1(m_serialNumber);
    } else {
        return i18n("unknown");
    }
}

} // namespace KWin
