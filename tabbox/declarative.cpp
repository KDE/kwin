/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Martin Gräßlin <mgraesslin@kde.org>

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
// own
#include "declarative.h"
#include "tabboxhandler.h"
#include "clientmodel.h"
// Qt
#include <QApplication>
#include <QDebug>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QDesktopWidget>
#include <QPainter>
#include <QtGui/QResizeEvent>
#include <QQuickItem>
#include <QX11Info>
#include <QtCore/QStandardPaths>

// include KDE
#include <KDE/KIconEffect>
#include <KDE/KIconLoader>
#include <KDE/KServiceTypeTrader>
#include <KDE/Plasma/FrameSvg>
#include <KDE/Plasma/Theme>
#include <kwindoweffects.h>
#include <kdeclarative/kdeclarative.h>
// KWin
#include "thumbnailitem.h"
#include <kwindowsystem.h>
#include "../effects.h"
#include "../client.h"
#include "../workspace.h"

namespace KWin
{
namespace TabBox
{

ImageProvider::ImageProvider(QAbstractItemModel *model)
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
    , m_model(model)
{
}

QPixmap ImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    bool ok = false;
    QStringList parts = id.split(QStringLiteral("/"));
    const int row = parts.first().toInt(&ok);
    if (!ok) {
        return QPixmap();
    }
    QModelIndex parentIndex;
    const int parentRow = parts.at(1).toInt(&ok);
    if (ok) {
        // we have parent index
        parentIndex = m_model->index(parentRow, 0);
        if (!parentIndex.isValid()) {
            return QPixmap();
        }
    }
    const QModelIndex index = m_model->index(row, 0, parentIndex);
    if (!index.isValid()) {
        return QPixmap();
    }
    TabBoxClient* client = static_cast< TabBoxClient* >(index.model()->data(index, ClientModel::ClientRole).value<void *>());
    if (!client) {
        return QPixmap();
    }

    QSize s(32, 32);
    if (requestedSize.isValid()) {
        s = requestedSize;
    }
    *size = s;
    QPixmap icon = client->icon().pixmap(s);
    if (s.width() > icon.width() || s.height() > icon.height()) {
        // icon is smaller than what we requested - QML would scale it which looks bad
        QPixmap temp(s);
        temp.fill(Qt::transparent);
        QPainter p(&temp);
        p.drawPixmap(s.width()/2 - icon.width()/2, s.height()/2 - icon.height()/2, icon);
        icon = temp;
    }
    if (parts.size() > 2) {
        KIconEffect *effect = KIconLoader::global()->iconEffect();
        KIconLoader::States state = KIconLoader::DefaultState;
        if (parts.last() == QLatin1String("selected")) {
            state = KIconLoader::ActiveState;
        } else if (parts.last() == QLatin1String("disabled")) {
            state = KIconLoader::DisabledState;
        }
        icon = effect->apply(icon, KIconLoader::Desktop, state);
    }
    return icon;
}


DeclarativeView::DeclarativeView(QAbstractItemModel *model, TabBoxConfig::TabBoxMode mode, QQuickWindow *parent)
    : QQuickView(parent)
    , m_model(model)
    , m_mode(mode)
    , m_currentScreenGeometry()
    , m_frame(new Plasma::FrameSvg(this))
    , m_currentLayout()
    , m_cachedWidth(0)
    , m_cachedHeight(0)
{
    setColor(Qt::transparent);
    setFlags(Qt::X11BypassWindowManagerHint);
    if (tabBox->embedded()) {
        setResizeMode(QQuickView::SizeRootObjectToView);
    } else {
        setResizeMode(QQuickView::SizeViewToRootObject);
    }
    engine()->addImageProvider(QLatin1String("client"), new ImageProvider(model));
    KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
    kdeclarative.initialize();
    kdeclarative.setupBindings();
#ifndef TABBOX_KCM
    qmlRegisterType<DesktopThumbnailItem>("org.kde.kwin", 0, 1, "DesktopThumbnailItem");
#endif
    qmlRegisterType<WindowThumbnailItem>("org.kde.kwin", 0, 1, "ThumbnailItem");
    rootContext()->setContextProperty(QStringLiteral("viewId"), static_cast<qulonglong>(winId()));
    if (m_mode == TabBoxConfig::ClientTabBox) {
        rootContext()->setContextProperty(QStringLiteral("clientModel"), model);
    } else if (m_mode == TabBoxConfig::DesktopTabBox) {
        rootContext()->setContextProperty(QStringLiteral("clientModel"), model);
    }
    setSource(QUrl(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QLatin1String(KWIN_NAME) + QLatin1String("/tabbox/tabbox.qml"))));

