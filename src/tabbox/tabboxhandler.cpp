/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "tabboxhandler.h"

#include "config-kwin.h"

// own
#include "clientmodel.h"
#include "scripting/scripting.h"
#include "switcheritem.h"
#include "tabbox_logging.h"
#include "window.h"
// Qt
#include <QKeyEvent>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <qpa/qwindowsysteminterface.h>
// KDE
#include <KLocalizedString>
#include <KPackage/Package>
#include <KPackage/PackageLoader>
#include <KProcess>

namespace KWin
{
namespace TabBox
{

class TabBoxHandlerPrivate
{
public:
    TabBoxHandlerPrivate(TabBoxHandler *q);

    ~TabBoxHandlerPrivate();

    /**
     * Updates the current highlight window state
     */
    void updateHighlightWindows();
    /**
     * Ends window highlighting
     */
    void endHighlightWindows(bool abort = false);

    void show();
    QQuickWindow *window() const;
    SwitcherItem *switcherItem() const;

    ClientModel *clientModel() const;

    bool isHighlightWindows() const;

    TabBoxHandler *q; // public pointer
    // members
    TabBoxConfig config;
    std::unique_ptr<QQmlContext> m_qmlContext;
    std::unique_ptr<QQmlComponent> m_qmlComponent;
    QObject *m_mainItem;
    QMap<QString, QObject *> m_clientTabBoxes;
    ClientModel *m_clientModel;
    QModelIndex index;
    /**
     * Indicates if the tabbox is shown.
     */
    bool isShown;
    Window *lastRaisedClient, *lastRaisedClientSucc;
    int wheelAngleDelta = 0;

private:
    QObject *createSwitcherItem();
};

TabBoxHandlerPrivate::TabBoxHandlerPrivate(TabBoxHandler *q)
    : m_qmlContext()
    , m_qmlComponent(nullptr)
    , m_mainItem(nullptr)
{
    this->q = q;
    isShown = false;
    lastRaisedClient = nullptr;
    lastRaisedClientSucc = nullptr;
    config = TabBoxConfig();
    m_clientModel = new ClientModel(q);
}

TabBoxHandlerPrivate::~TabBoxHandlerPrivate()
{
    qDeleteAll(m_clientTabBoxes);
}

QQuickWindow *TabBoxHandlerPrivate::window() const
{
    if (!m_mainItem) {
        return nullptr;
    }
    if (QQuickWindow *w = qobject_cast<QQuickWindow *>(m_mainItem)) {
        return w;
    }
    return m_mainItem->findChild<QQuickWindow *>();
}

#ifndef KWIN_UNIT_TEST
SwitcherItem *TabBoxHandlerPrivate::switcherItem() const
{
    if (!m_mainItem) {
        return nullptr;
    }
    if (SwitcherItem *i = qobject_cast<SwitcherItem *>(m_mainItem)) {
        return i;
    } else if (QQuickWindow *w = qobject_cast<QQuickWindow *>(m_mainItem)) {
        return w->contentItem()->findChild<SwitcherItem *>();
    }
    return m_mainItem->findChild<SwitcherItem *>();
}
#endif

ClientModel *TabBoxHandlerPrivate::clientModel() const
{
    return m_clientModel;
}

bool TabBoxHandlerPrivate::isHighlightWindows() const
{
    const QQuickWindow *w = window();
    if (w && w->visibility() == QWindow::FullScreen) {
        return false;
    }
    return config.isHighlightWindows();
}

void TabBoxHandlerPrivate::updateHighlightWindows()
{
    if (!isShown) {
        return;
    }

    Window *currentClient = q->client(index);
    QWindow *w = window();

    if (q->isKWinCompositing()) {
        if (lastRaisedClient) {
            q->elevateClient(lastRaisedClient, w, false);
        }
        lastRaisedClient = currentClient;
        // don't elevate desktop
        const auto desktop = q->desktopClient();
        if (currentClient && (!desktop || currentClient->internalId() != desktop->internalId())) {
            q->elevateClient(currentClient, w, true);
        }
    } else {
        if (lastRaisedClient) {
            q->shadeClient(lastRaisedClient, true);
            if (lastRaisedClientSucc) {
                q->restack(lastRaisedClient, lastRaisedClientSucc);
            }
            // TODO lastRaisedClient->setMinimized( lastRaisedClientWasMinimized );
        }

        lastRaisedClient = currentClient;
        if (lastRaisedClient) {
            q->shadeClient(lastRaisedClient, false);
            // TODO if ( (lastRaisedClientWasMinimized = lastRaisedClient->isMinimized()) )
            //         lastRaisedClient->setMinimized( false );
            QList<Window *> order = q->stackingOrder();
            int succIdx = order.count() + 1;
            for (int i = 0; i < order.count(); ++i) {
                if (order.at(i) == lastRaisedClient) {
                    succIdx = i + 1;
                    break;
                }
            }
            lastRaisedClientSucc = (succIdx < order.count()) ? order.at(succIdx) : nullptr;
            q->raiseClient(lastRaisedClient);
        }
    }

    if (config.isShowTabBox() && w) {
        q->highlightWindows(currentClient, w);
    } else {
        q->highlightWindows(currentClient);
    }
}

void TabBoxHandlerPrivate::endHighlightWindows(bool abort)
{
    Window *currentClient = q->client(index);
    if (isHighlightWindows() && q->isKWinCompositing()) {
        const auto stackingOrder = q->stackingOrder();
        for (Window *window : stackingOrder) {
            if (window != currentClient) { // to not mess up with wanted ShadeActive/ShadeHover state
                q->shadeClient(window, true);
            }
        }
    }
    QWindow *w = window();
    if (currentClient) {
        q->elevateClient(currentClient, w, false);
    }
    if (abort && lastRaisedClient && lastRaisedClientSucc) {
        q->restack(lastRaisedClient, lastRaisedClientSucc);
    }
    lastRaisedClient = nullptr;
    lastRaisedClientSucc = nullptr;
    // highlight windows
    q->highlightWindows();
}

#ifndef KWIN_UNIT_TEST
QObject *TabBoxHandlerPrivate::createSwitcherItem()
{
    // first try look'n'feel package
    QString file = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("plasma/look-and-feel/%1/contents/windowswitcher/WindowSwitcher.qml").arg(config.layoutName()));
    if (file.isNull()) {
        QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, KWIN_DATADIR + QLatin1String("/tabbox/") + config.layoutName(), QStandardPaths::LocateDirectory);
        if (path.isEmpty()) {
            path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QLatin1String("kwin/tabbox/") + config.layoutName(), QStandardPaths::LocateDirectory);
        }
        if (path.isEmpty()) {
            // load default
            qCWarning(KWIN_TABBOX) << "Could not load window switcher package" << config.layoutName() << ". Falling back to default";
            path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, KWIN_DATADIR + QLatin1String("/tabbox/") + TabBoxConfig::defaultLayoutName(), QStandardPaths::LocateDirectory);
        }

        KPackage::Package pkg = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("KWin/WindowSwitcher"));
        pkg.setPath(path);
        file = pkg.filePath("mainscript");
    }
    if (file.isNull()) {
        qCDebug(KWIN_TABBOX) << "Could not find QML file for window switcher";
        return nullptr;
    }
    m_qmlComponent->loadUrl(QUrl::fromLocalFile(file));
    if (m_qmlComponent->isError()) {
        qCWarning(KWIN_TABBOX) << "Component failed to load: " << m_qmlComponent->errors();
        QStringList args;
        args << QStringLiteral("--passivepopup") << i18n("The Window Switcher installation is broken, resources are missing.\n"
                                                         "Contact your distribution about this.")
             << QStringLiteral("20");
        KProcess::startDetached(QStringLiteral("kdialog"), args);
        m_qmlComponent.reset(nullptr);
    } else {
        QObject *object = m_qmlComponent->create(m_qmlContext.get());
        m_clientTabBoxes.insert(config.layoutName(), object);
        return object;
    }
    return nullptr;
}
#endif

