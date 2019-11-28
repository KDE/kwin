/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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
