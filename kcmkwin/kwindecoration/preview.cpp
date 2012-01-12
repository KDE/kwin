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

#include <kapplication.h>
#include <klocale.h>
#include <kconfig.h>
#include <kglobal.h>
#include <QLabel>
#include <QStyle>
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QVector>
#include <kicon.h>

#include <X11/Xlib.h>
#include <X11/extensions/shape.h>

#include <kdecorationfactory.h>
#include <kdecoration_plugins_p.h>
#include <QX11Info>
#include <kwindowsystem.h>
#include <QTextDocument>

KDecorationPreview::KDecorationPreview(QWidget* parent)
    :   QWidget(parent)
{
    options = new KDecorationPreviewOptions;

    bridge[Active]   = new KDecorationPreviewBridge(this, true);
    bridge[Inactive] = new KDecorationPreviewBridge(this, false);

    deco[Active] = deco[Inactive] = NULL;

    no_preview = new QLabel(i18n("No preview available.\n"
                                 "Most probably there\n"
                                 "was a problem loading the plugin."), this);

    no_preview->setAlignment(Qt::AlignCenter);

    setMinimumSize(100, 100);
    no_preview->resize(size());
}

KDecorationPreview::~KDecorationPreview()
{
    for (int i = 0; i < NumWindows; i++) {
        delete deco[i];
        delete bridge[i];
    }
    delete options;
}

bool KDecorationPreview::recreateDecoration(KDecorationPlugins* plugins)
{
    for (int i = 0; i < NumWindows; i++) {
        delete deco[i];   // deletes also window
        deco[i] = plugins->createDecoration(bridge[i]);
        deco[i]->init();
    }

    if (deco[Active] == NULL || deco[Inactive] == NULL) {
        return false;
    }

    positionPreviews();
    //deco[Inactive]->widget()->show();
    //deco[Active]->widget()->show();

    //deco[Inactive]->widget()->render( this, deco[Inactive]->widget()->mapToParent( QPoint(0,0) ) );

    return true;
}

void KDecorationPreview::enablePreview()
{
    no_preview->hide();
}

void KDecorationPreview::disablePreview()
{
    delete deco[Active];
    delete deco[Inactive];
    deco[Active] = deco[Inactive] = NULL;
    no_preview->show();
}

void KDecorationPreview::paintEvent(QPaintEvent* e)
{
    Q_UNUSED(e);
    QPainter painter(this);
    QPoint delta = mapTo(window(), QPoint(0, 0));

    if (deco[Inactive]) {
        QWidget *w = deco[Inactive]->widget();
        w->render(&painter, delta + w->mapToParent(QPoint(0, 0)));
    }
    if (deco[Active]) {
        QWidget *w = deco[Active]->widget();
        w->render(&painter, delta + w->mapToParent(QPoint(0, 0)));
    }
}

QPixmap KDecorationPreview::preview(QTextDocument* document, QWidget* widget)
{
    Q_UNUSED(document);
    Q_UNUSED(widget);
    QPixmap pixmap(size());
    pixmap.fill(Qt::transparent);

    if (deco[Inactive]) {
        QWidget *w = deco[Inactive]->widget();
        w->render(&pixmap, w->mapToParent(QPoint(0, 0)));
    }
    if (deco[Active]) {
        QWidget *w = deco[Active]->widget();
        w->render(&pixmap, w->mapToParent(QPoint(0, 0)));
    }
    return pixmap;
}

void KDecorationPreview::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    positionPreviews();
}

