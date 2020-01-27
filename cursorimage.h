/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2018 Roman Gilg <subdiff@gmail.com>
Copyright (C) 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

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
#ifndef KWIN_CURSOR_IMAGE_H
#define KWIN_CURSOR_IMAGE_H

#include "input.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QPointF>

class QWindow;

namespace KWayland {
namespace Server {
class SurfaceInterface;
}
}

namespace KWin
{
class InputRedirection;
class WaylandCursorTheme;
class CursorShape;

namespace Decoration
{
class DecoratedClientImpl;
}

namespace LibInput
{
class Device;
}

uint32_t qtMouseButtonToButton(Qt::MouseButton button);

class KWIN_EXPORT CursorImage : public QObject
{
    Q_OBJECT
public:
    explicit CursorImage(QObject *parent = nullptr);
    ~CursorImage() override;

    void setEffectsOverrideCursor(Qt::CursorShape shape);
    void removeEffectsOverrideCursor();
    void setWindowSelectionCursor(const QByteArray &shape);
    void removeWindowSelectionCursor();

    QImage image() const;
    QPoint hotSpot() const;

    void setBlockUpdate(bool blockUpdate) {
        m_blockUpdate = blockUpdate;
    }

    void markAsRendered();
    void updateDecoration(Decoration::DecoratedClientImpl* decoration);

    virtual QPointer<KWayland::Server::SurfaceInterface> cursorSurface() = 0;
    virtual QPoint cursorHotspot() = 0;

Q_SIGNALS:
    void changed();

protected:
    void reevaluteSource();
    void update();
    void updateServerCursor();
    void updateDecorationCursor(Decoration::DecoratedClientImpl* decoration);
    void updateDecorationCursorWithShape(CursorShape shape, Decoration::DecoratedClientImpl* decoration);
    void updateMoveResize();
    void updateDrag();
    void updateDragCursor();
    void loadTheme();
    struct Image {
        QImage image;
        QPoint hotSpot;
    };
    void loadThemeCursor(CursorShape shape, Image *image);
    void loadThemeCursor(const QByteArray &shape, Image *image);
    template <typename T>
    void loadThemeCursor(const T &shape, QHash<T, Image> &cursors, Image *image);

    enum class CursorSource {
        LockScreen,
        EffectsOverride,
        MoveResize,
        PointerSurface,
        Decoration,
        DragAndDrop,
        Fallback,
        WindowSelector
    };
    void setSource(CursorSource source);

    CursorSource m_currentSource = CursorSource::Fallback;
    WaylandCursorTheme *m_cursorTheme = nullptr;
    struct {
        QMetaObject::Connection connection;
        QImage image;
        QPoint hotSpot;
    } m_serverCursor;

    Image m_effectsCursor;
    Image m_decorationCursor;
    QMetaObject::Connection m_decorationConnection;
    Image m_fallbackCursor;
    Image m_moveResizeCursor;
    Image m_windowSelectionCursor;
    QHash<CursorShape, Image> m_cursors;
    QHash<QByteArray, Image> m_cursorsByName;
    QElapsedTimer m_surfaceRenderedTimer;
    struct {
        Image cursor;
        QMetaObject::Connection connection;
    } m_drag;
    bool m_blockUpdate = false;
};

}

#endif
