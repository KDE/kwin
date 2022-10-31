/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "clientmodel.h"

#include <config-kwin.h>

#if KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "core/output.h"
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"

namespace KWin::ScriptingModels::V2
{

static quint32 nextId()
{
    static quint32 counter = 0;
    return ++counter;
}

ClientLevel::ClientLevel(ClientModel *model, AbstractLevel *parent)
    : AbstractLevel(model, parent)
{
#if KWIN_BUILD_ACTIVITIES
    if (Activities *activities = Workspace::self()->activities()) {
        connect(activities, &Activities::currentChanged, this, &ClientLevel::reInit);
    }
#endif
    connect(VirtualDesktopManager::self(), &VirtualDesktopManager::currentChanged, this, &ClientLevel::reInit);
    connect(Workspace::self(), &Workspace::windowAdded, this, &ClientLevel::clientAdded);
    connect(Workspace::self(), &Workspace::windowRemoved, this, &ClientLevel::clientRemoved);
    connect(model, &ClientModel::exclusionsChanged, this, &ClientLevel::reInit);
}

ClientLevel::~ClientLevel()
{
}

void ClientLevel::clientAdded(Window *client)
{
    setupClientConnections(client);
    checkClient(client);
}

void ClientLevel::clientRemoved(Window *client)
{
    removeClient(client);
}

void ClientLevel::setupClientConnections(Window *client)
{
    auto check = [this, client] {
        checkClient(client);
    };
    connect(client, &Window::desktopChanged, this, check);
    connect(client, &Window::screenChanged, this, check);
    connect(client, &Window::activitiesChanged, this, check);
    connect(client, &Window::windowHidden, this, check);
    connect(client, &Window::windowShown, this, check);
}

void ClientLevel::checkClient(Window *client)
{
    const bool shouldInclude = !exclude(client) && shouldAdd(client);
    const bool contains = containsClient(client);

    if (shouldInclude && !contains) {
        addClient(client);
    } else if (!shouldInclude && contains) {
        removeClient(client);
    }
}

bool ClientLevel::exclude(Window *client) const
{
    ClientModel::Exclusions exclusions = model()->exclusions();
    if (exclusions == ClientModel::NoExclusion) {
        return false;
    }
    if (exclusions & ClientModel::DesktopWindowsExclusion) {
        if (client->isDesktop()) {
            return true;
        }
    }
    if (exclusions & ClientModel::DockWindowsExclusion) {
        if (client->isDock()) {
            return true;
        }
    }
    if (exclusions & ClientModel::UtilityWindowsExclusion) {
        if (client->isUtility()) {
            return true;
        }
    }
    if (exclusions & ClientModel::SpecialWindowsExclusion) {
        if (client->isSpecialWindow()) {
            return true;
        }
    }
    if (exclusions & ClientModel::SkipTaskbarExclusion) {
        if (client->skipTaskbar()) {
            return true;
        }
    }
    if (exclusions & ClientModel::SkipPagerExclusion) {
        if (client->skipPager()) {
            return true;
        }
    }
    if (exclusions & ClientModel::SwitchSwitcherExclusion) {
        if (client->skipSwitcher()) {
            return true;
        }
    }
    if (exclusions & ClientModel::OtherDesktopsExclusion) {
        if (!client->isOnCurrentDesktop()) {
            return true;
        }
    }
    if (exclusions & ClientModel::OtherActivitiesExclusion) {
        if (!client->isOnCurrentActivity()) {
            return true;
        }
    }
    if (exclusions & ClientModel::MinimizedExclusion) {
        if (client->isMinimized()) {
            return true;
        }
    }
    if (exclusions & ClientModel::NotAcceptingFocusExclusion) {
        if (!client->wantsInput()) {
            return true;
        }
    }
    return false;
}

bool ClientLevel::shouldAdd(Window *client) const
{
    if (restrictions() == ClientModel::NoRestriction) {
        return true;
    }
    if (restrictions() & ClientModel::ActivityRestriction) {
        if (!client->isOnActivity(activity())) {
            return false;
        }
    }
    if (restrictions() & ClientModel::VirtualDesktopRestriction) {
        if (!client->isOnDesktop(virtualDesktop())) {
            return false;
        }
    }
    if (restrictions() & ClientModel::ScreenRestriction) {
        if (client->screen() != int(screen())) {
            return false;
        }
    }
    return true;
}

void ClientLevel::addClient(Window *client)
{
    if (containsClient(client)) {
        return;
    }
    Q_EMIT beginInsert(m_clients.count(), m_clients.count(), id());
    m_clients.insert(nextId(), client);
    Q_EMIT endInsert();
}

void ClientLevel::removeClient(Window *client)
{
    int index = 0;
    auto it = m_clients.begin();
    for (; it != m_clients.end(); ++it, ++index) {
        if (it.value() == client) {
            break;
        }
    }
    if (it == m_clients.end()) {
        return;
    }
    Q_EMIT beginRemove(index, index, id());
    m_clients.erase(it);
    Q_EMIT endRemove();
}

void ClientLevel::init()
{
    const QList<Window *> &clients = workspace()->allClientList();
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        Window *client = *it;
        setupClientConnections(client);
        if (!exclude(client) && shouldAdd(client)) {
            m_clients.insert(nextId(), client);
        }
    }
}

