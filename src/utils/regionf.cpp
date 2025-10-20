/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "regionf.h"

#include <cmath>

namespace KWin
{

RegionF::RegionF(const QRectF &rect)
    : m_rects({rect})
{
}

RegionF::RegionF(const QRect &rect)
    : RegionF(QRectF(rect))
{
}

RegionF::RegionF(const QRegion &region)
{
    for (const QRect &r : region) {
        add(r);
    }
}

RegionF::RegionF(const RegionF &region)
    : m_rects(region.m_rects)
{
}

RegionF::RegionF(double x, double y, double w, double h)
    : RegionF(QRectF(x, y, w, h))
{
}

void RegionF::add(const QRectF &rect)
{
    m_rects.push_back(rect);
}

RegionF RegionF::translated(const QPointF &point) const
{
    RegionF ret = *this;
    ret.translate(point);
    return ret;
}

RegionF RegionF::translated(double x, double y) const
{
    return translated(QPointF(x, y));
}

void RegionF::translate(const QPointF &point)
{
    for (QRectF &rect : *this) {
        rect.translate(point);
    }
}

void RegionF::operator=(const QRegion &region)
{
    m_rects.clear();
    for (const auto &rect : region) {
        m_rects.push_back(rect);
    }
}

void RegionF::operator=(const RegionF &region)
{
    m_rects = region.m_rects;
}

RegionF RegionF::operator&(const QRectF &bounds) const
{
    RegionF ret;
    for (const QRectF &rect : m_rects) {
        QRectF bounded = rect & bounds;
        if (!bounded.isEmpty()) {
            ret.add(bounded);
        }
    }
    return ret;
}

RegionF RegionF::operator&(const QRect &rect) const
{
    return operator&(QRectF(rect));
}

RegionF RegionF::operator&(const QRegion &region) const
{
    RegionF ret;
    for (const QRect &rect : region) {
        ret |= *this & QRectF(rect);
    }
    return ret;
}

RegionF RegionF::operator&(const RegionF &region) const
{
    RegionF ret;
    for (const QRectF &rect : region) {
        ret |= *this & rect;
    }
    return ret;
}

RegionF RegionF::operator|(const RegionF &region) const
{
    RegionF ret = *this;
    ret |= region;
    return ret;
}

RegionF RegionF::operator|(const QRegion &region) const
{
    RegionF ret = *this;
    ret |= region;
    return ret;
}

void RegionF::operator|=(const RegionF &region)
{
    m_rects.append(region.m_rects);
}

void RegionF::operator|=(const QRegion &region)
{
    for (const QRect &rect : region) {
        add(rect);
    }
}

void RegionF::operator|=(const QRectF &rect)
{
    add(rect);
}

void RegionF::operator+=(const QRectF &rect)
{
    add(rect);
}

void RegionF::operator&=(const QRectF &rect)
{
    for (auto it = m_rects.begin(); it != m_rects.end();) {
        QRectF newRect = *it & rect;
        if (newRect.isEmpty()) {
            it = m_rects.erase(it);
        } else {
            *it = newRect;
            it++;
        }
    }
}

void RegionF::operator&=(const RegionF &region)
{
    *this = *this & region;
}

bool RegionF::operator==(const RegionF &region) const
{
    return region.m_rects == m_rects;
}

bool RegionF::operator==(const QRectF &rect) const
{
    return m_rects.size() == 1 && m_rects.front() == rect;
}

bool RegionF::operator==(const QRect &rect) const
{
    return m_rects.size() == 1 && m_rects.front() == rect;
}

RegionF RegionF::operator*(double scale) const
{
    RegionF ret;
    for (const QRectF &r : *this) {
        ret |= QRectF(r.x() * scale, r.y() * scale, r.width() * scale, r.height() * scale);
    }
    return ret;
}

RegionF RegionF::operator/(double scale) const
{
    RegionF ret;
    for (const QRectF &r : *this) {
        ret |= QRectF(r.x() / scale, r.y() / scale, r.width() / scale, r.height() / scale);
    }
    return ret;
}

QVector<QRectF> RegionF::rects() const
{
    return m_rects;
}

bool RegionF::isEmpty() const
{
    return m_rects.empty();
}

bool RegionF::intersects(const RegionF &region) const
{
    return std::any_of(begin(), end(), [region](const QRectF &rect) {
        return region.intersects(rect);
    });
}

bool RegionF::intersects(const QRegion &region) const
{
    return std::any_of(begin(), end(), [region](const QRectF &rect) {
        return std::any_of(region.begin(), region.end(), [rect](const QRect &r2) {
            return rect.intersects(r2);
        });
    });
}

bool RegionF::intersects(const QRectF &rect) const
{
    return std::any_of(begin(), end(), [rect](const QRectF &r) {
        return r.intersects(rect);
    });
}

bool RegionF::contains(const QPointF &point) const
{
    return std::any_of(begin(), end(), [point](const QRectF &r) {
        return r.contains(point);
    });
}

bool RegionF::exclusiveContains(const QPointF &point) const
{
    return std::any_of(begin(), end(), [point](const QRectF &r) {
        return point.x() >= r.x() && point.y() >= r.y() && point.x() < (r.x() + r.width()) && point.y() < (r.y() + r.height());
    });
}

bool RegionF::contains(const RegionF &region) const
{
    // if region is entirely contained within this, the AND operation will
    // not clip any parts of rectangles away, and the area will stay the same
    const RegionF andRegion = *this & region;
    double area = 0;
    for (const QRectF &r : region) {
        area += r.width() * r.height();
    }
    double newArea = 0;
    for (const QRectF &r : andRegion) {
        newArea += r.width() * r.height();
    }
    return area == newArea;
}

QRectF RegionF::boundingRect() const
{
    QRectF ret;
    for (const QRectF &rect : m_rects) {
        if (rect.left() < ret.left()) {
            ret.setLeft(rect.left());
        }
        if (rect.right() > ret.right()) {
            ret.setRight(rect.right());
        }
        if (rect.top() < ret.top()) {
            ret.setTop(rect.top());
        }
        if (rect.bottom() > ret.bottom()) {
            ret.setBottom(rect.bottom());
        }
    }
    return ret;
}

QVector<QRectF>::iterator RegionF::begin()
{
    return m_rects.begin();
}

QVector<QRectF>::iterator RegionF::end()
{
    return m_rects.end();
}

QVector<QRectF>::const_iterator RegionF::begin() const
{
    return m_rects.begin();
}

QVector<QRectF>::const_iterator RegionF::end() const
{
    return m_rects.end();
}

QRegion RegionF::containedAlignedRegion() const
{
    QRegion ret;
    for (const QRectF &r : *this) {
        ret |= QRect(std::ceil(r.x()), std::ceil(r.y()), std::floor(r.width()), std::floor(r.height()));
    }
    return ret;
}

QRegion RegionF::toAlignedRegion() const
{
    QRegion ret;
    for (const QRectF &r : *this) {
        ret |= r.toAlignedRect();
    }
    return ret;
}
}
