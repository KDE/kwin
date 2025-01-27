/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "layoutpreview.h"

#include <KConfigGroup>
#include <KDesktopFile>
#include <KLocalizedString>
#include <QApplication>
#include <QDebug>
#include <QQmlContext>
#include <QQmlEngine>
#include <QScreen>
#include <QStandardPaths>
#include <QStringLiteral>

namespace KWin
{
namespace TabBox
{

LayoutPreview::LayoutPreview(const QString &path, bool showDesktopThumbnail, QObject *parent)
    : QObject(parent)
    , m_item(nullptr)
{
    QQmlEngine *engine = new QQmlEngine(this);
    QQmlComponent *component = new QQmlComponent(engine, this);
    qmlRegisterType<WindowThumbnailItem>("org.kde.kwin", 3, 0, "WindowThumbnail");
    qmlRegisterType<SwitcherItem>("org.kde.kwin", 3, 0, "TabBoxSwitcher");
    qmlRegisterType<DesktopBackground>("org.kde.kwin", 3, 0, "DesktopBackground");
    qmlRegisterAnonymousType<QAbstractItemModel>("org.kde.kwin", 3);
    component->loadUrl(QUrl::fromLocalFile(path));
    if (component->isError()) {
        qDebug() << component->errorString();
    }
    QObject *item = component->create();
    auto findSwitcher = [item]() -> SwitcherItem * {
        if (!item) {
            return nullptr;
        }
        if (SwitcherItem *i = qobject_cast<SwitcherItem *>(item)) {
            return i;
        } else if (QQuickWindow *w = qobject_cast<QQuickWindow *>(item)) {
            return w->contentItem()->findChild<SwitcherItem *>();
        }
        return item->findChild<SwitcherItem *>();
    };
    if (SwitcherItem *switcher = findSwitcher()) {
        m_item = switcher;
        static_cast<ExampleClientModel *>(switcher->model())->showDesktopThumbnail(showDesktopThumbnail);
        switcher->setVisible(true);
    }
    auto findWindow = [item]() -> QQuickWindow * {
        if (!item) {
            return nullptr;
        }
        if (QQuickWindow *w = qobject_cast<QQuickWindow *>(item)) {
            return w;
        }
        return item->findChild<QQuickWindow *>();
    };
    if (QQuickWindow *w = findWindow()) {
        w->setKeyboardGrabEnabled(true);
        w->installEventFilter(this);
    }
}

LayoutPreview::~LayoutPreview()
{
}

bool LayoutPreview::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape || keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Space) {
            object->deleteLater();
            deleteLater();
        }
        if (m_item && keyEvent->key() == Qt::Key_Tab) {
            m_item->incrementIndex();
        }
        if (m_item && keyEvent->key() == Qt::Key_Backtab) {
            m_item->decrementIndex();
        }
    } else if (event->type() == QEvent::FocusOut) {
        object->deleteLater();
        deleteLater();
    }
    return QObject::eventFilter(object, event);
}

ExampleClientModel::ExampleClientModel(QObject *parent)
    : QAbstractListModel(parent)
{
    init();
}

ExampleClientModel::~ExampleClientModel()
{
}

void ExampleClientModel::init()
{
    m_thumbnails << ThumbnailInfo{
        WindowThumbnailItem::Dolphin,
        i18nc("The name of KDE's file manager in this language, if translated", "Dolphin"),
        QStringLiteral("system-file-manager")};
    m_thumbnails << ThumbnailInfo{
        WindowThumbnailItem::Konqueror,
        i18nc("The name of KDE's web browser in this language, if translated", "Konqueror"),
        QStringLiteral("konqueror")};
    m_thumbnails << ThumbnailInfo{
        WindowThumbnailItem::KMail,
        i18nc("The name of KDE's email client in this language, if translated", "KMail"),
        QStringLiteral("kmail")};
    m_thumbnails << ThumbnailInfo{
        WindowThumbnailItem::Systemsettings,
        i18nc("The name of KDE's System Settings app in this language, if translated", "System Settings"),
        QStringLiteral("systemsettings")};
}