    // FrameSvg
    m_frame->setImagePath(QStringLiteral("dialogs/background"));
    m_frame->setCacheAllRenderedFrames(true);
    m_frame->setEnabledBorders(Plasma::FrameSvg::AllBorders);

    connect(tabBox, SIGNAL(configChanged()), SLOT(updateQmlSource()));
    if (m_mode == TabBoxConfig::ClientTabBox) {
        connect(tabBox, SIGNAL(embeddedChanged(bool)), SLOT(slotEmbeddedChanged(bool)));
    }
}

void DeclarativeView::showEvent(QShowEvent *event)
{
#ifndef TABBOX_KCM
    if (tabBox->embedded()) {
        Client *c = Workspace::self()->findClient(WindowMatchPredicate(tabBox->embedded()));
        if (c) {
            connect(c, SIGNAL(geometryChanged()), this, SLOT(slotUpdateGeometry()));
        }
    }
#endif
    updateQmlSource();
    m_currentScreenGeometry = QApplication::desktop()->screenGeometry(tabBox->activeScreen());
    rootObject()->setProperty("screenWidth", m_currentScreenGeometry.width());
    rootObject()->setProperty("screenHeight", m_currentScreenGeometry.height());
    rootObject()->setProperty("allDesktops", tabBox->config().tabBoxMode() == TabBoxConfig::ClientTabBox &&
        tabBox->config().clientDesktopMode() == TabBoxConfig::AllDesktopsClients);
    if (ClientModel *clientModel = qobject_cast<ClientModel*>(m_model)) {
        rootObject()->setProperty("longestCaption", clientModel->longestCaption());
    }

    if (QObject *item = rootObject()->findChild<QObject*>(QStringLiteral("listView"))) {
        item->setProperty("currentIndex", tabBox->first().row());
        connect(item, SIGNAL(currentIndexChanged(int)), SLOT(currentIndexChanged(int)));
    }
    slotUpdateGeometry();
    QResizeEvent re(size(), size()); // to set mask and blurring.
    resizeEvent(&re);
    QQuickView::showEvent(event);
}

void DeclarativeView::resizeEvent(QResizeEvent *event)
{
    if (tabBox->embedded()) {
        KWindowEffects::enableBlurBehind(winId(), false);
    } else {
        const QString maskImagePath = rootObject()->property("maskImagePath").toString();
        if (maskImagePath.isEmpty()) {
            KWindowEffects::enableBlurBehind(winId(), false);
        } else {
            const double maskWidth = rootObject()->property("maskWidth").toDouble();
            const double maskHeight = rootObject()->property("maskHeight").toDouble();
            const int maskTopMargin = rootObject()->property("maskTopMargin").toInt();
            const int maskLeftMargin = rootObject()->property("maskLeftMargin").toInt();
            m_frame->setImagePath(maskImagePath);
            m_frame->resizeFrame(QSizeF(maskWidth, maskHeight));
            QRegion mask = m_frame->mask().translated(maskLeftMargin, maskTopMargin);
#ifndef TABBOX_KCM
            // notice: this covers an issue with plasma detecting the compositing state. see plasmaThemeVariant()
            if (Workspace::self()->compositing() && effects) {
                // blur background?!
                KWindowEffects::enableBlurBehind(winId(), static_cast<EffectsHandlerImpl*>(effects)->provides(Effect::Blur), mask);
            } else
#endif
            {
                // do not trim to mask with compositing enabled, otherwise shadows are cropped
                setMask(mask);
            }
        }
    }
    QQuickView::resizeEvent(event);
}

void DeclarativeView::hideEvent(QHideEvent *event)
{
    QQuickView::hideEvent(event);
#ifndef TABBOX_KCM
    if (tabBox->embedded()) {
        Client *c = Workspace::self()->findClient(WindowMatchPredicate(tabBox->embedded()));
        if (c) {
            disconnect(c, SIGNAL(geometryChanged()), this, SLOT(slotUpdateGeometry()));
        }
    }
#endif
}

