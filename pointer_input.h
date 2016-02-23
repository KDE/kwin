/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_POINTER_INPUT_H
#define KWIN_POINTER_INPUT_H

#include "input.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QPointF>

class QWindow;

namespace KWin
{

class CursorImage;
class InputRedirection;
class Toplevel;
class WaylandCursorTheme;

namespace Decoration
{
class DecoratedClientImpl;
}

class PointerInputRedirection : public QObject
{
    Q_OBJECT
public:
    explicit PointerInputRedirection(InputRedirection *parent);
    virtual ~PointerInputRedirection();

    void init();

    void update();
    void updateAfterScreenChange();
    bool supportsWarping() const;
    void warp(const QPointF &pos);

    QPointF pos() const {
        return m_pos;
    }
    Qt::MouseButtons buttons() const {
        return m_qtButtons;
    }
    QPointer<Toplevel> window() const {
        return m_window;
    }
    QPointer<Decoration::DecoratedClientImpl> decoration() const {
        return m_decoration;
    }
    QPointer<QWindow> internalWindow() const {
        return m_internalWindow;
    }

    QImage cursorImage() const;
    QPoint cursorHotSpot() const;
    void markCursorAsRendered();
    void setEffectsOverrideCursor(Qt::CursorShape shape);
    void removeEffectsOverrideCursor();

    /**
     * @internal
     */
    void processMotion(const QPointF &pos, uint32_t time);
    /**
     * @internal
     */
    void processButton(uint32_t button, InputRedirection::PointerButtonState state, uint32_t time);
    /**
     * @internal
     */
    void processAxis(InputRedirection::PointerAxis axis, qreal delta, uint32_t time);

Q_SIGNALS:
    void decorationChanged();

private:
    void updatePosition(const QPointF &pos);
    void updateButton(uint32_t button, InputRedirection::PointerButtonState state);
    void updateInternalWindow();
    void updateDecoration(Toplevel *t);
    InputRedirection *m_input;
    CursorImage *m_cursor;
    bool m_inited = false;
    bool m_supportsWarping;
    QPointF m_pos;
    QHash<uint32_t, InputRedirection::PointerButtonState> m_buttons;
    Qt::MouseButtons m_qtButtons;
    /**
     * @brief The Toplevel which currently receives pointer events
     */
    QPointer<Toplevel> m_window;
    /**
     * @brief The Decoration which currently receives pointer events.
     * Decoration belongs to the pointerWindow
     **/
    QPointer<Decoration::DecoratedClientImpl> m_decoration;
    QPointer<QWindow> m_internalWindow;
    QMetaObject::Connection m_windowGeometryConnection;
    QMetaObject::Connection m_internalWindowConnection;
};

class CursorImage : public QObject
{
    Q_OBJECT
public:
    explicit CursorImage(PointerInputRedirection *parent = nullptr);
    virtual ~CursorImage();

    void setEffectsOverrideCursor(Qt::CursorShape shape);
    void removeEffectsOverrideCursor();

    QImage image() const;
    QPoint hotSpot() const;
    void markAsRendered();

Q_SIGNALS:
    void changed();

private:
    void reevaluteSource();
    void update();
    void updateServerCursor();
    void updateDecoration();
    void updateDecorationCursor();
    void loadTheme();
    struct Image {
        QImage image;
        QPoint hotSpot;
    };
    void loadThemeCursor(Qt::CursorShape shape, Image *image);

    enum class CursorSource {
        LockScreen,
        EffectsOverride,
        PointerSurface,
        Decoration,
        Fallback
    };
    void setSource(CursorSource source);

    PointerInputRedirection *m_pointer;
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
    QHash<Qt::CursorShape, Image> m_cursors;
    QElapsedTimer m_surfaceRenderedTimer;
};

}

#endif
