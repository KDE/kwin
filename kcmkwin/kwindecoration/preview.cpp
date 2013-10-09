/*
 *
 * Copyright (c) 2003 Lubos Lunak <l.lunak@kde.org>
 * Copyright (c) 2013 Martin Gräßlin <mgraesslin@kde.org>
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
#include <QApplication>
#include <QPainter>
#include <QMouseEvent>
#include <QVector>
#include <kicon.h>

#include <kdecorationfactory.h>

static const int SMALL_OFFSET = 10;
static const int LARGE_OFFSET = 40;

PreviewItem::PreviewItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
    , m_plugins(new KDecorationPreviewPlugins(KSharedConfig::openConfig(QStringLiteral("kwinrc"))))
    , m_activeBridge(new KDecorationPreviewBridge(this, true))
    , m_inactiveBridge(new KDecorationPreviewBridge(this, false))
    , m_activeDecoration(nullptr)
    , m_inactiveDecoration(nullptr)
    , m_activeEntered(nullptr)
    , m_inactiveEntered(nullptr)
{
    setFlag(ItemHasContents, true);
    setAcceptHoverEvents(true);
    connect(this, &PreviewItem::libraryChanged, this, &PreviewItem::recreateDecorations);
}

PreviewItem::~PreviewItem()
{
    delete m_activeDecoration;
    delete m_inactiveDecoration;
}

void PreviewItem::setLibraryName(const QString &library)
{
    if (library == m_libraryName || library.isEmpty()) {
        return;
    }
    m_libraryName = library;
    emit libraryChanged();
}

void PreviewItem::recreateDecorations()
{
    const bool loaded = m_plugins->loadPlugin(m_libraryName);
    if (!loaded) {
        return;
    }
    m_plugins->destroyPreviousPlugin();
    delete m_activeDecoration;
    delete m_inactiveDecoration;
    m_activeDecoration = m_plugins->createDecoration(m_activeBridge.data());
    m_inactiveDecoration = m_plugins->createDecoration(m_inactiveBridge.data());

    if (m_activeDecoration) {
        m_activeDecoration->init();
        if (m_activeDecoration->widget()) {
            m_activeDecoration->widget()->installEventFilter(this);
        }
    }
    if (m_inactiveDecoration) {
        m_inactiveDecoration->init();
        if (m_inactiveDecoration->widget()) {
            m_inactiveDecoration->widget()->installEventFilter(this);
        }
    }
    updatePreview();
}

void PreviewItem::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickPaintedItem::geometryChanged(newGeometry, oldGeometry);
    updatePreview();
}

void PreviewItem::updatePreview()
{
    if (width() == 0 && height() == 0) {
        return;
    }
    if (!m_activeDecoration && !m_inactiveDecoration) {
        return;
    }
    const QSize size(width() - 50, height() - 50);
    updateSize(size, m_activeDecoration, m_activeBuffer);
    updateSize(size, m_inactiveDecoration, m_inactiveBuffer);

    render(&m_activeBuffer, m_activeDecoration);
    render(&m_inactiveBuffer, m_inactiveDecoration);
}

void PreviewItem::updateSize(const QSize &baseSize, KDecoration *decoration, QImage &buffer)
{
    if (!decoration) {
        return;
    }
    int left, right, top, bottom;
    left = right = top = bottom = 0;
    decoration->padding(left, right, top, bottom);
    const QSize size = baseSize + QSize(left + right, top + bottom);
    if (decoration->geometry().size() != size) {
        decoration->resize(size);
    }
    if (buffer.isNull() || buffer.size() != size) {
        buffer = QImage(size, QImage::Format_ARGB32_Premultiplied);
    }
}

void PreviewItem::render(QImage* image, KDecoration* decoration)
{
    image->fill(Qt::transparent);
    decoration->render(image, QRegion());
}

void PreviewItem::paint(QPainter *painter)
{
    int paddingLeft, paddingRigth, paddingTop, paddingBottom;
    paddingLeft = paddingRigth = paddingTop = paddingBottom = 0;
    if (m_inactiveDecoration) {
        m_inactiveDecoration->padding(paddingLeft, paddingRigth, paddingTop, paddingBottom);
    }

    painter->drawImage(LARGE_OFFSET - paddingLeft, SMALL_OFFSET - paddingTop, m_inactiveBuffer);

    paddingLeft = paddingRigth = paddingTop = paddingBottom = 0;
    if (m_activeDecoration) {
        m_activeDecoration->padding(paddingLeft, paddingRigth, paddingTop, paddingBottom);
    }
    painter->drawImage(SMALL_OFFSET - paddingLeft, LARGE_OFFSET - paddingTop, m_activeBuffer);
}

void PreviewItem::hoverMoveEvent(QHoverEvent* event)
{
    QQuickItem::hoverMoveEvent(event);
    const int w = width() - LARGE_OFFSET - SMALL_OFFSET;
    const int h = height() - LARGE_OFFSET - SMALL_OFFSET;
    forwardMoveEvent(event, QRect(SMALL_OFFSET, LARGE_OFFSET, w, h), m_activeDecoration, &m_activeEntered);
    forwardMoveEvent(event, QRect(LARGE_OFFSET, SMALL_OFFSET, w, h), m_inactiveDecoration, &m_inactiveEntered);
}

void PreviewItem::forwardMoveEvent(QHoverEvent *event, const QRect &geo, KDecoration *deco, QWidget **entered)
{
    auto leaveEvent = [](QWidget **widget) {
        if (!(*widget)) {
            return;
        }
        // send old one a leave event
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(*widget, &leave);
        *widget = nullptr;
    };
    if (geo.contains(event->pos()) && deco && deco->widget()) {
        int paddingLeft, paddingRigth, paddingTop, paddingBottom;
        paddingLeft = paddingRigth = paddingTop = paddingBottom = 0;
        deco->padding(paddingLeft, paddingRigth, paddingTop, paddingBottom);
        const QPoint widgetPos = event->pos() - geo.topLeft() + QPoint(paddingLeft, paddingTop);
        if (QWidget *widget = deco->widget()->childAt(widgetPos)) {
            if (widget != *entered) {
                leaveEvent(entered);
                // send enter event
                *entered = widget;
                QEnterEvent enter(widgetPos - widget->geometry().topLeft(), widgetPos, event->pos());
                QApplication::sendEvent(*entered, &enter);
            }
        } else {
            leaveEvent(entered);
        }
    } else {
        leaveEvent(entered);
    }
}

void PreviewItem::updateDecoration(KDecorationPreviewBridge *bridge)
{
    if (bridge == m_activeBridge.data()) {
        render(&m_activeBuffer, m_activeDecoration);
    } else if (bridge == m_inactiveBridge.data()) {
        render(&m_inactiveBuffer, m_inactiveDecoration);
    }
    update();
}

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
    decoration->padding(padLeft, padRight, padTop, padBottom);
    size.setWidth(size.width() + padLeft + padRight);
    size.setHeight(size.height() + padTop + padBottom);
    if (padLeft || padRight || padTop || padBottom) {
        useMask = false;
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
    auto connection = connect(plugin->factory(), &KDecorationFactory::recreateDecorations, [this, plugin] {
        // can't handle the change, recreate decorations then
        recreateDecoration(plugin);
    });
    options->setCustomBorderSize(size);
    disconnect(connection);
}

void KDecorationPreview::setTempButtons(KDecorationPlugins* plugin, bool customEnabled, const QString &left, const QString &right)
{
    auto connection = connect(plugin->factory(), &KDecorationFactory::recreateDecorations, [this, plugin] {
        // can't handle the change, recreate decorations then
        recreateDecoration(plugin);
    });
    options->setCustomTitleButtonsEnabled(customEnabled);
    options->setCustomTitleButtons(left, right);
    disconnect(connection);
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
    :   preview(p), active(a), m_previewItem(nullptr)
{
}

KDecorationPreviewBridge::KDecorationPreviewBridge(PreviewItem *p, bool a)
    : preview(nullptr)
    , active(a)
    , m_previewItem(p)
{
}

Qt::WindowFlags KDecorationPreviewBridge::initialWFlags() const
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
    Q_UNUSED(reg)
    Q_UNUSED(mode)
}

bool KDecorationPreviewBridge::isPreview() const
{
    return true;
}

QRect KDecorationPreviewBridge::geometry() const
{
    if (preview) {
        return preview->windowGeometry(active);
    }
    return QRect();
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

void KDecorationPreviewBridge::update(const QRegion&)
{
    if (m_previewItem) {
        // call update
        m_previewItem->updateDecoration(this);
    }
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

void KDecorationPreviewOptions::updateSettings()
{
    KConfig cfg("kwinrc");
    KDecorationOptions::updateSettings(&cfg);

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
