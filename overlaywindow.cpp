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

#include "overlaywindow.h"

#include "kwinglobals.h"

#include "assert.h"

#include <QtCore/QVector>

#include <X11/extensions/shape.h>

namespace KWin {
OverlayWindow::OverlayWindow()
    : m_window(None)
    , m_visible(true)
    , m_shown(false)
{
}

OverlayWindow::~OverlayWindow()
{
}

bool OverlayWindow::create()
{
    assert(m_window == None);
    if (!Extensions::compositeOverlayAvailable())
        return false;
    if (!Extensions::shapeInputAvailable())  // needed in setupOverlay()
        return false;
#ifdef HAVE_XCOMPOSITE_OVERLAY
    m_overlay = XCompositeGetOverlayWindow(display(), rootWindow());
    if (m_overlay == None)
        return false;
    XResizeWindow(display(), m_overlay, displayWidth(), displayHeight());
    return true;
#else
    return false;
#endif
}

void OverlayWindow::setup(Window window)
{
    assert(m_window != None);
    assert(Extensions::shapeInputAvailable());
    XSetWindowBackgroundPixmap(display(), m_window, None);
    m_shape = QRegion();
    setShape(QRect(0, 0, displayWidth(), displayHeight()));
    if (window != None) {
        XSetWindowBackgroundPixmap(display(), window, None);
        XShapeCombineRectangles(display(), window, ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted);
    }
    XSelectInput(display(), m_window, VisibilityChangeMask);
}

void OverlayWindow::show()
{
    assert(m_window != None);
    if (m_shown)
        return;
    XMapSubwindows(display(), m_window);
    XMapWindow(display(), m_window);
    m_shown = true;
}

void OverlayWindow::hide()
{
    assert(m_window != None);
    XUnmapWindow(display(), m_window);
    m_shown = false;
    setShape(QRect(0, 0, displayWidth(), displayHeight()));
}

void OverlayWindow::setShape(const QRegion& reg)
{
    // Avoid setting the same shape again, it causes flicker (apparently it is not a no-op
    // and triggers something).
    if (reg == m_shape)
        return;
    QVector< QRect > rects = reg.rects();
    XRectangle* xrects = new XRectangle[ rects.count()];
    for (int i = 0;
            i < rects.count();
            ++i) {
        xrects[ i ].x = rects[ i ].x();
        xrects[ i ].y = rects[ i ].y();
        xrects[ i ].width = rects[ i ].width();
        xrects[ i ].height = rects[ i ].height();
    }
    XShapeCombineRectangles(display(), m_window, ShapeBounding, 0, 0,
                            xrects, rects.count(), ShapeSet, Unsorted);
    delete[] xrects;
    XShapeCombineRectangles(display(), m_window, ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted);
    m_shape = reg;
}

bool OverlayWindow::isVisible()
{
    return m_visible;
}

void OverlayWindow::setVisibility(bool visible)
{
    m_visible = visible;
}

void OverlayWindow::destroy()
{
    if (m_window == None)
        return;
    // reset the overlay shape
    XRectangle rec = { 0, 0, displayWidth(), displayHeight() };
    XShapeCombineRectangles(display(), m_window, ShapeBounding, 0, 0, &rec, 1, ShapeSet, Unsorted);
    XShapeCombineRectangles(display(), m_window, ShapeInput, 0, 0, &rec, 1, ShapeSet, Unsorted);
#ifdef HAVE_XCOMPOSITE_OVERLAY
    XCompositeReleaseOverlayWindow(display(), m_overlay);
#endif
    m_window = None;
    m_shown = false;
}

Window OverlayWindow::window()
{
    return m_window;
}

} // namespace KWin
