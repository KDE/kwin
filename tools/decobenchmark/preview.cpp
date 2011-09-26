/*
 *
 * Copyright (c) 2003 Lubos Lunak <l.lunak@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "preview.h"

#include <kdebug.h>

#include <kapplication.h>
#include <klocale.h>
#include <kconfig.h>
#include <kglobal.h>
#include <QLabel>
#include <QStyle>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QVector>
#include <kiconloader.h>

#include <X11/Xlib.h>
#include <X11/extensions/shape.h>

#include <kdecorationfactory.h>
#include <kdecoration_plugins_p.h>
#include <QX11Info>

// FRAME the preview doesn't update to reflect the changes done in the kcm

KDecorationPreview::KDecorationPreview(KDecorationPlugins* plugin, QWidget* parent, const char* name)
    :   QWidget(parent, name),
        m_plugin(plugin)
{
    options = new KDecorationPreviewOptions;

    bridge = new KDecorationPreviewBridge(this, true, "Deco Benchmark");

    deco = 0;

    setFixedSize(600, 500);

    positionPreviews();
}

KDecorationPreview::~KDecorationPreview()
{
    delete deco;
    delete bridge;
    delete options;
}

void KDecorationPreview::performRepaintTest(int n)
{
    kDebug(1212) << "start " << n << " repaints...";
    bridge->setCaption("Deco Benchmark");
    deco->captionChange();
    positionPreviews(0);
    for (int i = 0; i < n; ++i) {
        deco->widget()->repaint();
        kapp->processEvents();
    }
}

void KDecorationPreview::performCaptionTest(int n)
{
    kDebug(1212) << "start " << n << " caption changes...";
    QString caption = "Deco Benchmark %1";
    positionPreviews(0);
    for (int i = 0; i < n; ++i) {
        bridge->setCaption(caption.arg(i));
        deco->captionChange();
        deco->widget()->repaint();
        kapp->processEvents();
    }
}

void KDecorationPreview::performResizeTest(int n)
{
    kDebug(1212) << "start " << n << " resizes...";
    bridge->setCaption("Deco Benchmark");
    deco->captionChange();
    for (int i = 0; i < n; ++i) {
        positionPreviews(i % 200);
        kapp->processEvents();
    }
}

void KDecorationPreview::performRecreationTest(int n)
{
    kDebug(1212) << "start " << n << " resizes...";
    bridge->setCaption("Deco Benchmark");
    deco->captionChange();
    positionPreviews(0);
    for (int i = 0; i < n; ++i) {
        recreateDecoration();
        kapp->processEvents();
    }
}

bool KDecorationPreview::recreateDecoration()
{
    delete deco;
    deco = m_plugin->createDecoration(bridge);

    if (!deco)
        return false;

    deco->init();
    positionPreviews();
    deco->widget()->show();

    return true;
}

void KDecorationPreview::positionPreviews(int shrink)
{
    if (!deco)
        return;

    QSize size = QSize(width() - 2 * 10 - shrink, height() - 2 * 10 - shrink)/*.expandedTo(deco->minimumSize()*/;

    QRect geometry(QPoint(10, 10), size);
    deco->widget()->setGeometry(geometry);
}

void KDecorationPreview::setPreviewMask(const QRegion& reg, int mode)
{
    QWidget *widget = deco->widget();

    // FRAME duped from client.cpp
    if (mode == Unsorted) {
        XShapeCombineRegion(QX11Info::display(), widget->winId(), ShapeBounding, 0, 0,
                            reg.handle(), ShapeSet);
    } else {
        QVector< QRect > rects = reg.rects();
        XRectangle* xrects = new XRectangle[ rects.count()];
        for (unsigned int i = 0;
                i < rects.count();
                ++i) {
            xrects[ i ].x = rects[ i ].x();
            xrects[ i ].y = rects[ i ].y();
            xrects[ i ].width = rects[ i ].width();
            xrects[ i ].height = rects[ i ].height();
        }
        XShapeCombineRectangles(QX11Info::display(), widget->winId(), ShapeBounding, 0, 0,
                                xrects, rects.count(), ShapeSet, mode);
        delete[] xrects;
    }
}

QRect KDecorationPreview::windowGeometry(bool active) const
{
    QWidget *widget = deco->widget();
    return widget->geometry();
}

QRegion KDecorationPreview::unobscuredRegion(bool active, const QRegion& r) const
{
    return r;
}

