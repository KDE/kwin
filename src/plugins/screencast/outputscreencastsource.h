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
class EglContext;

class ScreencastLayer : public OutputLayer
{
public:
    explicit ScreencastLayer(Output *output, EglContext *context);

    void setFramebuffer(GLFramebuffer *buffer);

    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;

private:
    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;

    EglContext *const m_context;
    GLFramebuffer *m_buffer = nullptr;
};

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

    void render(GLFramebuffer *target) override;
    void render(QImage *target) override;
    std::chrono::nanoseconds clock() const override;

    void resume() override;
    void pause() override;

    bool includesCursor(Cursor *cursor) const override;

    QPointF mapFromGlobal(const QPointF &point) const override;
    QRectF mapFromGlobal(const QRectF &rect) const override;

private:
    void report();

    QPointer<Output> m_output;
    std::unique_ptr<ScreencastLayer> m_layer;
    std::unique_ptr<SceneView> m_sceneView;
    bool m_active = false;
};

} // namespace KWin
