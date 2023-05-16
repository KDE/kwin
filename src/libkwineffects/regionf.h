/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once
#include <QDebug>
#include <QMetaType>
#include <QRectF>
#include <QRegion>
#include <QVector>

#include "kwin_export.h"

namespace KWin
{

class KWIN_EXPORT RegionF
{
public:
    RegionF() = default;
    RegionF(const QRect &rect);
    RegionF(const QRectF &rect);
    RegionF(const QRegion &region);
    RegionF(const RegionF &region);

    void add(const QRectF &rect);
    RegionF translated(const QPointF &point) const;
    RegionF translated(double x, double y) const;
    void translate(const QPointF &point);
    void clear();

    void operator=(const QRegion &region);
    void operator=(const RegionF &region);
    RegionF operator&(const QRect &rect) const;
    RegionF operator&(const QRectF &rect) const;
    RegionF operator&(const QRegion &region) const;
    RegionF operator&(const RegionF &region) const;
    RegionF operator|(const RegionF &region) const;
    RegionF operator|(const QRegion &region) const;
    RegionF operator-(const QRectF &rect) const;
    RegionF operator-(const RegionF &region) const;
    void operator-=(const QRectF &rect);
    void operator-=(const RegionF &region);
    void operator|=(const RegionF &region);
    void operator|=(const QRegion &region);
    void operator|=(const QRectF &rect);
    void operator+=(const QRectF &rect);
    void operator&=(const QRectF &rect);
    void operator&=(const RegionF &region);
    bool operator==(const RegionF &region) const;
    bool operator==(const QRectF &rect) const;
    bool operator==(const QRect &rect) const;

    RegionF operator*(double scale) const;
    RegionF operator/(double scale) const;

    QVector<QRectF> rects() const;
    int rectCount() const;
    bool isEmpty() const;
    bool intersects(const RegionF &region) const;
    bool intersects(const QRegion &region) const;
    bool intersects(const QRectF &rect) const;
    bool contains(const QPointF &point) const;
    bool contains(const RegionF &region) const;
    QRectF boundingRect() const;

    QVector<QRectF>::iterator begin();
    QVector<QRectF>::iterator end();
    QVector<QRectF>::const_iterator begin() const;
    QVector<QRectF>::const_iterator end() const;

    /**
     * @returns the largest aligned region that's entirely
     *          contained within this region
     */
    QRegion containedAlignedRegion(double scale = 1) const;
    QRegion toAlignedRegion(double scale = 1) const;
    QRegion round(double scale = 1) const;

    bool isInfinite() const;
    static RegionF infiniteRegion();

private:
    QVector<QRectF> m_rects;
};

}

inline static QDebug operator<<(QDebug stream, const KWin::RegionF &region)
{
    return stream << region.rects();
}
Q_DECLARE_METATYPE(KWin::RegionF)
