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
#include <QtDeclarative/qdeclarative.h>
#include <QtDeclarative/QDeclarativeContext>
#include <QtDeclarative/QDeclarativeEngine>
#include <QtGui/QGraphicsObject>
#include <QtGui/QResizeEvent>
#include <QX11Info>

// include KDE
#include <KDE/KDebug>
#include <KDE/KIconEffect>
#include <KDE/KIconLoader>
#include <KDE/KServiceTypeTrader>
#include <KDE/KStandardDirs>
#include <KDE/Plasma/FrameSvg>
#include <KDE/Plasma/Theme>
#include <KDE/Plasma/WindowEffects>
#include <kdeclarative.h>
#include <kephal/screens.h>
// KWin
#include "thumbnailitem.h"
#include <kwindowsystem.h>
#include "../client.h"
#include "../workspace.h"

namespace KWin
{
namespace TabBox
{

ImageProvider::ImageProvider(QAbstractItemModel *model)
    : QDeclarativeImageProvider(QDeclarativeImageProvider::Pixmap)
    , m_model(model)
{
}

QPixmap ImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    bool ok = false;
    QStringList parts = id.split('/');
    const int row = parts.first().toInt(&ok);
    if (!ok) {
        return QDeclarativeImageProvider::requestPixmap(id, size, requestedSize);
    }
    const QModelIndex index = m_model->index(row, 0);
    if (!index.isValid()) {
        return QDeclarativeImageProvider::requestPixmap(id, size, requestedSize);
    }
    if (index.model()->data(index, ClientModel::EmptyRole).toBool()) {
        return QDeclarativeImageProvider::requestPixmap(id, size, requestedSize);
    }
    TabBoxClient* client = static_cast< TabBoxClient* >(index.model()->data(index, ClientModel::ClientRole).value<void *>());
    if (!client) {
        return QDeclarativeImageProvider::requestPixmap(id, size, requestedSize);
    }

    QSize s(32, 32);
    if (requestedSize.isValid()) {
        s = requestedSize;
    }
    *size = s;
    QPixmap icon = client->icon(s);
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
        if (parts.at(2) == QLatin1String("selected")) {
            state = KIconLoader::ActiveState;
        } else if (parts.at(2) == QLatin1String("disabled")) {
            state = KIconLoader::DisabledState;
        }
        icon = effect->apply(icon, KIconLoader::Desktop, state);
    }
    return icon;
}

DeclarativeView::DeclarativeView(QAbstractItemModel *model, TabBoxConfig::TabBoxMode mode, QWidget *parent)
    : QDeclarativeView(parent)
    , m_model(model)
    , m_mode(mode)
    , m_currentScreenGeometry()
    , m_frame(new Plasma::FrameSvg(this))
    , m_currentLayout()
    , m_cachedWidth(0)
    , m_cachedHeight(0)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::X11BypassWindowManagerHint);
    if (tabBox->embedded()) {
        setResizeMode(QDeclarativeView::SizeRootObjectToView);
    } else {
        setResizeMode(QDeclarativeView::SizeViewToRootObject);
    }
    QPalette pal = palette();
    pal.setColor(backgroundRole(), Qt::transparent);
    setPalette(pal);
    foreach (const QString &importPath, KGlobal::dirs()->findDirs("module", "imports")) {
        engine()->addImportPath(importPath);
    }
    engine()->addImageProvider(QLatin1String("client"), new ImageProvider(model));
    KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
    kdeclarative.initialize();
    kdeclarative.setupBindings();
    qmlRegisterType<ThumbnailItem>("org.kde.kwin", 0, 1, "ThumbnailItem");
    rootContext()->setContextProperty("viewId", static_cast<qulonglong>(winId()));
    if (m_mode == TabBoxConfig::ClientTabBox) {
        rootContext()->setContextProperty("clientModel", model);
    } else if (m_mode == TabBoxConfig::DesktopTabBox) {
        rootContext()->setContextProperty("clientModel", model);
    }
    setSource(QUrl(KStandardDirs::locate("data", "kwin/tabbox/tabbox.qml")));

    // FrameSvg
    m_frame->setImagePath("dialogs/background");
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
    m_currentScreenGeometry = Kephal::ScreenUtils::screenGeometry(tabBox->activeScreen());
    rootObject()->setProperty("screenWidth", m_currentScreenGeometry.width());
    rootObject()->setProperty("screenHeight", m_currentScreenGeometry.height());
    rootObject()->setProperty("allDesktops", tabBox->config().tabBoxMode() == TabBoxConfig::ClientTabBox &&
        ((tabBox->config().clientListMode() == TabBoxConfig::AllDesktopsClientList) ||
        (tabBox->config().clientListMode() == TabBoxConfig::AllDesktopsApplicationList)));
    if (ClientModel *clientModel = qobject_cast<ClientModel*>(m_model)) {
        rootObject()->setProperty("longestCaption", clientModel->longestCaption());
    }

    if (QObject *item = rootObject()->findChild<QObject*>("listView")) {
        item->setProperty("currentIndex", tabBox->first().row());
        connect(item, SIGNAL(currentIndexChanged(int)), SLOT(currentIndexChanged(int)));
    }
    slotUpdateGeometry();
    QGraphicsView::showEvent(event);
}

