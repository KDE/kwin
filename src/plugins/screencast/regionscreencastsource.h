/*
    SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "screencastsource.h"

#include "opengl/gltexture.h"
#include "opengl/glutils.h"

namespace KWin
{

class Output;
class RegionScreenCastSource;
class ScreencastLayer;
class SceneView;
class ItemTreeView;

class RegionScreenCastSource : public ScreenCastSource
{
    Q_OBJECT

public:
    explicit RegionScreenCastSource(const QRect &region, qreal scale, QObject *parent = nullptr);
    ~RegionScreenCastSource() override;

    quint32 drmFormat() const override;
    QSize textureSize() const override;
    qreal devicePixelRatio() const override;
    uint refreshRate() const override;

    QRegion render(GLFramebuffer *target, const QRegion &bufferRepair) override;
    QRegion render(QImage *target, const QRegion &bufferRepair) override;
    std::chrono::nanoseconds clock() const override;

    void close();
    void pause() override;
    void resume() override;

    bool includesCursor(Cursor *cursor) const override;

    QPointF mapFromGlobal(const QPointF &point) const override;
    QRectF mapFromGlobal(const QRectF &rect) const override;

private:
    void report();

    const QRect m_region;
    const qreal m_scale;
    std::chrono::nanoseconds m_last;
    bool m_closed = false;
    bool m_active = false;

    std::unique_ptr<ScreencastLayer> m_layer;
    std::unique_ptr<SceneView> m_sceneView;
    std::unique_ptr<ItemTreeView> m_cursorView;
};

} // namespace KWin