void KDecorationPreview::positionPreviews()
{
    int titleBarHeight, leftBorder, rightBorder, xoffset,
        dummy1, dummy2, dummy3;
    QRect geometry;
    QSize size;

    no_preview->resize(this->size());

    if (!deco[Active] || !deco[Inactive])
        return;

    // don't have more than one reference to the same dummy variable in one borders() call.
    deco[Active]->borders(dummy1, dummy2, titleBarHeight, dummy3);
    deco[Inactive]->borders(leftBorder, rightBorder, dummy1, dummy2);

    titleBarHeight = qMin(int(titleBarHeight * .9), 30);
    xoffset = qMin(qMax(10, QApplication::isRightToLeft()
                        ? leftBorder : rightBorder), 30);

    // Resize the active window
    size = QSize(width() - xoffset, height() - titleBarHeight)
           .expandedTo(deco[Active]->minimumSize());
    geometry = QRect(QPoint(0, titleBarHeight), size);
    if (KDecorationUnstable *unstable = qobject_cast<KDecorationUnstable *>(deco[Active])) {
        int padLeft, padRight, padTop, padBottom;
        unstable->padding(padLeft, padRight, padTop, padBottom);
        geometry.adjust(-padLeft, -padTop, padRight, padBottom);
    }
    geometry.adjust(10, 10, -10, -10);
    deco[Active]->widget()->setGeometry(QStyle::visualRect(this->layoutDirection(), this->rect(), geometry));

    // Resize the inactive window
    size = QSize(width() - xoffset, height() - titleBarHeight)
           .expandedTo(deco[Inactive]->minimumSize());
    geometry = QRect(QPoint(xoffset, 0), size);
    if (KDecorationUnstable *unstable = qobject_cast<KDecorationUnstable *>(deco[Inactive])) {
        int padLeft, padRight, padTop, padBottom;
        unstable->padding(padLeft, padRight, padTop, padBottom);
        geometry.adjust(-padLeft, -padTop, padRight, padBottom);
    }
    geometry.adjust(10, 10, -10, -10);
    deco[Inactive]->widget()->setGeometry(QStyle::visualRect(this->layoutDirection(), this->rect(), geometry));
}

void KDecorationPreview::setPreviewMask(const QRegion& reg, int mode, bool active)
{
    QWidget *widget = active ? deco[Active]->widget() : deco[Inactive]->widget();

    // FRAME duped from client.cpp
    if (mode == Unsorted) {
        XShapeCombineRegion(QX11Info::display(), widget->winId(), ShapeBounding, 0, 0,
                            reg.handle(), ShapeSet);
    } else {
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
        XShapeCombineRectangles(QX11Info::display(), widget->winId(), ShapeBounding, 0, 0,
                                xrects, rects.count(), ShapeSet, mode);
        delete[] xrects;
    }
    if (active)
        mask = reg; // keep shape of the active window for unobscuredRegion()
}

QRect KDecorationPreview::windowGeometry(bool active) const
{
    QWidget *widget = active ? deco[Active]->widget() : deco[Inactive]->widget();
    return widget->geometry();
}

void KDecorationPreview::setTempBorderSize(KDecorationPlugins* plugin, KDecorationDefines::BorderSize size)
{
    options->setCustomBorderSize(size);
    if (plugin->factory()->reset(KDecorationDefines::SettingBorder)) {
        // can't handle the change, recreate decorations then
        recreateDecoration(plugin);
    } else {
        // handles the update, only update position...
        positionPreviews();
    }
}

void KDecorationPreview::setTempButtons(KDecorationPlugins* plugin, bool customEnabled, const QString &left, const QString &right)
{
    options->setCustomTitleButtonsEnabled(customEnabled);
    options->setCustomTitleButtons(left, right);
    if (plugin->factory()->reset(KDecorationDefines::SettingButtons)) {
        // can't handle the change, recreate decorations then
        recreateDecoration(plugin);
    } else {
        // handles the update, only update position...
        positionPreviews();
    }
}

QRegion KDecorationPreview::unobscuredRegion(bool active, const QRegion& r) const
{
    if (active)   // this one is not obscured
        return r;
    else {
        // copied from KWin core's code
        QRegion ret = r;
        QRegion r2 = mask;
        if (r2.isEmpty())
            r2 = QRegion(windowGeometry(true));
        r2.translate(windowGeometry(true).x() - windowGeometry(false).x(),
                     windowGeometry(true).y() - windowGeometry(false).y());
        ret -= r2;
        return ret;
    }
}

KDecorationPreviewBridge::KDecorationPreviewBridge(KDecorationPreview* p, bool a)
    :   preview(p), active(a)
{
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
    return KIcon("xorg");
}

