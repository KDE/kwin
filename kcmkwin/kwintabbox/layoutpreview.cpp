/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009, 2011 Martin Gräßlin <mgraesslin@kde.org>

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
#include "layoutpreview.h"
#include "thumbnailitem.h"
#include <QQmlContext>
#include <QtCore/QStandardPaths>
#include <KDE/KConfigGroup>
#include <KDE/KDesktopFile>
#include <KDE/KIconEffect>
#include <KDE/KIconLoader>
#include <KDE/KLocalizedString>
#include <KDE/KService>

namespace KWin
{
namespace TabBox
{

LayoutPreview::LayoutPreview(QWindow *parent)
    : QQuickView(parent)
{
    setColor(Qt::white);
    setMinimumSize(QSize(480, 300));
    setResizeMode(QQuickView::SizeRootObjectToView);
    foreach (const QString &importPath, QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, "kwin/tabbox", QStandardPaths::LocateDirectory)) {
        engine()->addImportPath(importPath);
    }
    ExampleClientModel *model = new ExampleClientModel(this);
    engine()->addImageProvider(QLatin1String("client"), new TabBoxImageProvider(model));
    qmlRegisterType<WindowThumbnailItem>("org.kde.kwin", 0, 1, "ThumbnailItem");
    rootContext()->setContextProperty("clientModel", model);
    rootContext()->setContextProperty("sourcePath", QString());
    rootContext()->setContextProperty("name", QString());
    setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kwin/kcm_kwintabbox/main.qml")));
}

LayoutPreview::~LayoutPreview()
{
}

void LayoutPreview::setLayout(const QString &path, const QString &name)
{
    rootContext()->setContextProperty("sourcePath", path);
    rootContext()->setContextProperty("name", name);
}

TabBoxImageProvider::TabBoxImageProvider(QAbstractListModel* model)
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
    , m_model(model)
{
}

QPixmap TabBoxImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    bool ok = false;
    QStringList parts = id.split('/');
    const int index = parts.first().toInt(&ok);
    if (!ok) {
        return QQuickImageProvider::requestPixmap(id, size, requestedSize);
    }
    QSize s(32, 32);
    if (requestedSize.isValid()) {
        s = requestedSize;
    }
    *size = s;
    QPixmap icon(QIcon::fromTheme(m_model->data(m_model->index(index), Qt::UserRole+3).toString()).pixmap(s));
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

ExampleClientModel::ExampleClientModel (QObject* parent)
    : QAbstractListModel (parent)
{
    QHash<int, QByteArray> roles;
    roles[Qt::UserRole] = "caption";
    roles[Qt::UserRole+1] = "minimized";
    roles[Qt::UserRole+2] = "desktopName";
    roles[Qt::UserRole+4] = "windowId";
    setRoleNames(roles);
    init();
}

ExampleClientModel::~ExampleClientModel()
{
}

void ExampleClientModel::init()
{
    QList<QString> applications;
    applications << "konqbrowser" << "KMail2" << "systemsettings" << "dolphin";

    foreach (const QString& application, applications) {
        KService::Ptr service = KService::serviceByStorageId("kde4-" + application + ".desktop");
        if (service) {
            m_nameList << service->entryPath();
        }
    }
}

QVariant ExampleClientModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    switch (role) {
    case Qt::DisplayRole:
    case Qt::UserRole:
        return KDesktopFile(m_nameList.at(index.row())).readName();
    case Qt::UserRole+1:
        return false;
    case Qt::UserRole+2:
        return i18nc("An example Desktop Name", "Desktop 1");
    case Qt::UserRole+3:
        return KDesktopFile(m_nameList.at(index.row())).readIcon();
    case Qt::UserRole+4:
        const QString desktopFile = KDesktopFile(m_nameList.at(index.row())).fileName().split('/').last();
        if (desktopFile == "konqbrowser.desktop") {
            return WindowThumbnailItem::Konqueror;
        } else if (desktopFile == "KMail2.desktop") {
            return WindowThumbnailItem::KMail;
        } else if (desktopFile == "systemsettings.desktop") {
            return WindowThumbnailItem::Systemsettings;
        } else if (desktopFile == "dolphin.desktop") {
            return WindowThumbnailItem::Dolphin;
        }
        return 0;
    }
    return QVariant();
}

int ExampleClientModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_nameList.size();
}

} // namespace KWin
} // namespace TabBox

