/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_DECORATION_RENDERER_H
#define KWIN_DECORATION_RENDERER_H

#include <QObject>
#include <QRegion>

#include <kwin_export.h>

namespace KWin
{

class Deleted;

namespace Decoration
{

class DecoratedClientImpl;

class KWIN_EXPORT Renderer : public QObject
{
    Q_OBJECT
public:
    ~Renderer() override;

    void schedule(const QRect &rect);

    /**
     * Reparents this Renderer to the @p deleted.
     * After this call the Renderer is no longer able to render
     * anything, client() returns a nullptr.
     */
    virtual void reparent(Deleted *deleted);

Q_SIGNALS:
    void renderScheduled(const QRect &geo);

protected:
    explicit Renderer(DecoratedClientImpl *client);
    /**
     * @returns the scheduled paint region and resets
     */
    QRegion getScheduled();

    virtual void render() = 0;

    DecoratedClientImpl *client() {
        return m_client;
    }

    bool areImageSizesDirty() const {
        return m_imageSizesDirty;
    }
    void resetImageSizesDirty() {
        m_imageSizesDirty = false;
    }
    QImage renderToImage(const QRect &geo);
    void renderToPainter(QPainter *painter, const QRect &rect);

private:
    DecoratedClientImpl *m_client;
    QRegion m_scheduled;
    bool m_imageSizesDirty;
};

}
}

#endif
