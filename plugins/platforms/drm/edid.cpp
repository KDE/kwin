/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Flöser <mgraesslin@kde.org>
Copyright (C) 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

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

#include "edid.h"

namespace KWin
{

static bool verifyHeader(const uint8_t *data)
{
    if (data[0] != 0x0 || data[7] != 0x0) {
        return false;
    }

    return std::all_of(data + 1, data + 7,
        [](uint8_t byte) { return byte == 0xff; });
}

static QSize parsePhysicalSize(const uint8_t *data)
{
    // Convert physical size from centimeters to millimeters.
    return QSize(data[0x15], data[0x16]) * 10;
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
            return QByteArray(reinterpret_cast<const char *>(&data[i + 5]), 12).trimmed();
        }
    }

    // If there isn't an ASCII EISA ID descriptor, try to decode PNP ID from
    // three 5 bit words packed into 2 bytes:
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
            return QByteArray(reinterpret_cast<const char *>(&data[i + 5]), 12).trimmed();
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
            return QByteArray(reinterpret_cast<const char *>(&data[i + 5]), 12).trimmed();
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

Edid::Edid()
{
}

Edid::Edid(const void *data, uint32_t size)
{
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

} // namespace KWin
