/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/outputlayer.h"
#include "screencastsource.h"

#include <QPointer>

namespace KWin
{

class FilteredSceneView;
class ItemTreeView;
class LogicalOutput;
class ScreencastLayer;

class OutputScreenCastSource : public ScreenCastSource
{
    Q_OBJECT

public:
    explicit OutputScreenCastSource(LogicalOutput *output, std::optional<pid_t> pidToHide);
    ~OutputScreenCastSource() override;

    uint refreshRate() const override;
    QSize textureSize() const override;
    qreal devicePixelRatio() const override;
    quint32 drmFormat() const override;

    void setRenderCursor(bool enable) override;
    QRegion render(GLFramebuffer *target, const QRegion &bufferRepair) override;
    QRegion render(QImage *target, const QRegion &bufferRepair) override;
    std::chrono::nanoseconds clock() const override;

    void resume() override;
    void pause() override;

    bool includesCursor(Cursor *cursor) const override;

    QPointF mapFromGlobal(const QPointF &point) const override;
    QRectF mapFromGlobal(const QRectF &rect) const override;

private:
    QPointer<LogicalOutput> m_output;
    std::optional<pid_t> m_pidToHide;
    std::unique_ptr<ScreencastLayer> m_layer;
    std::unique_ptr<FilteredSceneView> m_sceneView;
    std::unique_ptr<ItemTreeView> m_cursorView;
    bool m_active = false;
    bool m_renderCursor = false;
};

} // namespace KWin