void TabBoxHandlerPrivate::show()
{
#ifndef KWIN_UNIT_TEST
    if (!m_qmlContext) {
        qmlRegisterType<SwitcherItem>("org.kde.kwin", 3, 0, "TabBoxSwitcher");
        m_qmlContext = std::make_unique<QQmlContext>(Scripting::self()->qmlEngine());
    }
    if (!m_qmlComponent) {
        m_qmlComponent = std::make_unique<QQmlComponent>(Scripting::self()->qmlEngine());
    }
    auto findMainItem = [this](const QMap<QString, QObject *> &tabBoxes) -> QObject * {
        auto it = tabBoxes.constFind(config.layoutName());
        if (it != tabBoxes.constEnd()) {
            return it.value();
        }
        return nullptr;
    };
    m_mainItem = nullptr;
    m_mainItem = findMainItem(m_clientTabBoxes);
    if (!m_mainItem) {
        m_mainItem = createSwitcherItem();
        if (!m_mainItem) {
            return;
        }
    }
    if (SwitcherItem *item = switcherItem()) {
        // In case the model isn't yet set (see below), index will be reset and therefore we
        // need to save the current index row (https://bugs.kde.org/show_bug.cgi?id=333511).
        int indexRow = index.row();
        if (!item->model()) {
            item->setModel(clientModel());
        }
        item->setAllDesktops(config.clientDesktopMode() == TabBoxConfig::AllDesktopsClients);
        item->setCurrentIndex(indexRow);
        item->setNoModifierGrab(q->noModifierGrab());
        Q_EMIT item->aboutToShow();

        // When SwitcherItem gets hidden, destroy also the window and main item
        QObject::connect(item, &SwitcherItem::visibleChanged, q, [this, item]() {
            if (!item->isVisible()) {
                if (QQuickWindow *w = window()) {
                    w->hide();
                    w->destroy();
                }
                m_mainItem = nullptr;
            }
        });

        // everything is prepared, so let's make the whole thing visible
        item->setVisible(true);
    }
    if (QWindow *w = window()) {
        wheelAngleDelta = 0;
        w->installEventFilter(q);
        // pretend to activate the window to enable accessibility notifications
        QWindowSystemInterface::handleFocusWindowChanged(w, Qt::TabFocusReason);
    }
#endif
}

