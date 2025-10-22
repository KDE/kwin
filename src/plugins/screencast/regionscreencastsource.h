/*
    SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/rect.h"
#include "screencastsource.h"

namespace KWin
{

class FilteredSceneView;
class ItemTreeView;
class RegionScreenCastSource;
class ScreencastLayer;

class RegionScreenCastSource : public ScreenCastSource
{
    Q_OBJECT

public:
    explicit RegionScreenCastSource(const Rect &region, qreal scale, std::optional<pid_t> pidToHide);
    ~RegionScreenCastSource() override;

    quint32 drmFormat() const override;
    QSize textureSize() const override;
    qreal devicePixelRatio() const override;
    uint refreshRate() const override;

    void setRenderCursor(bool enable) override;
    void setColor(const std::shared_ptr<ColorDescription> &color) override;
    Region render(GLFramebuffer *target, const Region &bufferRepair) override;
    Region render(QImage *target, const Region &bufferRepair) override;
    std::chrono::nanoseconds clock() const override;

    void close();
    void pause() override;
    void resume() override;

    bool includesCursor(Cursor *cursor) const override;

    QPointF mapFromGlobal(const QPointF &point) const override;
    RectF mapFromGlobal(const RectF &rect) const override;

private:
    const Rect m_region;
    const qreal m_scale;
    const std::optional<pid_t> m_pidToHide;
    std::chrono::nanoseconds m_last{0};
    bool m_closed = false;
    bool m_active = false;
    bool m_renderCursor = false;

    std::unique_ptr<ScreencastLayer> m_layer;
    std::unique_ptr<FilteredSceneView> m_sceneView;
    std::unique_ptr<ItemTreeView> m_cursorView;
};

} // namespace KWin