void DeclarativeView::resizeEvent(QResizeEvent *event)
{
    m_frame->resizeFrame(event->size());
    if (Plasma::Theme::defaultTheme()->windowTranslucencyEnabled() && !tabBox->embedded()) {
        // blur background
        Plasma::WindowEffects::enableBlurBehind(winId(), true, m_frame->mask());
        Plasma::WindowEffects::overrideShadow(winId(), true);
    } else if (tabBox->embedded()) {
        Plasma::WindowEffects::enableBlurBehind(winId(), false);
    } else {
        // do not trim to mask with compositing enabled, otherwise shadows are cropped
        setMask(m_frame->mask());
    }
    QDeclarativeView::resizeEvent(event);
}

void DeclarativeView::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
#ifndef TABBOX_KCM
    if (tabBox->embedded()) {
        Client *c = Workspace::self()->findClient(WindowMatchPredicate(tabBox->embedded()));
        if (c) {
            disconnect(c, SIGNAL(geometryChanged()), this, SLOT(slotUpdateGeometry()));
        }
    }
#endif
}

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
        setGeometry(m_currentScreenGeometry.x() + static_cast<qreal>(m_currentScreenGeometry.width()) * 0.5 - static_cast<qreal>(width) * 0.5,
            m_currentScreenGeometry.y() + static_cast<qreal>(m_currentScreenGeometry.height()) * 0.5 - static_cast<qreal>(height) * 0.5,
            width, height);
        m_relativePos = pos();
    }
}

void DeclarativeView::setCurrentIndex(const QModelIndex &index)
{
    if (tabBox->config().tabBoxMode() != m_mode) {
        return;
    }
    if (QObject *item = rootObject()->findChild<QObject*>("listView")) {
        item->setProperty("currentIndex", index.row());
    }
}

void DeclarativeView::currentIndexChanged(int row)
{
    tabBox->setCurrentIndex(m_model->index(row, 0));
    KWindowSystem::forceActiveWindow(m_model->data(m_model->index(row, 0), ClientModel::WIdRole).toLongLong());
}

void DeclarativeView::updateQmlSource(bool force)
{
    if (tabBox->config().tabBoxMode() != m_mode) {
        return;
    }
    if (!force && tabBox->config().layoutName() == m_currentLayout) {
        return;
    }
    if (m_mode == TabBoxConfig::DesktopTabBox) {
        m_currentLayout = tabBox->config().layoutName();
        const QString file = KStandardDirs::locate("data", "kwin/tabbox/desktop.qml");
        rootObject()->setProperty("source", QUrl(file));
        return;
    }
    QString constraint = QString("[X-KDE-PluginInfo-Name] == '%1'").arg(tabBox->config().layoutName());
    KService::List offers = KServiceTypeTrader::self()->query("KWin/WindowSwitcher", constraint);
    if (offers.isEmpty()) {
        // load default
        constraint = QString("[X-KDE-PluginInfo-Name] == '%1'").arg("informative");
        offers = KServiceTypeTrader::self()->query("KWin/WindowSwitcher", constraint);
        if (offers.isEmpty()) {
            kDebug(1212) << "could not find default window switcher layout";
            return;
        }
    }
    m_currentLayout = tabBox->config().layoutName();
    KService::Ptr service = offers.first();
    const QString pluginName = service->property("X-KDE-PluginInfo-Name").toString();
    if (service->property("X-Plasma-API").toString() != "declarativeappletscript") {
        kDebug(1212) << "Window Switcher Layout is no declarativeappletscript";
        return;
    }
    const QString scriptName = service->property("X-Plasma-MainScript").toString();
    const QString file = KStandardDirs::locate("data", "kwin/tabbox/" + pluginName + "/contents/" + scriptName);
    if (file.isNull()) {
        kDebug(1212) << "Could not find QML file for window switcher";
        return;
    }
    rootObject()->setProperty("source", QUrl(file));
}

void DeclarativeView::slotEmbeddedChanged(bool enabled)
{
    if (enabled) {
        // cache the width
        setResizeMode(QDeclarativeView::SizeRootObjectToView);
        m_cachedWidth = rootObject()->property("width").toInt();
        m_cachedHeight = rootObject()->property("height").toInt();
    } else {
        setResizeMode(QDeclarativeView::SizeViewToRootObject);
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

} // namespace TabBox
} // namespace KWin