void ClientLevel::reInit()
{
    const QList<Window *> &clients = workspace()->allClientList();
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        checkClient((*it));
    }
}

quint32 ClientLevel::idForRow(int row) const
{
    if (row >= m_clients.size()) {
        return 0;
    }
    auto it = m_clients.constBegin();
    for (int i = 0; i < row; ++i) {
        ++it;
    }
    return it.key();
}

bool ClientLevel::containsId(quint32 id) const
{
    return m_clients.contains(id);
}

int ClientLevel::rowForId(quint32 id) const
{
    int row = 0;
    for (auto it = m_clients.constBegin();
         it != m_clients.constEnd();
         ++it, ++row) {
        if (it.key() == id) {
            return row;
        }
    }
    return -1;
}

Window *ClientLevel::clientForId(quint32 child) const
{
    auto it = m_clients.constFind(child);
    if (it == m_clients.constEnd()) {
        return nullptr;
    }
    return it.value();
}

bool ClientLevel::containsClient(Window *client) const
{
    for (auto it = m_clients.constBegin();
         it != m_clients.constEnd();
         ++it) {
        if (it.value() == client) {
            return true;
        }
    }
    return false;
}

const AbstractLevel *ClientLevel::levelForId(quint32 id) const
{
    if (id == AbstractLevel::id()) {
        return this;
    }
    return nullptr;
}

AbstractLevel *ClientLevel::parentForId(quint32 child) const
{
    if (child == id()) {
        return parentLevel();
    }
    if (m_clients.contains(child)) {
        return const_cast<ClientLevel *>(this);
    }
    return nullptr;
}

AbstractLevel *AbstractLevel::create(const QList<ClientModel::LevelRestriction> &restrictions, ClientModel::LevelRestrictions parentRestrictions, ClientModel *model, AbstractLevel *parent)
{
    if (restrictions.isEmpty() || restrictions.first() == ClientModel::NoRestriction) {
        ClientLevel *leaf = new ClientLevel(model, parent);
        leaf->setRestrictions(parentRestrictions);
        if (!parent) {
            leaf->setParent(model);
        }
        return leaf;
    }
    // create a level
    QList<ClientModel::LevelRestriction> childRestrictions(restrictions);
    ClientModel::LevelRestriction restriction = childRestrictions.takeFirst();
    ClientModel::LevelRestrictions childrenRestrictions = restriction | parentRestrictions;
    ForkLevel *currentLevel = new ForkLevel(childRestrictions, model, parent);
    currentLevel->setRestrictions(childrenRestrictions);
    currentLevel->setRestriction(restriction);
    if (!parent) {
        currentLevel->setParent(model);
    }
    switch (restriction) {
    case ClientModel::ActivityRestriction: {
#if KWIN_BUILD_ACTIVITIES
        if (Workspace::self()->activities()) {
            const QStringList &activities = Workspace::self()->activities()->all();
            for (QStringList::const_iterator it = activities.begin(); it != activities.end(); ++it) {
                AbstractLevel *childLevel = create(childRestrictions, childrenRestrictions, model, currentLevel);
                if (!childLevel) {
                    continue;
                }
                childLevel->setActivity(*it);
                currentLevel->addChild(childLevel);
            }
        }
        break;
#else
        return nullptr;
#endif
    }
    case ClientModel::ScreenRestriction:
        for (int i = 0; i < workspace()->outputs().count(); ++i) {
            AbstractLevel *childLevel = create(childRestrictions, childrenRestrictions, model, currentLevel);
            if (!childLevel) {
                continue;
            }
            childLevel->setScreen(i);
            currentLevel->addChild(childLevel);
        }
        break;
    case ClientModel::VirtualDesktopRestriction:
        for (uint i = 1; i <= VirtualDesktopManager::self()->count(); ++i) {
            AbstractLevel *childLevel = create(childRestrictions, childrenRestrictions, model, currentLevel);
            if (!childLevel) {
                continue;
            }
            childLevel->setVirtualDesktop(i);
            currentLevel->addChild(childLevel);
        }
        break;
    default:
        // invalid
        return nullptr;
    }

    return currentLevel;
}