/***********************************************
 * TabBoxHandler
 ***********************************************/

TabBoxHandler::TabBoxHandler(QObject *parent)
    : QObject(parent)
{
    KWin::TabBox::tabBox = this;
    d = new TabBoxHandlerPrivate(this);
}

TabBoxHandler::~TabBoxHandler()
{
    delete d;
}

const KWin::TabBox::TabBoxConfig &TabBoxHandler::config() const
{
    return d->config;
}

void TabBoxHandler::setConfig(const TabBoxConfig &config)
{
    d->config = config;
    Q_EMIT configChanged();
}

void TabBoxHandler::show()
{
    d->isShown = true;
    d->lastRaisedClient = nullptr;
    d->lastRaisedClientSucc = nullptr;
    if (d->config.isShowTabBox()) {
        d->show();
    }
    if (d->isHighlightWindows()) {
        // TODO this should be
        // QMetaObject::invokeMethod(this, "initHighlightWindows", Qt::QueuedConnection);
        // but we somehow need to cross > 1 event cycle (likely because of queued invocation in the effects)
        // to ensure the EffectWindow is present when updateHighlightWindows, thus elevating the window/tabbox
        QTimer::singleShot(1, this, &TabBoxHandler::initHighlightWindows);
    }
}

void TabBoxHandler::initHighlightWindows()
{
    if (isKWinCompositing()) {
        const auto stack = stackingOrder();
        for (Window *window : stack) {
            shadeClient(window, false);
        }
    }
    d->updateHighlightWindows();
}

void TabBoxHandler::hide(bool abort)
{
    d->isShown = false;
    if (d->isHighlightWindows()) {
        d->endHighlightWindows(abort);
    }
#ifndef KWIN_UNIT_TEST
    if (SwitcherItem *item = d->switcherItem()) {
        Q_EMIT item->aboutToHide();
        if (item->automaticallyHide()) {
            item->setVisible(false);
        }
    }
#endif
}

