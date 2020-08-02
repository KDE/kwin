/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QByteArray>
#include <QSize>

namespace KWin
{

/**
 * Helper class that can be used for parsing EDID blobs.
 *
 * http://read.pudn.com/downloads110/ebook/456020/E-EDID%20Standard.pdf
 */
class Edid
{
public:
    Edid();
    Edid(const void *data, uint32_t size);

    /**
     * Whether this instance of EDID is valid.
     */
    bool isValid() const;

    /**
     * Returns physical dimensions of the monitor, in millimeters.
     */
    QSize physicalSize() const;

    /**
     * Returns EISA ID of the manufacturer of the monitor.
     */
    QByteArray eisaId() const;

    /**
     * Returns the product name of the monitor.
     */
    QByteArray monitorName() const;

    /**
     * Returns the serial number of the monitor.
     */
    QByteArray serialNumber() const;

    /**
     * Returns the name of the vendor.
     */
    QByteArray vendor() const;

private:
    QSize m_physicalSize;
    QByteArray m_vendor;
    QByteArray m_eisaId;
    QByteArray m_monitorName;
    QByteArray m_serialNumber;
    bool m_isValid = false;
};

} // namespace KWin