AbstractLevel::AbstractLevel(ClientModel *model, AbstractLevel *parent)
    : QObject(parent)
    , m_model(model)
    , m_parent(parent)
    , m_screen(0)
    , m_virtualDesktop(0)
    , m_activity()
    , m_restriction(ClientModel::ClientModel::NoRestriction)
    , m_restrictions(ClientModel::NoRestriction)
    , m_id(nextId())
{
}

AbstractLevel::~AbstractLevel()
{
}

void AbstractLevel::setRestriction(ClientModel::LevelRestriction restriction)
{
    m_restriction = restriction;
}

void AbstractLevel::setActivity(const QString &activity)
{
    m_activity = activity;
}

void AbstractLevel::setScreen(uint screen)
{
    m_screen = screen;
}

void AbstractLevel::setVirtualDesktop(uint virtualDesktop)
{
    m_virtualDesktop = virtualDesktop;
}

void AbstractLevel::setRestrictions(ClientModel::LevelRestrictions restrictions)
{
    m_restrictions = restrictions;
}

ForkLevel::ForkLevel(const QList<ClientModel::LevelRestriction> &childRestrictions, ClientModel *model, AbstractLevel *parent)
    : AbstractLevel(model, parent)
    , m_childRestrictions(childRestrictions)
{
    connect(VirtualDesktopManager::self(), &VirtualDesktopManager::countChanged, this, &ForkLevel::desktopCountChanged);
    connect(workspace(), &Workspace::outputAdded, this, &ForkLevel::outputAdded);
    connect(workspace(), &Workspace::outputRemoved, this, &ForkLevel::outputRemoved);
#if KWIN_BUILD_ACTIVITIES
    if (Activities *activities = Workspace::self()->activities()) {
        connect(activities, &Activities::added, this, &ForkLevel::activityAdded);
        connect(activities, &Activities::removed, this, &ForkLevel::activityRemoved);
    }
#endif
}

ForkLevel::~ForkLevel()
{
}

void ForkLevel::desktopCountChanged(uint previousCount, uint newCount)
{
    if (restriction() != ClientModel::ClientModel::VirtualDesktopRestriction) {
        return;
    }
    if (previousCount != uint(count())) {
        return;
    }
    if (previousCount > newCount) {
        // desktops got removed
        Q_EMIT beginRemove(newCount, previousCount - 1, id());
        while (uint(m_children.count()) > newCount) {
            delete m_children.takeLast();
        }
        Q_EMIT endRemove();
    } else {
        // desktops got added
        Q_EMIT beginInsert(previousCount, newCount - 1, id());
        for (uint i = previousCount + 1; i <= newCount; ++i) {
            AbstractLevel *childLevel = AbstractLevel::create(m_childRestrictions, restrictions(), model(), this);
            if (!childLevel) {
                continue;
            }
            childLevel->setVirtualDesktop(i);
            childLevel->init();
            addChild(childLevel);
        }
        Q_EMIT endInsert();
    }
}

void ForkLevel::outputAdded()
{
    if (restriction() != ClientModel::ClientModel::ClientModel::ScreenRestriction) {
        return;
    }

    const int row = count();

    AbstractLevel *childLevel = AbstractLevel::create(m_childRestrictions, restrictions(), model(), this);
    if (!childLevel) {
        return;
    }
    Q_EMIT beginInsert(row, row, id());
    childLevel->setScreen(row);
    childLevel->init();
    addChild(childLevel);
    Q_EMIT endInsert();
}

