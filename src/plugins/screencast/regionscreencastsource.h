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

class RegionScreenCastScrapper : public QObject
{
    Q_OBJECT

public:
    explicit RegionScreenCastScrapper(RegionScreenCastSource *source, Output *output);

private:
    RegionScreenCastSource *m_source;
    Output *m_output;
};

class RegionScreenCastSource : public ScreenCastSource
{
    Q_OBJECT

public:
    explicit RegionScreenCastSource(const QRect &region, qreal scale, QObject *parent = nullptr);
    ~RegionScreenCastSource() override;

    quint32 drmFormat() const override;
    QSize textureSize() const override;
    uint refreshRate() const override;

    void render(GLFramebuffer *target) override;
    void render(QImage *target) override;
    std::chrono::nanoseconds clock() const override;

    void update(Output *output, const QRegion &damage);
    void close();
    void pause() override;
    void resume() override;

    bool includesCursor(Cursor *cursor) const override;

    QPointF mapFromGlobal(const QPointF &point) const override;
    QRectF mapFromGlobal(const QRectF &rect) const override;

private:
    void blit(Output *output);
    void ensureTexture();

    const QRect m_region;
    const qreal m_scale;
    std::vector<std::unique_ptr<RegionScreenCastScrapper>> m_scrappers;
    std::unique_ptr<GLFramebuffer> m_target;
    std::unique_ptr<GLTexture> m_renderedTexture;
    std::chrono::nanoseconds m_last;
    bool m_closed = false;
    bool m_active = false;
};

} // namespace KWin
