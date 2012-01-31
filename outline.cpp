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

#include "outline.h"
#include "effects.h"
#include <QtCore/QRect>
#include <QtGui/QX11Info>
#include <QtGui/QPainter>
#include <X11/Xlib.h>

namespace KWin {

Outline::Outline()
    : m_initialized(false)
    , m_active(false)
{
}

Outline::~Outline()
{
    if (m_initialized) {
        XDestroyWindow(QX11Info::display(), m_leftOutline);
        XDestroyWindow(QX11Info::display(), m_rightOutline);
        XDestroyWindow(QX11Info::display(), m_topOutline);
        XDestroyWindow(QX11Info::display(), m_bottomOutline);
    }
}

void Outline::show()
{
    m_active = true;
    if (effects && static_cast<EffectsHandlerImpl*>(effects)->provides(Effect::Outline)) {
        static_cast<EffectsHandlerImpl*>(effects)->slotShowOutline(m_outlineGeometry);
        return; // done by effect
    }
    showWithX();
}

void Outline::hide()
{
    if (!m_active) {
        return;
    }
    m_active = false;
    if (effects && static_cast<EffectsHandlerImpl*>(effects)->provides(Effect::Outline)) {
        static_cast<EffectsHandlerImpl*>(effects)->slotHideOutline();
        return; // done by effect
    }
    hideWithX();
}

void Outline::show(const QRect& outlineGeometry)
{
    setGeometry(outlineGeometry);
    show();
}

void Outline::setGeometry(const QRect& outlineGeometry)
{
    m_outlineGeometry = outlineGeometry;
}

QVector< Window > Outline::windowIds() const
{
    QVector<Window> windows;
    if (m_initialized) {
        windows.reserve(4);
        windows << m_leftOutline << m_topOutline << m_rightOutline << m_bottomOutline;
    }
    return windows;
}

void Outline::showWithX()
{
    if (!m_initialized) {
        XSetWindowAttributes attr;
        attr.override_redirect = 1;
        m_leftOutline = XCreateWindow(QX11Info::display(), QX11Info::appRootWindow(), 0, 0, 1, 1, 0,
                                      CopyFromParent, CopyFromParent, CopyFromParent, CWOverrideRedirect, &attr);
        m_rightOutline = XCreateWindow(QX11Info::display(), QX11Info::appRootWindow(), 0, 0, 1, 1, 0,
                                       CopyFromParent, CopyFromParent, CopyFromParent, CWOverrideRedirect, &attr);
        m_topOutline = XCreateWindow(QX11Info::display(), QX11Info::appRootWindow(), 0, 0, 1, 1, 0,
                                     CopyFromParent, CopyFromParent, CopyFromParent, CWOverrideRedirect, &attr);
        m_bottomOutline = XCreateWindow(QX11Info::display(), QX11Info::appRootWindow(), 0, 0, 1, 1, 0,
                                        CopyFromParent, CopyFromParent, CopyFromParent, CWOverrideRedirect, &attr);
        m_initialized = true;
    }

    int defaultDepth = XDefaultDepth(QX11Info::display(), QX11Info::appScreen());

// left/right parts are between top/bottom, they don't reach as far as the corners
    XMoveResizeWindow(QX11Info::display(), m_leftOutline, m_outlineGeometry.x(), m_outlineGeometry.y() + 5, 5, m_outlineGeometry.height() - 10);
    XMoveResizeWindow(QX11Info::display(), m_rightOutline, m_outlineGeometry.x() + m_outlineGeometry.width() - 5, m_outlineGeometry.y() + 5, 5, m_outlineGeometry.height() - 10);
    XMoveResizeWindow(QX11Info::display(), m_topOutline, m_outlineGeometry.x(), m_outlineGeometry.y(), m_outlineGeometry.width(), 5);
    XMoveResizeWindow(QX11Info::display(), m_bottomOutline, m_outlineGeometry.x(), m_outlineGeometry.y() + m_outlineGeometry.height() - 5, m_outlineGeometry.width(), 5);
    {
        Pixmap xpix = XCreatePixmap(display(), rootWindow(), 5,
                                    m_outlineGeometry.height() - 10, defaultDepth);
        QPixmap pix = QPixmap::fromX11Pixmap(xpix, QPixmap::ExplicitlyShared);
        QPainter p(&pix);
        p.setPen(Qt::white);
        p.drawLine(0, 0, 0, pix.height() - 1);
        p.drawLine(4, 0, 4, pix.height() - 1);
        p.setPen(Qt::gray);
        p.drawLine(1, 0, 1, pix.height() - 1);
        p.drawLine(3, 0, 3, pix.height() - 1);
        p.setPen(Qt::black);
        p.drawLine(2, 0, 2, pix.height() - 1);
        p.end();
        XSetWindowBackgroundPixmap(QX11Info::display(), m_leftOutline, pix.handle());
        XSetWindowBackgroundPixmap(QX11Info::display(), m_rightOutline, pix.handle());
        // According to the XSetWindowBackgroundPixmap documentation the pixmap can be freed.
        XFreePixmap (display(), xpix);
    }
    {
        Pixmap xpix = XCreatePixmap(display(), rootWindow(), m_outlineGeometry.width(),
                                    5, defaultDepth);
        QPixmap pix = QPixmap::fromX11Pixmap(xpix, QPixmap::ExplicitlyShared);
        QPainter p(&pix);
        p.setPen(Qt::white);
        p.drawLine(0, 0, pix.width() - 1 - 0, 0);
        p.drawLine(4, 4, pix.width() - 1 - 4, 4);
        p.drawLine(0, 0, 0, 4);
        p.drawLine(pix.width() - 1 - 0, 0, pix.width() - 1 - 0, 4);
        p.setPen(Qt::gray);
        p.drawLine(1, 1, pix.width() - 1 - 1, 1);
        p.drawLine(3, 3, pix.width() - 1 - 3, 3);
        p.drawLine(1, 1, 1, 4);
        p.drawLine(3, 3, 3, 4);
        p.drawLine(pix.width() - 1 - 1, 1, pix.width() - 1 - 1, 4);
        p.drawLine(pix.width() - 1 - 3, 3, pix.width() - 1 - 3, 4);
        p.setPen(Qt::black);
        p.drawLine(2, 2, pix.width() - 1 - 2, 2);
        p.drawLine(2, 2, 2, 4);
        p.drawLine(pix.width() - 1 - 2, 2, pix.width() - 1 - 2, 4);
        p.end();
        XSetWindowBackgroundPixmap(QX11Info::display(), m_topOutline, pix.handle());
        // According to the XSetWindowBackgroundPixmap documentation the pixmap can be freed.
        XFreePixmap (display(), xpix);
    }
    {
        Pixmap xpix = XCreatePixmap(display(), rootWindow(), m_outlineGeometry.width(),
                                    5, defaultDepth);
        QPixmap pix = QPixmap::fromX11Pixmap(xpix, QPixmap::ExplicitlyShared);
        QPainter p(&pix);
        p.setPen(Qt::white);
        p.drawLine(4, 0, pix.width() - 1 - 4, 0);
        p.drawLine(0, 4, pix.width() - 1 - 0, 4);
        p.drawLine(0, 4, 0, 0);
        p.drawLine(pix.width() - 1 - 0, 4, pix.width() - 1 - 0, 0);
        p.setPen(Qt::gray);
        p.drawLine(3, 1, pix.width() - 1 - 3, 1);
        p.drawLine(1, 3, pix.width() - 1 - 1, 3);
        p.drawLine(3, 1, 3, 0);
        p.drawLine(1, 3, 1, 0);
        p.drawLine(pix.width() - 1 - 3, 1, pix.width() - 1 - 3, 0);
        p.drawLine(pix.width() - 1 - 1, 3, pix.width() - 1 - 1, 0);
        p.setPen(Qt::black);
        p.drawLine(2, 2, pix.width() - 1 - 2, 2);
        p.drawLine(2, 0, 2, 2);
        p.drawLine(pix.width() - 1 - 2, 0, pix.width() - 1 - 2, 2);
        p.end();
        XSetWindowBackgroundPixmap(QX11Info::display(), m_bottomOutline, pix.handle());
        // According to the XSetWindowBackgroundPixmap documentation the pixmap can be freed.
        XFreePixmap (display(), xpix);
    }
    XClearWindow(QX11Info::display(), m_leftOutline);
    XClearWindow(QX11Info::display(), m_rightOutline);
    XClearWindow(QX11Info::display(), m_topOutline);
    XClearWindow(QX11Info::display(), m_bottomOutline);
    XMapWindow(QX11Info::display(), m_leftOutline);
    XMapWindow(QX11Info::display(), m_rightOutline);
    XMapWindow(QX11Info::display(), m_topOutline);
    XMapWindow(QX11Info::display(), m_bottomOutline);
}

void Outline::hideWithX()
{
    XUnmapWindow(QX11Info::display(), m_leftOutline);
    XUnmapWindow(QX11Info::display(), m_rightOutline);
    XUnmapWindow(QX11Info::display(), m_topOutline);
    XUnmapWindow(QX11Info::display(), m_bottomOutline);
}

} // namespace