void ForkLevel::outputRemoved()
{
    if (restriction() != ClientModel::ClientModel::ClientModel::ScreenRestriction) {
        return;
    }

    const int row = count() - 1;

    Q_EMIT beginRemove(row, row, id());
    delete m_children.takeLast();
    Q_EMIT endRemove();
}

void ForkLevel::activityAdded(const QString &activityId)
{
#if KWIN_BUILD_ACTIVITIES
    if (restriction() != ClientModel::ClientModel::ActivityRestriction) {
        return;
    }
    // verify that our children do not contain this activity
    for (AbstractLevel *child : std::as_const(m_children)) {
        if (child->activity() == activityId) {
            return;
        }
    }
    Q_EMIT beginInsert(m_children.count(), m_children.count(), id());
    AbstractLevel *childLevel = AbstractLevel::create(m_childRestrictions, restrictions(), model(), this);
    if (!childLevel) {
        Q_EMIT endInsert();
        return;
    }
    childLevel->setActivity(activityId);
    childLevel->init();
    addChild(childLevel);
    Q_EMIT endInsert();
#endif
}

void ForkLevel::activityRemoved(const QString &activityId)
{
#if KWIN_BUILD_ACTIVITIES
    if (restriction() != ClientModel::ClientModel::ActivityRestriction) {
        return;
    }
    for (int i = 0; i < m_children.length(); ++i) {
        if (m_children.at(i)->activity() == activityId) {
            Q_EMIT beginRemove(i, i, id());
            delete m_children.takeAt(i);
            Q_EMIT endRemove();
            break;
        }
    }
#endif
}

int ForkLevel::count() const
{
    return m_children.count();
}

void ForkLevel::addChild(AbstractLevel *child)
{
    m_children.append(child);
    connect(child, &AbstractLevel::beginInsert, this, &AbstractLevel::beginInsert);
    connect(child, &AbstractLevel::beginRemove, this, &AbstractLevel::beginRemove);
    connect(child, &AbstractLevel::endInsert, this, &AbstractLevel::endInsert);
    connect(child, &AbstractLevel::endRemove, this, &AbstractLevel::endRemove);
}

void ForkLevel::setActivity(const QString &activity)
{
    AbstractLevel::setActivity(activity);
    for (QList<AbstractLevel *>::iterator it = m_children.begin(); it != m_children.end(); ++it) {
        (*it)->setActivity(activity);
    }
}

void ForkLevel::setScreen(uint screen)
{
    AbstractLevel::setScreen(screen);
    for (QList<AbstractLevel *>::iterator it = m_children.begin(); it != m_children.end(); ++it) {
        (*it)->setScreen(screen);
    }
}

void ForkLevel::setVirtualDesktop(uint virtualDesktop)
{
    AbstractLevel::setVirtualDesktop(virtualDesktop);
    for (QList<AbstractLevel *>::iterator it = m_children.begin(); it != m_children.end(); ++it) {
        (*it)->setVirtualDesktop(virtualDesktop);
    }
}

void ForkLevel::init()
{
    for (QList<AbstractLevel *>::iterator it = m_children.begin(); it != m_children.end(); ++it) {
        (*it)->init();
    }
}

quint32 ForkLevel::idForRow(int row) const
{
    if (row >= m_children.length()) {
        return 0;
    }
    return m_children.at(row)->id();
}

const AbstractLevel *ForkLevel::levelForId(quint32 id) const
{
    if (id == AbstractLevel::id()) {
        return this;
    }
    for (QList<AbstractLevel *>::const_iterator it = m_children.constBegin(); it != m_children.constEnd(); ++it) {
        if (const AbstractLevel *child = (*it)->levelForId(id)) {
            return child;
        }
    }
    // not found
    return nullptr;
}

AbstractLevel *ForkLevel::parentForId(quint32 child) const
{
    if (child == id()) {
        return parentLevel();
    }
    for (QList<AbstractLevel *>::const_iterator it = m_children.constBegin(); it != m_children.constEnd(); ++it) {
        if (AbstractLevel *parent = (*it)->parentForId(child)) {
            return parent;
        }
    }
    // not found
    return nullptr;
}