QModelIndex TabBoxHandler::nextPrev(bool forward) const
{
    QModelIndex ret;
    QAbstractItemModel *model = d->clientModel();
    if (forward) {
        int column = d->index.column() + 1;
        int row = d->index.row();
        if (column == model->columnCount()) {
            column = 0;
            row++;
            if (row == model->rowCount()) {
                row = 0;
            }
        }
        ret = model->index(row, column);
        if (!ret.isValid()) {
            ret = model->index(0, 0);
        }
    } else {
        int column = d->index.column() - 1;
        int row = d->index.row();
        if (column < 0) {
            column = model->columnCount() - 1;
            row--;
            if (row < 0) {
                row = model->rowCount() - 1;
            }
        }
        ret = model->index(row, column);
        if (!ret.isValid()) {
            row = model->rowCount() - 1;
            for (int i = model->columnCount() - 1; i >= 0; i--) {
                ret = model->index(row, i);
                if (ret.isValid()) {
                    break;
                }
            }
        }
    }
    if (ret.isValid()) {
        return ret;
    } else {
        return d->index;
    }
}

void TabBoxHandler::setCurrentIndex(const QModelIndex &index)
{
    if (d->index == index) {
        return;
    }
    if (!index.isValid()) {
        return;
    }
    d->index = index;
    if (d->isHighlightWindows()) {
        d->updateHighlightWindows();
    }
    Q_EMIT selectedIndexChanged();
}

const QModelIndex &TabBoxHandler::currentIndex() const
{
    return d->index;
}

void TabBoxHandler::grabbedKeyEvent(QKeyEvent *event) const
{
    if (!d->m_mainItem || !d->window()) {
        return;
    }
    QCoreApplication::sendEvent(d->window(), event);
}

bool TabBoxHandler::containsPos(const QPoint &pos) const
{
    if (!d->m_mainItem) {
        return false;
    }
    QWindow *w = d->window();
    if (w) {
        return w->geometry().contains(pos);
    }
    return false;
}

QModelIndex TabBoxHandler::index(Window *client) const
{
    return d->clientModel()->index(client);
}

QList<Window *> TabBoxHandler::clientList() const
{
    return d->clientModel()->clientList();
}

Window *TabBoxHandler::client(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return nullptr;
    }
    Window *c = static_cast<Window *>(
        d->clientModel()->data(index, ClientModel::ClientRole).value<void *>());
    return c;
}

void TabBoxHandler::createModel(bool partialReset)
{
    d->clientModel()->createClientList(partialReset);
    // TODO: C++11 use lambda function
    bool lastRaised = false;
    bool lastRaisedSucc = false;
    const auto clients = stackingOrder();
    for (Window *window : clients) {
        if (window == d->lastRaisedClient) {
            lastRaised = true;
        }
        if (window == d->lastRaisedClientSucc) {
            lastRaisedSucc = true;
        }
    }
    if (d->lastRaisedClient && !lastRaised) {
        d->lastRaisedClient = nullptr;
    }
    if (d->lastRaisedClientSucc && !lastRaisedSucc) {
        d->lastRaisedClientSucc = nullptr;
    }
}

QModelIndex TabBoxHandler::first() const
{
    return d->clientModel()->index(0, 0);
}

bool TabBoxHandler::eventFilter(QObject *watched, QEvent *e)
{
    if (e->type() == QEvent::Wheel && watched == d->window()) {
        QWheelEvent *event = static_cast<QWheelEvent *>(e);
        // On x11 the delta for vertical scrolling might also be on X for whatever reason
        const int delta = std::abs(event->angleDelta().x()) > std::abs(event->angleDelta().y()) ? event->angleDelta().x() : event->angleDelta().y();
        d->wheelAngleDelta += delta;
        while (d->wheelAngleDelta <= -120) {
            d->wheelAngleDelta += 120;
            const QModelIndex index = nextPrev(true);
            if (index.isValid()) {
                setCurrentIndex(index);
            }
        }
        while (d->wheelAngleDelta >= 120) {
            d->wheelAngleDelta -= 120;
            const QModelIndex index = nextPrev(false);
            if (index.isValid()) {
                setCurrentIndex(index);
            }
        }
        return true;
    }
    // pass on
    return QObject::eventFilter(watched, e);
}

TabBoxHandler *tabBox = nullptr;

} // namespace TabBox
} // namespace KWin

#include "moc_tabboxhandler.cpp"
