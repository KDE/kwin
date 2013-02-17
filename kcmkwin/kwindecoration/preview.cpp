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
#include <KDE/KLocalizedString>
#include <kconfig.h>
#include <kglobal.h>
#include <QLabel>
#include <QStyle>
#include <QPainter>
#include <QMouseEvent>
#include <QVector>
#include <kicon.h>

#include <kdecorationfactory.h>

KDecorationPreview::KDecorationPreview(QWidget* parent)
    :   QWidget(parent)
{
    options = new KDecorationPreviewOptions;

    bridge[Active]   = new KDecorationPreviewBridge(this, true);
    bridge[Inactive] = new KDecorationPreviewBridge(this, false);

    deco[Active] = deco[Inactive] = NULL;

    setMinimumSize(100, 100);
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

    m_activeMask = QRegion();
    m_inactiveMask = QRegion();

    if (deco[Active] == NULL || deco[Inactive] == NULL) {
        return false;
    }

    return true;
}

void KDecorationPreview::disablePreview()
{
    delete deco[Active];
    delete deco[Inactive];
    deco[Active] = deco[Inactive] = NULL;
}

KDecorationFactory *KDecorationPreview::factory() const
{
    return deco[Active] ? deco[Active]->factory() : 0;
}

QPixmap KDecorationPreview::preview()
{
    QPixmap pixmap(size());
    pixmap.fill(Qt::transparent);
    if (!deco[Active] || !deco[Inactive])
        return pixmap;

    int titleBarHeight, leftBorder, rightBorder, xoffset,
        dummy1, dummy2, dummy3;
    // don't have more than one reference to the same dummy variable in one borders() call.
    deco[Active]->borders(dummy1, dummy2, titleBarHeight, dummy3);
    deco[Inactive]->borders(leftBorder, rightBorder, dummy1, dummy2);

    titleBarHeight = qMin(int(titleBarHeight * .9), 30);
    xoffset = qMin(qMax(10, QApplication::isRightToLeft()
                        ? leftBorder : rightBorder), 30);
    QPainter p;
    p.begin(&pixmap);

    const QSize size(width() - xoffset - 20, height() - titleBarHeight - 20);
    render(&p, deco[Inactive], size, QPoint(10 + xoffset, 10), m_inactiveMask);
    render(&p, deco[Active], size, QPoint(10, 10 + titleBarHeight), m_activeMask);
    p.end();
    return pixmap;
}

void KDecorationPreview::render(QPainter *painter, KDecoration *decoration, const QSize &recommendedSize, const QPoint &offset, const QRegion &mask) const
{
    QWidget *w = decoration->widget();
    QSize size = QSize(recommendedSize)
        .expandedTo(decoration->minimumSize());
    int padLeft, padRight, padTop, padBottom;
    padLeft = padRight = padTop = padBottom = 0;
    bool useMask = true;
    if (KDecorationUnstable *unstable = qobject_cast<KDecorationUnstable *>(decoration)) {
        unstable->padding(padLeft, padRight, padTop, padBottom);
        size.setWidth(size.width() + padLeft + padRight);
        size.setHeight(size.height() + padTop + padBottom);
        if (padLeft || padRight || padTop || padBottom) {
            useMask = false;
        }
    }
    decoration->resize(size);

    // why an if-else block instead of (useMask ? mask : QRegion())?
    // For what reason ever it completely breaks if the mask is copied.
    if (useMask) {
        w->render(painter, offset + QPoint(-padLeft, - padTop), mask,
                  QWidget::DrawWindowBackground | QWidget::DrawChildren | QWidget::IgnoreMask);
    } else {
        w->render(painter, offset + QPoint(-padLeft, - padTop), QRegion(),
                  QWidget::DrawWindowBackground | QWidget::DrawChildren | QWidget::IgnoreMask);
    }
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
    }
}

void KDecorationPreview::setTempButtons(KDecorationPlugins* plugin, bool customEnabled, const QString &left, const QString &right)
{
    options->setCustomTitleButtonsEnabled(customEnabled);
    options->setCustomTitleButtons(left, right);
    if (plugin->factory()->reset(KDecorationDefines::SettingButtons)) {
        // can't handle the change, recreate decorations then
        recreateDecoration(plugin);
    }
}

QRegion KDecorationPreview::unobscuredRegion(bool active, const QRegion& r) const
{
    Q_UNUSED(active)
    return r;
}

void KDecorationPreview::setMask(const QRegion &region, bool active)
{
    if (active) {
        m_activeMask = region;
    } else {
        m_inactiveMask = region;
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

KDecoration::QuickTileMode KDecorationPreviewBridge::quickTileMode() const
{
    return KDecoration::QuickTileNone;
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

void KDecorationPreviewBridge::showApplicationMenu(const QPoint &)
{
}

bool KDecorationPreviewBridge::menuAvailable() const
{
    return false;
}

void KDecorationPreviewBridge::performWindowOperation(WindowOperation)
{
}

void KDecorationPreviewBridge::setMask(const QRegion& reg, int mode)
{
    Q_UNUSED(mode)
    preview->setMask(reg, active);
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
    return true;
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
