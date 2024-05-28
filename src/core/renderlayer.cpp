/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderlayer.h"
#include "outputlayer.h"
#include "renderlayerdelegate.h"
#include "renderloop.h"

namespace KWin
{

RenderLayer::RenderLayer(RenderLoop *loop, RenderLayer *superlayer)
    : m_loop(loop)
{
    setSuperlayer(superlayer);
}

RenderLayer::~RenderLayer()
{
    const auto sublayers = m_sublayers;
    for (RenderLayer *sublayer : sublayers) {
        sublayer->setSuperlayer(superlayer());
    }
    setSuperlayer(nullptr);
}

OutputLayer *RenderLayer::outputLayer() const
{
    return m_outputLayer;
}

void RenderLayer::setOutputLayer(OutputLayer *layer)
{
    if (m_outputLayer == layer) {
        return;
    }
    if (m_outputLayer) {
        m_outputLayer->addRepaint(mapToGlobal(boundingRect()).toAlignedRect());
    }
    m_outputLayer = layer;
    for (RenderLayer *sublayer : std::as_const(m_sublayers)) {
        sublayer->setOutputLayer(layer);
    }
}

RenderLayer *RenderLayer::superlayer() const
{
    return m_superlayer;
}

void RenderLayer::setSuperlayer(RenderLayer *layer)
{
    if (m_superlayer == layer) {
        return;
    }
    if (m_superlayer) {
        m_superlayer->removeSublayer(this);
    }
    m_superlayer = layer;
    if (m_superlayer) {
        m_superlayer->addSublayer(this);
    }
    updateEffectiveVisibility();
}

QList<RenderLayer *> RenderLayer::sublayers() const
{
    return m_sublayers;
}

void RenderLayer::addSublayer(RenderLayer *sublayer)
{
    m_sublayers.append(sublayer);
    sublayer->setOutputLayer(m_outputLayer);
    updateBoundingRect();
}

void RenderLayer::removeSublayer(RenderLayer *sublayer)
{
    m_sublayers.removeOne(sublayer);
    sublayer->setOutputLayer(nullptr);
    updateBoundingRect();
}

RenderLoop *RenderLayer::loop() const
{
    return m_loop;
}

RenderLayerDelegate *RenderLayer::delegate() const
{
    return m_delegate.get();
}

void RenderLayer::setDelegate(std::unique_ptr<RenderLayerDelegate> delegate)
{
    m_delegate = std::move(delegate);
    m_delegate->setLayer(this);
}

QRectF RenderLayer::rect() const
{
    return QRectF(0, 0, m_geometry.width(), m_geometry.height());
}

QRectF RenderLayer::boundingRect() const
{
    return m_boundingRect;
}

QRectF RenderLayer::geometry() const
{
    return m_geometry;
}

void RenderLayer::setGeometry(const QRectF &geometry)
{
    if (m_geometry == geometry) {
        return;
    }
    if (m_effectiveVisible && m_outputLayer) {
        m_outputLayer->addRepaint(mapToGlobal(boundingRect()).toAlignedRect());
    }

    m_geometry = geometry;
    addRepaintFull();

    updateBoundingRect();
    if (m_superlayer) {
        m_superlayer->updateBoundingRect();
    }
}

void RenderLayer::scheduleRepaint(Item *item)
{
    m_repaintScheduled = true;
    m_loop->scheduleRepaint(item);
}

bool RenderLayer::needsRepaint() const
{
    if (m_repaintScheduled) {
        return true;
    }
    if (!m_repaints.isEmpty()) {
        return true;
    }
    return std::any_of(m_sublayers.constBegin(), m_sublayers.constEnd(), [](RenderLayer *layer) {
        return layer->needsRepaint();
    });
}

void RenderLayer::updateBoundingRect()
{
    QRectF boundingRect = rect();
    for (const RenderLayer *sublayer : std::as_const(m_sublayers)) {
        boundingRect |= sublayer->boundingRect().translated(sublayer->geometry().topLeft());
    }

    if (m_boundingRect != boundingRect) {
        m_boundingRect = boundingRect;
        if (m_superlayer) {
            m_superlayer->updateBoundingRect();
        }
    }
}

void RenderLayer::addRepaintFull()
{
    addRepaint(rect().toAlignedRect());
}

void RenderLayer::addRepaint(int x, int y, int width, int height)
{
    addRepaint(QRegion(x, y, width, height));
}

void RenderLayer::addRepaint(const QRect &rect)
{
    addRepaint(QRegion(rect));
}

void RenderLayer::addRepaint(const QRegion &region)
{
    if (!m_effectiveVisible) {
        return;
    }
    if (!region.isEmpty()) {
        m_repaints += region;
        m_loop->scheduleRepaint(nullptr, this);
    }
}

QRegion RenderLayer::repaints() const
{
    return m_repaints;
}

void RenderLayer::resetRepaints()
{
    m_repaints = QRegion();
    m_repaintScheduled = false;
}

bool RenderLayer::isVisible() const
{
    return m_effectiveVisible;
}

void RenderLayer::setVisible(bool visible)
{
    if (m_explicitVisible != visible) {
        m_explicitVisible = visible;
        updateEffectiveVisibility();
    }
}

bool RenderLayer::computeEffectiveVisibility() const
{
    return m_explicitVisible && (!m_superlayer || m_superlayer->isVisible());
}

void RenderLayer::updateEffectiveVisibility()
{
    const bool effectiveVisible = computeEffectiveVisibility();
    if (m_effectiveVisible == effectiveVisible) {
        return;
    }

    m_effectiveVisible = effectiveVisible;

    if (effectiveVisible) {
        addRepaintFull();
    } else {
        if (m_outputLayer) {
            m_outputLayer->addRepaint(mapToGlobal(boundingRect()).toAlignedRect());
            m_loop->scheduleRepaint(nullptr, this);
        }
    }

    for (RenderLayer *sublayer : std::as_const(m_sublayers)) {
        sublayer->updateEffectiveVisibility();
    }
}

QPoint RenderLayer::mapToGlobal(const QPoint &point) const
{
    return mapToGlobal(QPointF(point)).toPoint();
}

QPointF RenderLayer::mapToGlobal(const QPointF &point) const
{
    QPointF result = point;
    const RenderLayer *layer = this;
    while (layer) {
        result += layer->geometry().topLeft();
        layer = layer->superlayer();
    }
    return result;
}

QRect RenderLayer::mapToGlobal(const QRect &rect) const
{
    return rect.translated(mapToGlobal(QPoint(0, 0)));
}

QRectF RenderLayer::mapToGlobal(const QRectF &rect) const
{
    return rect.translated(mapToGlobal(QPointF(0, 0)));
}

QRegion RenderLayer::mapToGlobal(const QRegion &region) const
{
    if (region.isEmpty()) {
        return QRegion();
    }
    return region.translated(mapToGlobal(QPoint(0, 0)));
}

QPoint RenderLayer::mapFromGlobal(const QPoint &point) const
{
    return mapFromGlobal(QPointF(point)).toPoint();
}

QPointF RenderLayer::mapFromGlobal(const QPointF &point) const
{
    QPointF result = point;
    const RenderLayer *layer = this;
    while (layer) {
        result -= layer->geometry().topLeft();
        layer = layer->superlayer();
    }
    return result;
}

QRect RenderLayer::mapFromGlobal(const QRect &rect) const
{
    return rect.translated(mapFromGlobal(QPoint(0, 0)));
}

QRectF RenderLayer::mapFromGlobal(const QRectF &rect) const
{
    return rect.translated(mapFromGlobal(QPointF(0, 0)));
}

QRegion RenderLayer::mapFromGlobal(const QRegion &region) const
{
    if (region.isEmpty()) {
        return QRegion();
    }
    return region.translated(mapFromGlobal(QPoint(0, 0)));
}

} // namespace KWin

#include "moc_renderlayer.cpp"