KDecorationPreviewBridge::KDecorationPreviewBridge(KDecorationPreview* p, bool a, const QString &c)
    :   preview(p), active(a), m_caption(c)
{
}

void KDecorationPreviewBridge::setCaption(const QString &c)
{
    m_caption = c;
}

QWidget* KDecorationPreviewBridge::initialParentWidget() const
{
    return preview;
}

Qt::WFlags KDecorationPreviewBridge::initialWFlags() const
{
    return 0;
}

bool KDecorationPreviewBridge::isActive() const
{
    return active;
}

bool KDecorationPreviewBridge::isCloseable() const
{
    return true;
}

bool KDecorationPreviewBridge::isMaximizable() const
{
    return true;
}

KDecoration::MaximizeMode KDecorationPreviewBridge::maximizeMode() const
{
    return KDecoration::MaximizeRestore;
}

bool KDecorationPreviewBridge::isMinimizable() const
{
    return true;
}

bool KDecorationPreviewBridge::providesContextHelp() const
{
    return true;
}

int KDecorationPreviewBridge::desktop() const
{
    return 1;
}

bool KDecorationPreviewBridge::isModal() const
{
    return false;
}

bool KDecorationPreviewBridge::isShadeable() const
{
    return true;
}

bool KDecorationPreviewBridge::isShade() const
{
    return false;
}

bool KDecorationPreviewBridge::isSetShade() const
{
    return false;
}

bool KDecorationPreviewBridge::keepAbove() const
{
    return false;
}

bool KDecorationPreviewBridge::keepBelow() const
{
    return false;
}

bool KDecorationPreviewBridge::isMovable() const
{
    return true;
}

bool KDecorationPreviewBridge::isResizable() const
{
    return true;
}

NET::WindowType KDecorationPreviewBridge::windowType(unsigned long) const
{
    return NET::Normal;
}

QIcon KDecorationPreviewBridge::icon() const
{
    return QIcon(KGlobal::iconLoader()->loadIcon("xorg", KIconLoader::NoGroup, 32));
}

QString KDecorationPreviewBridge::caption() const
{
    return m_caption;
}

void KDecorationPreviewBridge::processMousePressEvent(QMouseEvent*)
{
}

void KDecorationPreviewBridge::showWindowMenu(const QRect &)
{
}

void KDecorationPreviewBridge::showWindowMenu(QPoint)
{
}

void KDecorationPreviewBridge::performWindowOperation(WindowOperation)
{
}

void KDecorationPreviewBridge::setMask(const QRegion& reg, int mode)
{
    preview->setPreviewMask(reg, mode);
}

bool KDecorationPreviewBridge::isPreview() const
{
    return false;
}

QRect KDecorationPreviewBridge::geometry() const
{
    return preview->windowGeometry(active);
}

QRect KDecorationPreviewBridge::iconGeometry() const
{
    return QRect();
}

QRegion KDecorationPreviewBridge::unobscuredRegion(const QRegion& r) const
{
    return preview->unobscuredRegion(active, r);
}

WId KDecorationPreviewBridge::windowId() const
{
    return 0; // no decorated window
}

void KDecorationPreviewBridge::closeWindow()
{
}

void KDecorationPreviewBridge::maximize(MaximizeMode)
{
}

void KDecorationPreviewBridge::minimize()
{
}

void KDecorationPreviewBridge::showContextHelp()
{
}

void KDecorationPreviewBridge::setDesktop(int)
{
}

void KDecorationPreviewBridge::titlebarDblClickOperation()
{
}

void KDecorationPreviewBridge::titlebarMouseWheelOperation(int)
{
}

void KDecorationPreviewBridge::setShade(bool)
{
}

void KDecorationPreviewBridge::setKeepAbove(bool)
{
}

void KDecorationPreviewBridge::setKeepBelow(bool)
{
}

int KDecorationPreviewBridge::currentDesktop() const
{
    return 1;
}

void KDecorationPreviewBridge::grabXServer(bool)
{
}

KDecorationPreviewOptions::KDecorationPreviewOptions()
{
    defaultKWinSettings();
    updateSettings();
}

KDecorationPreviewOptions::~KDecorationPreviewOptions()
{
}

unsigned long KDecorationPreviewOptions::updateSettings()
{
    KConfig cfg("kwinrc", true);
    unsigned long changed = 0;
    changed |= KDecorationOptions::updateKWinSettings(&cfg);

    return changed;
}

bool KDecorationPreviewPlugins::provides(Requirement)
{
    return false;
}

// #include "preview.moc"
