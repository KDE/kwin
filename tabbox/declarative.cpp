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
// include KDE
#include <KDE/KDebug>
#include <KDE/KIconEffect>
#include <KDE/KIconLoader>
#include <KDE/KStandardDirs>
#include <KDE/Plasma/FrameSvg>
#include <KDE/Plasma/Theme>
#include <KDE/Plasma/WindowEffects>
#include <kdeclarative.h>
#include <kephal/screens.h>

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
    TabBoxClient* client = static_cast< TabBoxClient* >(index.model()->data(index, ClientModel::ClientRole).value<void *>());

    QSize s(32, 32);
    if (requestedSize.isValid()) {
        s = requestedSize;
    }
    *size = s;
    QPixmap icon = client->icon(s);
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

DeclarativeView::DeclarativeView(QAbstractItemModel *model, QWidget *parent)
    : QDeclarativeView(parent)
    , m_model(model)
    , m_currentScreenGeometry()
    , m_frame(new Plasma::FrameSvg(this))
    , m_currentLayout()
{
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::X11BypassWindowManagerHint);
    setResizeMode(QDeclarativeView::SizeViewToRootObject);
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
    rootContext()->setContextProperty("clientModel", model);
    setSource(QUrl(KStandardDirs::locate("data", "kwin/tabbox/tabbox.qml")));

    // FrameSvg
    m_frame->setImagePath("dialogs/background");
    m_frame->setCacheAllRenderedFrames(true);
    m_frame->setEnabledBorders(Plasma::FrameSvg::AllBorders);

    connect(tabBox, SIGNAL(configChanged()), SLOT(updateQmlSource()));
}

void DeclarativeView::showEvent(QShowEvent *event)
{
    updateQmlSource();
    m_currentScreenGeometry = Kephal::ScreenUtils::screenGeometry(tabBox->activeScreen());
    rootObject()->setProperty("screenWidth", m_currentScreenGeometry.width());
    rootObject()->setProperty("screenHeight", m_currentScreenGeometry.height());
    rootObject()->setProperty("allDesktops", tabBox->config().tabBoxMode() == TabBoxConfig::ClientTabBox &&
        ((tabBox->config().clientListMode() == TabBoxConfig::AllDesktopsClientList) ||
        (tabBox->config().clientListMode() == TabBoxConfig::AllDesktopsApplicationList)));
    rootObject()->setProperty("longestCaption", static_cast<ClientModel*>(m_model)->longestCaption());

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
    if (Plasma::Theme::defaultTheme()->windowTranslucencyEnabled()) {
        // blur background
        Plasma::WindowEffects::enableBlurBehind(winId(), true, m_frame->mask());
        Plasma::WindowEffects::overrideShadow(winId(), true);
    } else {
        // do not trim to mask with compositing enabled, otherwise shadows are cropped
        setMask(m_frame->mask());
    }
    QDeclarativeView::resizeEvent(event);
}

void DeclarativeView::slotUpdateGeometry()
{
    const int width = rootObject()->property("width").toInt();
    const int height = rootObject()->property("height").toInt();
    setGeometry(m_currentScreenGeometry.x() + static_cast<qreal>(m_currentScreenGeometry.width()) * 0.5 - static_cast<qreal>(width) * 0.5,
        m_currentScreenGeometry.y() + static_cast<qreal>(m_currentScreenGeometry.height()) * 0.5 - static_cast<qreal>(height) * 0.5,
        width, height);
}

void DeclarativeView::setCurrentIndex(const QModelIndex &index)
{
    if (QObject *item = rootObject()->findChild<QObject*>("listView")) {
        item->setProperty("currentIndex", index.row());
    }
}

QModelIndex DeclarativeView::indexAt(const QPoint &pos) const
{
    if (QObject *item = rootObject()->findChild<QObject*>("listView")) {
        QVariant returnedValue;
        QVariant xPos(pos.x());
        QVariant yPos(pos.y());
        QMetaObject::invokeMethod(item, "indexAtMousePos", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, QVariant(pos)));
        if (!returnedValue.canConvert<int>()) {
            return QModelIndex();
        }
        return m_model->index(returnedValue.toInt(), 0);
    }
    return QModelIndex();
}

void DeclarativeView::currentIndexChanged(int row)
{
    tabBox->setCurrentIndex(m_model->index(row, 0));
}

void DeclarativeView::updateQmlSource()
{
    if (tabBox->config().layoutName() == m_currentLayout) {
        return;
    }
    m_currentLayout = tabBox->config().layoutName();
    QString file = KStandardDirs::locate("data", "kwin/tabbox/" + m_currentLayout.toLower().replace(' ', '_') + ".qml");
    if (file.isNull()) {
        // fallback to default
        file = KStandardDirs::locate("data", "kwin/tabbox/informative.qml");
    }
    rootObject()->setProperty("source", QUrl(file));
}

} // namespace TabBox
} // namespace KWin