int ForkLevel::rowForId(quint32 child) const
{
    if (id() == child) {
        return 0;
    }
    for (int i = 0; i < m_children.count(); ++i) {
        if (m_children.at(i)->id() == child) {
            return i;
        }
    }
    // do recursion
    for (QList<AbstractLevel *>::const_iterator it = m_children.constBegin(); it != m_children.constEnd(); ++it) {
        int row = (*it)->rowForId(child);
        if (row != -1) {
            return row;
        }
    }
    // not found
    return -1;
}

Window *ForkLevel::clientForId(quint32 child) const
{
    for (QList<AbstractLevel *>::const_iterator it = m_children.constBegin(); it != m_children.constEnd(); ++it) {
        if (Window *client = (*it)->clientForId(child)) {
            return client;
        }
    }
    // not found
    return nullptr;
}

ClientModel::ClientModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_root(nullptr)
    , m_exclusions(NoExclusion)
{
}

ClientModel::~ClientModel()
{
}

void ClientModel::setLevels(QList<ClientModel::LevelRestriction> restrictions)
{
    beginResetModel();
    if (m_root) {
        delete m_root;
    }
    m_root = AbstractLevel::create(restrictions, NoRestriction, this);
    connect(m_root, &AbstractLevel::beginInsert, this, &ClientModel::levelBeginInsert);
    connect(m_root, &AbstractLevel::beginRemove, this, &ClientModel::levelBeginRemove);
    connect(m_root, &AbstractLevel::endInsert, this, &ClientModel::levelEndInsert);
    connect(m_root, &AbstractLevel::endRemove, this, &ClientModel::levelEndRemove);
    m_root->init();
    endResetModel();
}

void ClientModel::setExclusions(ClientModel::Exclusions exclusions)
{
    if (exclusions == m_exclusions) {
        return;
    }
    m_exclusions = exclusions;
    Q_EMIT exclusionsChanged();
}

QVariant ClientModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0) {
        return QVariant();
    }
    if (const AbstractLevel *level = getLevel(index)) {
        LevelRestriction restriction = level->restriction();
        if (restriction == ActivityRestriction && (role == Qt::DisplayRole || role == ActivityRole)) {
            return level->activity();
        } else if (restriction == VirtualDesktopRestriction && (role == Qt::DisplayRole || role == DesktopRole)) {
            return level->virtualDesktop();
        } else if (restriction == ScreenRestriction && (role == Qt::DisplayRole || role == ScreenRole)) {
            return level->screen();
        } else {
            return QVariant();
        }
    }
    if (role == Qt::DisplayRole || role == ClientRole) {
        if (Window *client = m_root->clientForId(index.internalId())) {
            return QVariant::fromValue(client);
        }
    }
    return QVariant();
}

int ClientModel::columnCount(const QModelIndex &parent) const
{
    return 1;
}

int ClientModel::rowCount(const QModelIndex &parent) const
{
    if (!m_root) {
        return 0;
    }
    if (!parent.isValid()) {
        return m_root->count();
    }
    if (const AbstractLevel *level = getLevel(parent)) {
        if (level->id() != parent.internalId()) {
            // not a real level - no children
            return 0;
        }
        return level->count();
    }
    return 0;
}

QHash<int, QByteArray> ClientModel::roleNames() const
{
    return {
        {Qt::DisplayRole, QByteArrayLiteral("display")},
        {ClientRole, QByteArrayLiteral("client")},
        {ScreenRole, QByteArrayLiteral("screen")},
        {DesktopRole, QByteArrayLiteral("desktop")},
        {ActivityRole, QByteArrayLiteral("activity")},
    };
}

QModelIndex ClientModel::parent(const QModelIndex &child) const
{
    if (!child.isValid() || child.column() != 0) {
        return QModelIndex();
    }
    return parentForId(child.internalId());
}

QModelIndex ClientModel::parentForId(quint32 childId) const
{
    if (childId == m_root->id()) {
        // asking for parent of our toplevel
        return QModelIndex();
    }
    if (AbstractLevel *parentLevel = m_root->parentForId(childId)) {
        if (parentLevel == m_root) {
            return QModelIndex();
        }
        const int row = m_root->rowForId(parentLevel->id());
        if (row == -1) {
            // error
            return QModelIndex();
        }
        return createIndex(row, 0, parentLevel->id());
    }
    return QModelIndex();
}

