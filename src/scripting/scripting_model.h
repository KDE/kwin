/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCRIPTING_MODEL_H
#define KWIN_SCRIPTING_MODEL_H

#include <QAbstractItemModel>
#include <QSortFilterProxyModel>
#include <QList>

namespace KWin {
class AbstractClient;
class Client;

namespace ScriptingClientModel {

class AbstractLevel;

class ClientModel : public QAbstractItemModel
{
    Q_OBJECT
    Q_ENUMS(Exclude)
    Q_ENUMS(LevelRestriction)
    Q_PROPERTY(Exclusions exclusions READ exclusions WRITE setExclusions NOTIFY exclusionsChanged)
public:
    enum Exclusion {
        NoExclusion = 0,
        // window types
        DesktopWindowsExclusion       = 1 << 0,
        DockWindowsExclusion          = 1 << 1,
        UtilityWindowsExclusion       = 1 << 2,
        SpecialWindowsExclusion       = 1 << 3,
        // windows with flags
        SkipTaskbarExclusion          = 1 << 4,
        SkipPagerExclusion            = 1 << 5,
        SwitchSwitcherExclusion       = 1 << 6,
        // based on state
        OtherDesktopsExclusion        = 1 << 7,
        OtherActivitiesExclusion      = 1 << 8,
        MinimizedExclusion            = 1 << 9,
        NonSelectedWindowTabExclusion = 1 << 10,
        NotAcceptingFocusExclusion    = 1 << 11
    };
    Q_DECLARE_FLAGS(Exclusions, Exclusion)
    Q_FLAGS(Exclusions)
    enum LevelRestriction {
        NoRestriction = 0,
        VirtualDesktopRestriction = 1 << 0,
        ScreenRestriction = 1 << 1,
        ActivityRestriction = 1 << 2
    };
    Q_DECLARE_FLAGS(LevelRestrictions, LevelRestriction)
    Q_FLAGS(LevelRestrictions)
    explicit ClientModel(QObject *parent);
    ~ClientModel() override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setExclusions(ClientModel::Exclusions exclusions);
    Exclusions exclusions() const;

Q_SIGNALS:
    void exclusionsChanged();

private Q_SLOTS:
    void levelBeginInsert(int rowStart, int rowEnd, quint32 parentId);
    void levelEndInsert();
    void levelBeginRemove(int rowStart, int rowEnd, quint32 parentId);
    void levelEndRemove();

protected:
    enum ClientModelRoles {
        ClientRole = Qt::UserRole,
        ScreenRole,
        DesktopRole,
        ActivityRole
    };
    void setLevels(QList<LevelRestriction> restrictions);

private:
    QModelIndex parentForId(quint32 childId) const;
    const AbstractLevel *getLevel(const QModelIndex &index) const;
    AbstractLevel *m_root;
    Exclusions m_exclusions;
};

/**
 * @brief The data structure of the Model.
 *
 * The model is implemented as a Tree consisting of AbstractLevels as the levels of the tree.
 * A non leaf level is represented by the inheriting class ForkLevel, the last level above a
 * leaf is represented by the inheriting class ClientLevel, which contains the Clients - each
 * Client is one leaf.
 *
 * In case the tree would only consist of Clients - leafs - it has always one ClientLevel as the root
 * of the tree.
 *
 * The number of levels in the tree is controlled by the LevelRestrictions. For each existing
 * LevelRestriction a new Level is created, if there are no more restrictions a ClientLevel is created.
 *
 * To build up the tree the static factory method @ref create has to be used. It will recursively
 * build up the tree. After the tree has been build up use @ref init to initialize the tree which
 * will add the Clients to the ClientLevel.
 *
 * Each element of the tree has a unique id which can be used by the QAbstractItemModel as the
 * internal id for its QModelIndex. Note: the ids have no ordering, if trying to get a specific element
 * the tree performs a depth-first search.
 */
class AbstractLevel : public QObject
{
    Q_OBJECT
public:
    ~AbstractLevel() override;
    virtual int count() const = 0;
    virtual void init() = 0;
    virtual quint32 idForRow(int row) const = 0;

    uint screen() const;
    uint virtualDesktop() const;
    const QString &activity() const;
    ClientModel::LevelRestrictions restrictions() const;
    void setRestrictions(ClientModel::LevelRestrictions restrictions);
    ClientModel::LevelRestriction restriction() const;
    void setRestriction(ClientModel::LevelRestriction restriction);
    quint32 id() const;
    AbstractLevel *parentLevel() const;
    virtual const AbstractLevel *levelForId(quint32 id) const = 0;
    virtual AbstractLevel *parentForId(quint32 child) const = 0;
    virtual int rowForId(quint32 child) const = 0;
    virtual AbstractClient *clientForId(quint32 child) const = 0;

    virtual void setScreen(uint screen);
    virtual void setVirtualDesktop(uint virtualDesktop);
    virtual void setActivity(const QString &activity);

