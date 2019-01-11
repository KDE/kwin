/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_QPA_BACKINGSTORE_H
#define KWIN_QPA_BACKINGSTORE_H

#include <qpa/qplatformbackingstore.h>

namespace KWayland
{
namespace Client
{
class Buffer;
class ShmPool;
}
}

namespace KWin
{
namespace QPA
{

class BackingStore : public QPlatformBackingStore
{
public:
    explicit BackingStore(QWindow *w, KWayland::Client::ShmPool *shm);
    virtual ~BackingStore();

    QPaintDevice *paintDevice() override;
    void flush(QWindow *window, const QRegion &region, const QPoint &offset) override;
    void resize(const QSize &size, const QRegion &staticContents) override;
    void beginPaint(const QRegion &) override;

private:
    int scale() const;
    KWayland::Client::ShmPool *m_shm;
    QWeakPointer<KWayland::Client::Buffer> m_buffer;
    QImage m_backBuffer;
    QSize m_size;
};

}
}

#endif
