/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QByteArray>
#include <QList>
#include <QSize>
#include <QVector2D>

namespace KWin
{

/**
 * Helper class that can be used for parsing EDID blobs.
 *
 * http://read.pudn.com/downloads110/ebook/456020/E-EDID%20Standard.pdf
 */
class KWIN_EXPORT Edid
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

    /**
     * Returns the raw edid
     */
    QByteArray raw() const;

    /**
     * returns the vendor if included, the EISA ID if not
     */
    QString manufacturerString() const;

    /**
     * returns a string representing the monitor name
     * Can be a serial number or "unknown" if the name is empty
     */
    QString nameString() const;

    QString hash() const;

    struct Colorimetry
    {
        QVector2D redPrimary;
        QVector2D greenPrimary;
        QVector2D bluePrimary;
        QVector2D whitePoint;
    };
    Colorimetry colorimetry() const;

    struct HDRMetadata
    {
        float desiredContentMinLuminance;
        float desiredContentMaxLuminance;
        float desiredMaxFrameAverageLuminance;
        bool supportsPQ;
        bool supportsBT2020;
    };
    std::optional<HDRMetadata> hdrMetadata() const;

private:
    QSize m_physicalSize;
    QByteArray m_vendor;
    QByteArray m_eisaId;
    QByteArray m_monitorName;
    QByteArray m_serialNumber;
    QString m_hash;
    Colorimetry m_colorimetry;
    std::optional<HDRMetadata> m_hdrMetadata;

    QByteArray m_raw;
    bool m_isValid = false;
};

} // namespace KWin