    static AbstractLevel *create(const QList<ClientModel::LevelRestriction> &restrictions, ClientModel::LevelRestrictions parentRestrictions, ClientModel *model, AbstractLevel *parent = nullptr);

Q_SIGNALS:
    void beginInsert(int rowStart, int rowEnd, quint32 parentId);
    void endInsert();
    void beginRemove(int rowStart, int rowEnd, quint32 parentId);
    void endRemove();
protected:
    AbstractLevel(ClientModel *model, AbstractLevel *parent);
    ClientModel *model() const;
private:
    ClientModel *m_model;
    AbstractLevel *m_parent;
    uint m_screen;
    uint m_virtualDesktop;
    QString m_activity;
    ClientModel::LevelRestriction m_restriction;
    ClientModel::LevelRestrictions m_restrictions;
    quint32 m_id;
};

class ForkLevel : public AbstractLevel
{
    Q_OBJECT
public:
    ForkLevel(const QList<ClientModel::LevelRestriction> &childRestrictions, ClientModel *model, AbstractLevel *parent);
    ~ForkLevel() override;
    int count() const override;
    void init() override;
    quint32 idForRow(int row) const override;
    void addChild(AbstractLevel *child);
    void setScreen(uint screen) override;
    void setVirtualDesktop(uint virtualDesktop) override;
    void setActivity(const QString &activity) override;
    const AbstractLevel *levelForId(quint32 id) const override;
    AbstractLevel *parentForId(quint32 child) const override;
    int rowForId(quint32 child) const override;
    AbstractClient *clientForId(quint32 child) const override;
private Q_SLOTS:
    void desktopCountChanged(uint previousCount, uint newCount);
    void screenCountChanged(int previousCount, int newCount);
    void activityAdded(const QString &id);
    void activityRemoved(const QString &id);
private:
    QList<AbstractLevel*> m_children;
    QList<ClientModel::LevelRestriction> m_childRestrictions;
};

/**
 * @brief The actual leafs of the model's tree containing the Client's in this branch of the tree.
 *
 * This class groups all the Clients of one branch of the tree and takes care of updating the tree
 * when a Client changes its state in a way that it should be excluded/included or gets added or
 * removed.
 *
 * The Clients in this group are not sorted in any particular way. It's a simple list which only
 * gets added to. If some sorting should be applied, use a QSortFilterProxyModel.
 */
class ClientLevel : public AbstractLevel
{
    Q_OBJECT
public:
    explicit ClientLevel(ClientModel *model, AbstractLevel *parent);
    ~ClientLevel() override;

    void init() override;

    int count() const override;
    quint32 idForRow(int row) const override;
    bool containsId(quint32 id) const;
    int rowForId(quint32 row) const override;
    AbstractClient *clientForId(quint32 child) const override;
    const AbstractLevel *levelForId(quint32 id) const override;
    AbstractLevel *parentForId(quint32 child) const override;
public Q_SLOTS:
    void clientAdded(KWin::AbstractClient *client);
    void clientRemoved(KWin::AbstractClient *client);
private Q_SLOTS:
    // uses sender()
    void reInit();
private:
    void checkClient(KWin::AbstractClient *client);
    void setupClientConnections(AbstractClient *client);
    void addClient(AbstractClient *client);
    void removeClient(AbstractClient *client);
    bool shouldAdd(AbstractClient *client) const;
    bool exclude(AbstractClient *client) const;
    bool containsClient(AbstractClient *client) const;
    QMap<quint32, AbstractClient*> m_clients;
};

class SimpleClientModel : public ClientModel
{
    Q_OBJECT
public:
    SimpleClientModel(QObject *parent = nullptr);
    ~SimpleClientModel() override;
};

class ClientModelByScreen : public ClientModel
{
    Q_OBJECT
public:
    ClientModelByScreen(QObject *parent = nullptr);
    ~ClientModelByScreen() override;
};

class ClientModelByScreenAndDesktop : public ClientModel
{
    Q_OBJECT
public:
    ClientModelByScreenAndDesktop(QObject *parent = nullptr);
    ~ClientModelByScreenAndDesktop() override;
};

class ClientModelByScreenAndActivity : public ClientModel
{
    Q_OBJECT
public:
    ClientModelByScreenAndActivity(QObject *parent = nullptr);
    ~ClientModelByScreenAndActivity() override;
};

/**
 * @brief Custom QSortFilterProxyModel to filter on Client caption, role and class.
 */
class ClientFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(KWin::ScriptingClientModel::ClientModel *clientModel READ clientModel WRITE setClientModel NOTIFY clientModelChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
public:
    ClientFilterModel(QObject *parent = nullptr);
    ~ClientFilterModel() override;
    ClientModel *clientModel() const;
    const QString &filter() const;

public Q_SLOTS:
    void setClientModel(ClientModel *clientModel);
    void setFilter(const QString &filter);

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

Q_SIGNALS:
    void clientModelChanged();
    void filterChanged();

private:
    ClientModel *m_clientModel;
    QString m_filter;
};

inline
int ClientLevel::count() const
{
    return m_clients.count();
}

inline
const QString &AbstractLevel::activity() const
{
    return m_activity;
}

inline
AbstractLevel *AbstractLevel::parentLevel() const
{
    return m_parent;
}

inline
ClientModel *AbstractLevel::model() const
{
    return m_model;
}

inline
uint AbstractLevel::screen() const
{
    return m_screen;
}

inline
uint AbstractLevel::virtualDesktop() const
{
    return m_virtualDesktop;
}

inline
ClientModel::LevelRestriction AbstractLevel::restriction() const
{
    return m_restriction;
}

inline
ClientModel::LevelRestrictions AbstractLevel::restrictions() const
{
    return m_restrictions;
}

inline
quint32 AbstractLevel::id() const
{
    return m_id;
}

inline
ClientModel::Exclusions ClientModel::exclusions() const
{
    return m_exclusions;
}

inline
ClientModel *ClientFilterModel::clientModel() const
{
    return m_clientModel;
}

inline
const QString &ClientFilterModel::filter() const
{
    return m_filter;
}

} // namespace Scripting
} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::ScriptingClientModel::ClientModel::Exclusions)
Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::ScriptingClientModel::ClientModel::LevelRestrictions)

#endif // KWIN_SCRIPTING_MODEL_H