#if KWIN_QT5_PORTING
bool DeclarativeView::x11Event(XEvent *e)
{
    if (tabBox->embedded() && 
        (e->type == ButtonPress || e->type == ButtonRelease || e->type == MotionNotify)) {
        XEvent ev;

        memcpy(&ev, e, sizeof(ev));
        if (e->type == ButtonPress || e->type == ButtonRelease) {
            ev.xbutton.x += m_relativePos.x();
            ev.xbutton.y += m_relativePos.y();
            ev.xbutton.window = tabBox->embedded();
        } else if (e->type == MotionNotify) {
            ev.xmotion.x += m_relativePos.x();
            ev.xmotion.y += m_relativePos.y();
            ev.xmotion.window = tabBox->embedded();
        }

        XSendEvent( QX11Info::display(), tabBox->embedded(), False, NoEventMask, &ev );
    }
    return QDeclarativeView::x11Event(e);
}
#endif

void DeclarativeView::slotUpdateGeometry()
{
    const WId embeddedId = tabBox->embedded();
    if (embeddedId != 0) {
        const KWindowInfo info = KWindowSystem::windowInfo(embeddedId, NET::WMGeometry);
        const Qt::Alignment alignment = tabBox->embeddedAlignment();
        const QPoint offset = tabBox->embeddedOffset();
        int x = info.geometry().left();
        int y = info.geometry().top();
        int width = tabBox->embeddedSize().width();
        int height = tabBox->embeddedSize().height();
        if (alignment.testFlag(Qt::AlignLeft) || alignment.testFlag(Qt::AlignHCenter)) {
            x += offset.x();
        }
        if (alignment.testFlag(Qt::AlignRight)) {
            x = x + info.geometry().width() - offset.x() - width;
        }
        if (alignment.testFlag(Qt::AlignHCenter)) {
            width = info.geometry().width() - 2 * offset.x();
        }
        if (alignment.testFlag(Qt::AlignTop) || alignment.testFlag(Qt::AlignVCenter)) {
            y += offset.y();
        }
        if (alignment.testFlag(Qt::AlignBottom)) {
            y = y + info.geometry().height() - offset.y() - height;
        }
        if (alignment.testFlag(Qt::AlignVCenter)) {
            height = info.geometry().height() - 2 * offset.y();
        }
        setGeometry(QRect(x, y, width, height));

        m_relativePos = QPoint(info.geometry().x(), info.geometry().x());
    } else {
        const int width = rootObject()->property("width").toInt();
        const int height = rootObject()->property("height").toInt();

        Qt::Alignment alignment = Qt::AlignCenter;

        if (rootObject()->property("alignment").canConvert<int>()) {
            alignment = (Qt::Alignment)rootObject()->property("alignment").toInt();
        }

        int x;
        int y;

        if (alignment & Qt::AlignLeft) {
            x = m_currentScreenGeometry.x();
        } else if (alignment & Qt::AlignRight) {
            x = m_currentScreenGeometry.right() - width;
        //Center
        } else {
            x = m_currentScreenGeometry.x() + static_cast<qreal>(m_currentScreenGeometry.width()) * 0.5 - static_cast<qreal>(width) * 0.5;
        }

        if (alignment & Qt::AlignTop) {
            y = m_currentScreenGeometry.y();
        } else if (alignment & Qt::AlignBottom) {
            y = m_currentScreenGeometry.bottom() - height;
        //Center
        } else {
            y = m_currentScreenGeometry.y() + static_cast<qreal>(m_currentScreenGeometry.height()) * 0.5 - static_cast<qreal>(height) * 0.5;
        }

        setGeometry(x, y, width, height);
        m_relativePos = position();
    }
}

void DeclarativeView::setCurrentIndex(const QModelIndex &index, bool disableAnimation)
{
    if (tabBox->config().tabBoxMode() != m_mode) {
        return;
    }
    if (QObject *item = rootObject()->findChild<QObject*>(QStringLiteral("listView"))) {
        QVariant durationRestore;
        if (disableAnimation) {
            durationRestore = item->property("highlightMoveDuration");
            item->setProperty("highlightMoveDuration", QVariant(1));
        }
        item->setProperty("currentIndex", index.row());
        if (disableAnimation) {
            item->setProperty("highlightMoveDuration", durationRestore);
        }
    }
}

void DeclarativeView::currentIndexChanged(int row)
{
    tabBox->setCurrentIndex(m_model->index(row, 0));
    KWindowSystem::forceActiveWindow(m_model->data(m_model->index(row, 0), ClientModel::WIdRole).toLongLong());
}