QModelIndex ClientModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0 || row < 0 || !m_root) {
        return QModelIndex();
    }
    if (!parent.isValid()) {
        if (row >= rowCount()) {
            return QModelIndex();
        }
        return createIndex(row, 0, m_root->idForRow(row));
    }
    const AbstractLevel *parentLevel = getLevel(parent);
    if (!parentLevel) {
        return QModelIndex();
    }
    if (row >= parentLevel->count()) {
        return QModelIndex();
    }
    const quint32 id = parentLevel->idForRow(row);
    if (id == 0) {
        return QModelIndex();
    }
    return createIndex(row, column, id);
}

const AbstractLevel *ClientModel::getLevel(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return m_root;
    }
    return m_root->levelForId(index.internalId());
}

void ClientModel::levelBeginInsert(int rowStart, int rowEnd, quint32 id)
{
    const int row = m_root->rowForId(id);
    QModelIndex parent;
    if (row != -1) {
        parent = createIndex(row, 0, id);
    }
    beginInsertRows(parent, rowStart, rowEnd);
}

void ClientModel::levelBeginRemove(int rowStart, int rowEnd, quint32 id)
{
    const int row = m_root->rowForId(id);
    QModelIndex parent;
    if (row != -1) {
        parent = createIndex(row, 0, id);
    }
    beginRemoveRows(parent, rowStart, rowEnd);
}

void ClientModel::levelEndInsert()
{
    endInsertRows();
}

void ClientModel::levelEndRemove()
{
    endRemoveRows();
}

#define CLIENT_MODEL_WRAPPER(name, levels) \
    name::name(QObject *parent)            \
        : ClientModel(parent)              \
    {                                      \
        setLevels(levels);                 \
    }                                      \
    name::~name()                          \
    {                                      \
    }

CLIENT_MODEL_WRAPPER(SimpleClientModel, QList<LevelRestriction>())
CLIENT_MODEL_WRAPPER(ClientModelByScreen, QList<LevelRestriction>() << ScreenRestriction)
CLIENT_MODEL_WRAPPER(ClientModelByScreenAndDesktop, QList<LevelRestriction>() << ScreenRestriction << VirtualDesktopRestriction)
CLIENT_MODEL_WRAPPER(ClientModelByScreenAndActivity, QList<LevelRestriction>() << ScreenRestriction << ActivityRestriction)
#undef CLIENT_MODEL_WRAPPER

ClientFilterModel::ClientFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_clientModel(nullptr)
{
}

ClientFilterModel::~ClientFilterModel()
{
}

void ClientFilterModel::setClientModel(ClientModel *clientModel)
{
    if (clientModel == m_clientModel) {
        return;
    }
    m_clientModel = clientModel;
    setSourceModel(m_clientModel);
    Q_EMIT clientModelChanged();
}

void ClientFilterModel::setFilter(const QString &filter)
{
    if (filter == m_filter) {
        return;
    }
    m_filter = filter;
    Q_EMIT filterChanged();
    invalidateFilter();
}

bool ClientFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (!m_clientModel) {
        return false;
    }
    if (m_filter.isEmpty()) {
        return true;
    }
    QModelIndex index = m_clientModel->index(sourceRow, 0, sourceParent);
    if (!index.isValid()) {
        return false;
    }
    QVariant data = index.data();
    if (!data.isValid()) {
        // an invalid QVariant is valid data
        return true;
    }
    // TODO: introduce a type as a data role and properly check, this seems dangerous
    if (data.type() == QVariant::Int || data.type() == QVariant::UInt || data.type() == QVariant::String) {
        // we do not filter out screen, desktop and activity
        return true;
    }
    Window *client = qvariant_cast<KWin::Window *>(data);
    if (!client) {
        return false;
    }
    if (client->caption().contains(m_filter, Qt::CaseInsensitive)) {
        return true;
    }
    if (client->windowRole().contains(m_filter, Qt::CaseInsensitive)) {
        return true;
    }
    if (client->resourceName().contains(m_filter, Qt::CaseInsensitive)) {
        return true;
    }
    if (client->resourceClass().contains(m_filter, Qt::CaseInsensitive)) {
        return true;
    }
    return false;
}

} // namespace KWin
