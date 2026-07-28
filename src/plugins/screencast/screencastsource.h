/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

class QImage;

namespace KWin
{

class Cursor;
class GLFramebuffer;
class GLTexture;
class RectF;
class Region;
class RenderDevice;
class EglContext;

class ScreenCastSource : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCastSource();
    ~ScreenCastSource() override;

    virtual uint refreshRate() const = 0;
    virtual quint32 drmFormat() const = 0;
    virtual QSize textureSize() const = 0;
    virtual qreal devicePixelRatio() const = 0;

    virtual void setRenderCursor(bool enable) = 0;
    virtual Region render(GLFramebuffer *target, const Region &bufferRepair) = 0;
    virtual Region render(QImage *target, const Region &bufferRepair) = 0;

    virtual void resume() = 0;
    virtual void pause() = 0;

    virtual bool includesCursor(Cursor *cursor) const = 0;

    virtual QPointF mapFromGlobal(const QPointF &point) const = 0;
    virtual RectF mapFromGlobal(const RectF &rect) const = 0;
    virtual bool followsStreamSize();
    virtual void resize(const QSize &size);

    RenderDevice *renderDevice() const;
    const std::shared_ptr<EglContext> &eglContext() const;

Q_SIGNALS:
    void frame();
    void closed();

protected:
    RenderDevice *const m_renderDevice;
    std::shared_ptr<EglContext> m_eglContext;
};

} // namespace KWin
