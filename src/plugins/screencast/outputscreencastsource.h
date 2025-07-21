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

class Output;
class SceneView;
class ItemTreeView;
class EglContext;
class ScreencastLayer;

class OutputScreenCastSource : public ScreenCastSource
{
    Q_OBJECT

public:
    explicit OutputScreenCastSource(Output *output, QObject *parent = nullptr);
    ~OutputScreenCastSource() override;

    uint refreshRate() const override;
    QSize textureSize() const override;
    qreal devicePixelRatio() const override;
    quint32 drmFormat() const override;

    QRegion render(GLFramebuffer *target, const QRegion &bufferRepair) override;
    QRegion render(QImage *target, const QRegion &bufferRepair) override;
    std::chrono::nanoseconds clock() const override;

    void resume() override;
    void pause() override;

    bool includesCursor(Cursor *cursor) const override;

    QPointF mapFromGlobal(const QPointF &point) const override;
    QRectF mapFromGlobal(const QRectF &rect) const override;

private:
    void updateView();
    void report();

    QPointer<Output> m_output;
    std::unique_ptr<ScreencastLayer> m_layer;
    std::unique_ptr<SceneView> m_sceneView;
    std::unique_ptr<ItemTreeView> m_cursorView;
    bool m_active = false;
};

} // namespace KWin
