/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "screencastsource.h"
#include "window.h"

#include <QPointer>
#include <QTimer>

namespace KWin
{

class WindowScreenCastSource : public ScreenCastSource
{
    Q_OBJECT

public:
    explicit WindowScreenCastSource(Window *window, QObject *parent = nullptr);
    ~WindowScreenCastSource() override;

    quint32 drmFormat() const override;
    QSize textureSize() const override;
    qreal devicePixelRatio() const override;
    uint refreshRate() const override;

    void setRenderCursor(bool enable) override;
    void setColor(const std::shared_ptr<ColorDescription> &color) override;
    QRegion render(GLFramebuffer *target, const QRegion &bufferDamage) override;
    QRegion render(QImage *target, const QRegion &bufferDamage) override;
    std::chrono::nanoseconds clock() const override;

    void resume() override;
    void pause() override;

    bool includesCursor(Cursor *cursor) const override;

    QPointF mapFromGlobal(const QPointF &point) const override;
    QRectF mapFromGlobal(const QRectF &rect) const override;

private:
    void add(Window *window);
    void watch(Window *window);
    void unwatch(Window *window);
    QRectF boundingRect() const;

    QList<Window *> m_windows;
    bool m_active = false;
    bool m_renderCursor = false;
    std::shared_ptr<ColorDescription> m_color = ColorDescription::sRGB;
};

} // namespace KWin
