/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "screencastsource.h"

#include <QPointer>

namespace KWin
{

class Output;

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
    void report(const QRegion &damage);

    QPointer<Output> m_output;
    bool m_active = false;
};

} // namespace KWin