QString KDecorationPreviewBridge::caption() const
{
    return active ? i18n("Active Window") : i18n("Inactive Window");
}

void KDecorationPreviewBridge::processMousePressEvent(QMouseEvent*)
{
}

void KDecorationPreviewBridge::showWindowMenu(const QRect &)
{
}

void KDecorationPreviewBridge::showWindowMenu(const QPoint &)
{
}

void KDecorationPreviewBridge::performWindowOperation(WindowOperation)
{
}

void KDecorationPreviewBridge::setMask(const QRegion& reg, int mode)
{
    preview->setPreviewMask(reg, mode, active);
}

bool KDecorationPreviewBridge::isPreview() const
{
    return true;
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

bool KDecorationPreviewBridge::compositingActive() const
{
    return KWindowSystem::compositingActive();
}

QRect KDecorationPreviewBridge::transparentRect() const
{
    return QRect();
}

// Window tabbing

int KDecorationPreviewBridge::tabCount() const
{
    return 1;
}

QString KDecorationPreviewBridge::caption(int) const
{
    return active ? "Active Window" : "Inactive Window";
}

QIcon KDecorationPreviewBridge::icon(int) const
{
    return icon();
}

long KDecorationPreviewBridge::tabId(int) const
{
    return 0;
}

long KDecorationPreviewBridge::currentTabId() const
{
    return 0;
}

void KDecorationPreviewBridge::setCurrentTab(long)
{
}

void KDecorationPreviewBridge::tab_A_before_B(long, long)
{
}

void KDecorationPreviewBridge::tab_A_behind_B(long, long)
{
}

void KDecorationPreviewBridge::untab(long, const QRect&)
{
}

void KDecorationPreviewBridge::closeTab(long)
{
}

void KDecorationPreviewBridge::closeTabGroup()
{
}

void KDecorationPreviewBridge::showWindowMenu(const QPoint &, long)
{
}


KDecoration::WindowOperation KDecorationPreviewBridge::buttonToWindowOperation(Qt::MouseButtons)
{
    return KDecoration::NoOp;
}

KDecorationPreviewOptions::KDecorationPreviewOptions()
{
    customBorderSize = BordersCount; // invalid
    customButtonsChanged = false; // invalid
    customButtons = true;
    customTitleButtonsLeft.clear(); // invalid
    customTitleButtonsRight.clear(); // invalid
    updateSettings();
}

KDecorationPreviewOptions::~KDecorationPreviewOptions()
{
}

unsigned long KDecorationPreviewOptions::updateSettings()
{
    KConfig cfg("kwinrc");
    unsigned long changed = 0;
    changed |= KDecorationOptions::updateSettings(&cfg);

    // set custom border size/buttons
    if (customBorderSize != BordersCount)
        setBorderSize(customBorderSize);
    if (customButtonsChanged)
        setCustomButtonPositions(customButtons);
    if (customButtons) {
        if (!customTitleButtonsLeft.isNull())
            setTitleButtonsLeft(customTitleButtonsLeft);
        if (!customTitleButtonsRight.isNull())
            setTitleButtonsRight(customTitleButtonsRight);
    } else {
        setTitleButtonsLeft(KDecorationOptions::defaultTitleButtonsLeft());
        setTitleButtonsRight(KDecorationOptions::defaultTitleButtonsRight());
    }

    return changed;
}

void KDecorationPreviewOptions::setCustomBorderSize(BorderSize size)
{
    customBorderSize = size;

    updateSettings();
}

void KDecorationPreviewOptions::setCustomTitleButtonsEnabled(bool enabled)
{
    customButtonsChanged = true;
    customButtons = enabled;

    updateSettings();
}

void KDecorationPreviewOptions::setCustomTitleButtons(const QString &left, const QString &right)
{
    customTitleButtonsLeft = left;
    customTitleButtonsRight = right;

    updateSettings();
}

bool KDecorationPreviewPlugins::provides(Requirement)
{
    return false;
}

#include "preview.moc"
