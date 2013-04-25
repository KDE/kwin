/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>

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

#ifndef KWIN_OUTLINE_H
#define KWIN_OUTLINE_H
#include "xcbutils.h"
#include <kwinglobals.h>
#include <QRect>
#include <QWidget>

namespace Plasma {
class FrameSvg;
}

namespace KWin {
class OutlineVisual;

/**
 * @short This class is used to show the outline of a given geometry.
 *
 * The class renders an outline by using four windows. One for each border of
 * the geometry. It is possible to replace the outline with an effect. If an
 * effect is available the effect will be used, otherwise the outline will be
 * rendered by using the X implementation.
 *
 * @author Arthur Arlt
 * @since 4.7
 */
class Outline : public QObject {
    Q_OBJECT
public:
    ~Outline();

    /**
     * Set the outline geometry.
     * To show the outline use @link showOutline.
     * @param outlineGeometry The geometry of the outline to be shown
     * @see showOutline
     */
    void setGeometry(const QRect &outlineGeometry);

    /**
     * Shows the outline of a window using either an effect or the X implementation.
     * To stop the outline process use @link hideOutline.
     * @see hideOutline
     */
    void show();

    /**
     * Shows the outline for the given @p outlineGeometry.
     * This is the same as setOutlineGeometry followed by showOutline directly.
     * To stop the outline process use @link hideOutline.
     * @param outlineGeometry The geometry of the outline to be shown
     * @see hideOutline
     */
    void show(const QRect &outlineGeometry);

    /**
     * Hides shown outline.
     * @see showOutline
     */
    void hide();

    const QRect &geometry() const;

private Q_SLOTS:
    void compositingChanged();

private:
    void createHelper();
    QScopedPointer<OutlineVisual> m_visual;
    QRect m_outlineGeometry;
    bool m_active;
    KWIN_SINGLETON(Outline)
};

class OutlineVisual
{
public:
    OutlineVisual(Outline *outline);
    virtual ~OutlineVisual();
    virtual void show() = 0;
    virtual void hide() = 0;
protected:
    Outline *outline();
    const Outline *outline() const;
private:
    Outline *m_outline;
};

class CompositedOutlineVisual : public QWidget, public OutlineVisual
{
public:
    CompositedOutlineVisual(Outline *outline);
    virtual ~CompositedOutlineVisual();
    virtual void show();
    virtual void hide();
protected:
    virtual void paintEvent(QPaintEvent *);
private:
    Plasma::FrameSvg *m_background;
};

class NonCompositedOutlineVisual : public OutlineVisual
{
public:
    NonCompositedOutlineVisual(Outline *outline);
    virtual ~NonCompositedOutlineVisual();
    virtual void show();
    virtual void hide();

private:
    // TODO: variadic template arguments for adding method arguments
    template <typename T>
    void forEachWindow(T method);
    bool m_initialized;
    Xcb::Window m_topOutline;
    Xcb::Window m_rightOutline;
    Xcb::Window m_bottomOutline;
    Xcb::Window m_leftOutline;
};

inline
const QRect &Outline::geometry() const
{
    return m_outlineGeometry;
}

inline
Outline *OutlineVisual::outline()
{
    return m_outline;
}

inline
const Outline *OutlineVisual::outline() const
{
    return m_outline;
}

template <typename T>
inline
void NonCompositedOutlineVisual::forEachWindow(T method)
{
    (m_topOutline.*method)();
    (m_rightOutline.*method)();
    (m_bottomOutline.*method)();
    (m_leftOutline.*method)();
}

inline
Outline *outline()
{
    return Outline::self();
}

}

#endif