void DeclarativeView::updateQmlSource(bool force)
{
    if (status() != Ready)
        return;
    if (tabBox->config().tabBoxMode() != m_mode) {
        return;
    }
    if (!force && tabBox->config().layoutName() == m_currentLayout) {
        return;
    }
    const bool desktopMode = (m_mode == TabBoxConfig::DesktopTabBox);
    m_currentLayout = tabBox->config().layoutName();
    KService::Ptr service = desktopMode ? findDesktopSwitcher() : findWindowSwitcher();
    if (!service) {
        return;
    }
    if (service->property(QStringLiteral("X-Plasma-API")).toString() != QStringLiteral("declarativeappletscript")) {
        qDebug() << "Window Switcher Layout is no declarativeappletscript";
        return;
    }
    const QString file = desktopMode ? findDesktopSwitcherScriptFile(service) : findWindowSwitcherScriptFile(service);
    if (file.isNull()) {
        qDebug() << "Could not find QML file for window switcher";
        return;
    }
    rootObject()->setProperty("source", QUrl(file));
}

KService::Ptr DeclarativeView::findWindowSwitcher()
{
    QString constraint = QStringLiteral("[X-KDE-PluginInfo-Name] == '%1'").arg(tabBox->config().layoutName());
    KService::List offers = KServiceTypeTrader::self()->query(QStringLiteral("KWin/WindowSwitcher"), constraint);
    if (offers.isEmpty()) {
        // load default
        constraint = QStringLiteral("[X-KDE-PluginInfo-Name] == '%1'").arg(QStringLiteral("informative"));
        offers = KServiceTypeTrader::self()->query(QStringLiteral("KWin/WindowSwitcher"), constraint);
        if (offers.isEmpty()) {
            qDebug() << "could not find default window switcher layout";
            return KService::Ptr();
        }
    }
    return offers.first();
}

KService::Ptr DeclarativeView::findDesktopSwitcher()
{
    QString constraint = QStringLiteral("[X-KDE-PluginInfo-Name] == '%1'").arg(tabBox->config().layoutName());
    KService::List offers = KServiceTypeTrader::self()->query(QStringLiteral("KWin/DesktopSwitcher"), constraint);
    if (offers.isEmpty()) {
        // load default
        constraint = QStringLiteral("[X-KDE-PluginInfo-Name] == '%1'").arg(QStringLiteral("informative"));
        offers = KServiceTypeTrader::self()->query(QStringLiteral("KWin/DesktopSwitcher"), constraint);
        if (offers.isEmpty()) {
            qDebug() << "could not find default desktop switcher layout";
            return KService::Ptr();
        }
    }
    return offers.first();
}

QString DeclarativeView::findWindowSwitcherScriptFile(KService::Ptr service)
{
    const QString pluginName = service->property(QStringLiteral("X-KDE-PluginInfo-Name")).toString();
    const QString scriptName = service->property(QStringLiteral("X-Plasma-MainScript")).toString();
    return QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral(KWIN_NAME) + QStringLiteral("/tabbox/") + pluginName + QStringLiteral("/contents/") + scriptName);
}

QString DeclarativeView::findDesktopSwitcherScriptFile(KService::Ptr service)
{
    const QString pluginName = service->property(QStringLiteral("X-KDE-PluginInfo-Name")).toString();
    const QString scriptName = service->property(QStringLiteral("X-Plasma-MainScript")).toString();
    return QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral(KWIN_NAME) + QStringLiteral("/desktoptabbox/") + pluginName + QStringLiteral("/contents/") + scriptName);
}

void DeclarativeView::slotEmbeddedChanged(bool enabled)
{
    if (enabled) {
        // cache the width
        setResizeMode(QQuickView::SizeRootObjectToView);
        m_cachedWidth = rootObject()->property("width").toInt();
        m_cachedHeight = rootObject()->property("height").toInt();
    } else {
        setResizeMode(QQuickView::SizeViewToRootObject);
        if (m_cachedWidth != 0 && m_cachedHeight != 0) {
            rootObject()->setProperty("width", m_cachedWidth);
            rootObject()->setProperty("height", m_cachedHeight);
        }
        updateQmlSource(true);
    }
}

void DeclarativeView::slotWindowChanged(WId wId, unsigned int properties)
{
    if (wId != tabBox->embedded()) {
        return;
    }
    if (properties & NET::WMGeometry) {
        slotUpdateGeometry();
    }
}

bool DeclarativeView::sendKeyEvent(QKeyEvent *e)
{
    if (QQuickItem *item = rootObject()->findChild<QQuickItem*>(QStringLiteral("listView"))) {
        return sendEvent(item, e);
    }
    return false;
}

} // namespace TabBox
} // namespace KWin
