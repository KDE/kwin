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
class ColorDescription;

class ScreenCastSource : public QObject
{
    Q_OBJECT

public:
    explicit ScreenCastSource(QObject *parent = nullptr);

    virtual uint refreshRate() const = 0;
    virtual quint32 drmFormat() const = 0;
    virtual QSize textureSize() const = 0;
    virtual qreal devicePixelRatio() const = 0;

    virtual void setRenderCursor(bool enable) = 0;
    virtual void setColor(const std::shared_ptr<ColorDescription> &color) = 0;
    virtual QRegion render(GLFramebuffer *target, const QRegion &bufferRepair) = 0;
    virtual QRegion render(QImage *target, const QRegion &bufferRepair) = 0;
    virtual std::chrono::nanoseconds clock() const = 0;

    virtual void resume() = 0;
    virtual void pause() = 0;

    virtual bool includesCursor(Cursor *cursor) const = 0;

    virtual QPointF mapFromGlobal(const QPointF &point) const = 0;
    virtual QRectF mapFromGlobal(const QRectF &rect) const = 0;

Q_SIGNALS:
    void frame();
    void closed();
};

} // namespace KWin