void ExampleClientModel::showDesktopThumbnail(bool showDesktop)
{
    const ThumbnailInfo desktopThumbnail = ThumbnailInfo{WindowThumbnailItem::Desktop, i18n("Show Desktop"), QStringLiteral("desktop")};
    const int desktopIndex = m_thumbnails.indexOf(desktopThumbnail);
    if (showDesktop == (desktopIndex >= 0)) {
        return;
    }

    Q_EMIT beginResetModel();
    if (showDesktop) {
        m_thumbnails << desktopThumbnail;
    } else {
        m_thumbnails.removeAt(desktopIndex);
    }
    Q_EMIT endResetModel();
}

QVariant ExampleClientModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount()) {
        return QVariant();
    }

    const ThumbnailInfo &item = m_thumbnails.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
    case CaptionRole:
        return item.caption;
    case MinimizedRole:
        return false;
    case DesktopNameRole:
        return i18nc("An example Desktop Name", "Desktop 1");
    case IconRole:
        return item.icon;
    case WindowIdRole:
        return item.wId;
    case CloseableRole:
        return item.wId != WindowThumbnailItem::Desktop;
    }
    return QVariant();
}

QString ExampleClientModel::longestCaption() const
{
    QString caption;
    for (const auto &item : m_thumbnails) {
        if (item.caption.size() > caption.size()) {
            caption = item.caption;
        }
    }
    return caption;
}

int ExampleClientModel::rowCount(const QModelIndex &parent) const
{
    return m_thumbnails.size();
}

QHash<int, QByteArray> ExampleClientModel::roleNames() const
{
    return {
        {CaptionRole, QByteArrayLiteral("caption")},
        {MinimizedRole, QByteArrayLiteral("minimized")},
        {DesktopNameRole, QByteArrayLiteral("desktopName")},
        {IconRole, QByteArrayLiteral("icon")},
        {WindowIdRole, QByteArrayLiteral("windowId")},
        {CloseableRole, QByteArrayLiteral("closeable")},
    };
}

SwitcherItem::SwitcherItem(QObject *parent)
    : QObject(parent)
    , m_model(new ExampleClientModel(this))
    , m_item(nullptr)
    , m_currentIndex(0)
    , m_visible(false)
{
}

SwitcherItem::~SwitcherItem()
{
}

void SwitcherItem::setVisible(bool visible)
{
    if (m_visible == visible) {
        return;
    }
    m_visible = visible;
    Q_EMIT visibleChanged();
}

void SwitcherItem::setItem(QObject *item)
{
    m_item = item;
    Q_EMIT itemChanged();
}

void SwitcherItem::setCurrentIndex(int index)
{
    if (m_currentIndex == index) {
        return;
    }
    m_currentIndex = index;
    Q_EMIT currentIndexChanged(m_currentIndex);
}

QRect SwitcherItem::screenGeometry() const
{
    const QScreen *primaryScreen = qApp->primaryScreen();
    return primaryScreen->geometry();
}

void SwitcherItem::incrementIndex()
{
    setCurrentIndex((m_currentIndex + 1) % m_model->rowCount());
}

void SwitcherItem::decrementIndex()
{
    int index = m_currentIndex - 1;
    if (index < 0) {
        index = m_model->rowCount() - 1;
    }
    setCurrentIndex(index);
}

DesktopBackground::DesktopBackground(QQuickItem *parent)
    : WindowThumbnailItem(parent)
{
    setWId(WindowThumbnailItem::Desktop);

    connect(this, &QQuickItem::windowChanged, this, &DesktopBackground::stretchToScreen);
    stretchToScreen();
};

void DesktopBackground::stretchToScreen()
{
    const QQuickWindow *w = window();
    if (!w) {
        return;
    }
    const QScreen *screen = w->screen();
    if (!screen) {
        return;
    }
    setImplicitSize(screen->size().width(), screen->size().height());
};

} // namespace KWin
} // namespace TabBox

#include "moc_layoutpreview.cpp"
