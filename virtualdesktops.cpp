/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtualdesktops.h"
#include "input.h"
// KDE
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <NETWM>

#include <KWaylandServer/plasmavirtualdesktop_interface.h>
// Qt
#include <QAction>
#include <QUuid>

#include <algorithm>
#include <QDebug>
namespace KWin {

static bool s_loadingDesktopSettings = false;

static QByteArray generateDesktopId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8();
}

VirtualDesktop::VirtualDesktop(QObject *parent)
    : QObject(parent)
{
}

VirtualDesktop::~VirtualDesktop()
{
    emit aboutToBeDestroyed();
}

void VirtualDesktopManager::setVirtualDesktopManagement(KWaylandServer::PlasmaVirtualDesktopManagementInterface *management)
{
    using namespace KWaylandServer;
    Q_ASSERT(!m_virtualDesktopManagement);
    m_virtualDesktopManagement = management;

    auto createPlasmaVirtualDesktop = [this](VirtualDesktop *desktop) {
        PlasmaVirtualDesktopInterface *pvd = m_virtualDesktopManagement->createDesktop(desktop->id(), desktop->x11DesktopNumber() - 1);
        pvd->setName(desktop->name());
        pvd->sendDone();

        connect(desktop, &VirtualDesktop::nameChanged, pvd,
            [desktop, pvd] {
                pvd->setName(desktop->name());
                pvd->sendDone();
            }
        );
        connect(pvd, &PlasmaVirtualDesktopInterface::activateRequested, this,
            [this, desktop] {
                setCurrent(desktop);
            }
        );
    };

    connect(this, &VirtualDesktopManager::desktopCreated, m_virtualDesktopManagement, createPlasmaVirtualDesktop);

    connect(this, &VirtualDesktopManager::rowsChanged, m_virtualDesktopManagement,
        [this](uint rows) {
            m_virtualDesktopManagement->setRows(rows);
            m_virtualDesktopManagement->sendDone();
        }
    );

    //handle removed: from VirtualDesktopManager to the wayland interface
    connect(this, &VirtualDesktopManager::desktopRemoved, m_virtualDesktopManagement,
        [this](VirtualDesktop *desktop) {
            m_virtualDesktopManagement->removeDesktop(desktop->id());
        }
    );

    //create a new desktop when the client asks to
    connect (m_virtualDesktopManagement, &PlasmaVirtualDesktopManagementInterface::desktopCreateRequested, this,
        [this](const QString &name, quint32 position) {
            createVirtualDesktop(position, name);
        }
    );

    //remove when the client asks to
    connect (m_virtualDesktopManagement, &PlasmaVirtualDesktopManagementInterface::desktopRemoveRequested, this,
        [this](const QString &id) {
            //here there can be some nice kauthorized check?
            //remove only from VirtualDesktopManager, the other connections will remove it from m_virtualDesktopManagement as well
            removeVirtualDesktop(id.toUtf8());
        }
    );

    std::for_each(m_desktops.constBegin(), m_desktops.constEnd(), createPlasmaVirtualDesktop);

    //Now we are sure all ids are there
    save();

    connect(this, &VirtualDesktopManager::currentChanged, m_virtualDesktopManagement,
        [this]() {
            for (auto *deskInt : m_virtualDesktopManagement->desktops()) {
                if (deskInt->id() == currentDesktop()->id()) {
                    deskInt->setActive(true);
                } else {
                    deskInt->setActive(false);
                }
            }
        }
    );
}

void VirtualDesktop::setId(const QByteArray &id)
{
    Q_ASSERT(m_id.isEmpty());
    m_id = id;
}

void VirtualDesktop::setX11DesktopNumber(uint number)
{
    //x11DesktopNumber can be changed now
    if (static_cast<uint>(m_x11DesktopNumber) == number) {
        return;
    }

    m_x11DesktopNumber = number;

    if (m_x11DesktopNumber != 0) {
        emit x11DesktopNumberChanged();
    }
}

void VirtualDesktop::setName(const QString &name)
{
    if (m_name == name) {
        return;
    }
    m_name = name;
    emit nameChanged();
}

VirtualDesktopGrid::VirtualDesktopGrid()
    : m_size(1, 2) // Default to tow rows
    , m_grid(QVector<QVector<VirtualDesktop*>>{QVector<VirtualDesktop*>{}, QVector<VirtualDesktop*>{}})
{    
}

VirtualDesktopGrid::~VirtualDesktopGrid() = default;

void VirtualDesktopGrid::update(const QSize &size, Qt::Orientation orientation, const QVector<VirtualDesktop*> &desktops)
{
    // Set private variables
    m_size = size;
    const uint width = size.width();
    const uint height = size.height();

    m_grid.clear();
    auto it = desktops.begin();
    auto end = desktops.end();
    if (orientation == Qt::Horizontal) {
        for (uint y = 0; y < height; ++y) {
            QVector<VirtualDesktop*> row;
            for (uint x = 0; x < width && it != end; ++x) {
                row << *it;
                it++;
            }
            m_grid << row;
        }
    } else {
        for (uint y = 0; y < height; ++y) {
            m_grid << QVector<VirtualDesktop*>();
        }
        for (uint x = 0; x < width; ++x) {
            for (uint y = 0; y < height && it != end; ++y) {
                auto &row = m_grid[y];
                row << *it;
                it++;
            }
        }
    }
}

QPoint VirtualDesktopGrid::gridCoords(uint id) const
{
    return gridCoords(VirtualDesktopManager::self()->desktopForX11Id(id));
}

QPoint VirtualDesktopGrid::gridCoords(VirtualDesktop *vd) const
{
    for (int y = 0; y < m_grid.count(); ++y) {
        const auto &row = m_grid.at(y);
        for (int x = 0; x < row.count(); ++x) {
            if (row.at(x) == vd) {
                return QPoint(x, y);
            }
        }
    }
    return QPoint(-1, -1);
}

VirtualDesktop *VirtualDesktopGrid::at(const QPoint &coords) const
{
    if (coords.y() >= m_grid.count()) {
        return nullptr;
    }
    const auto &row = m_grid.at(coords.y());
    if (coords.x() >= row.count()) {
        return nullptr;
    }
    return row.at(coords.x());
}

KWIN_SINGLETON_FACTORY_VARIABLE(VirtualDesktopManager, s_manager)

VirtualDesktopManager::VirtualDesktopManager(QObject *parent)
    : QObject(parent)
    , m_navigationWrapsAround(false)
    , m_rootInfo(nullptr)
{
}

VirtualDesktopManager::~VirtualDesktopManager()
{
    s_manager = nullptr;
}

void VirtualDesktopManager::setRootInfo(NETRootInfo *info)
{
    m_rootInfo = info;

    // Nothing will be connected to rootInfo
    if (m_rootInfo) {
        int columns = count() / m_rows;
        if (count() % m_rows > 0) {
            columns++;
        }
        m_rootInfo->setDesktopLayout(NET::OrientationHorizontal, columns, m_rows, NET::DesktopLayoutCornerTopLeft);
        updateRootInfo();
        m_rootInfo->setCurrentDesktop(currentDesktop()->x11DesktopNumber());
        for (auto *vd : m_desktops) {
            m_rootInfo->setDesktopName(vd->x11DesktopNumber(), vd->name().toUtf8().data());
        }
    }
}

QString VirtualDesktopManager::name(uint desktop) const
{
    if (uint(m_desktops.length()) > desktop - 1) {
        return m_desktops[desktop - 1]->name();
    }

    if (!m_rootInfo) {
        return defaultName(desktop);
    }
    return QString::fromUtf8(m_rootInfo->desktopName(desktop));
}

uint VirtualDesktopManager::above(uint id, bool wrap) const
{
    auto vd = above(desktopForX11Id(id), wrap);
    return vd ? vd->x11DesktopNumber() : 0;
}

VirtualDesktop *VirtualDesktopManager::above(VirtualDesktop *desktop, bool wrap) const
{
    Q_ASSERT(m_current);
    if (!desktop) {
        desktop = m_current;
    }
    QPoint coords = m_grid.gridCoords(desktop);
    Q_ASSERT(coords.x() >= 0);
    while (true) {
        coords.ry()--;
        if (coords.y() < 0) {
            if (wrap) {
                coords.setY(m_grid.height() - 1);
            } else {
                return desktop; // Already at the top-most desktop
            }
        }
        if (VirtualDesktop *vd = m_grid.at(coords)) {
            return vd;
        }
    }
    return nullptr;
}

uint VirtualDesktopManager::toRight(uint id, bool wrap) const
{
    auto vd = toRight(desktopForX11Id(id), wrap);
    return vd ? vd->x11DesktopNumber() : 0;
}

VirtualDesktop *VirtualDesktopManager::toRight(VirtualDesktop *desktop, bool wrap) const
{
    Q_ASSERT(m_current);
    if (!desktop) {
        desktop = m_current;
    }
    QPoint coords = m_grid.gridCoords(desktop);
    Q_ASSERT(coords.x() >= 0);
    while (true) {
        coords.rx()++;
        if (coords.x() >= m_grid.width()) {
            if (wrap) {
                coords.setX(0);
            } else {
                return desktop; // Already at the right-most desktop
            }
        }
        if (VirtualDesktop *vd = m_grid.at(coords)) {
            return vd;
        }
    }
    return nullptr;
}

uint VirtualDesktopManager::below(uint id, bool wrap) const
{
    auto vd = below(desktopForX11Id(id), wrap);
    return vd ? vd->x11DesktopNumber() : 0;
}

VirtualDesktop *VirtualDesktopManager::below(VirtualDesktop *desktop, bool wrap) const
{
    Q_ASSERT(m_current);
    if (!desktop) {
        desktop = m_current;
    }
    QPoint coords = m_grid.gridCoords(desktop);
    Q_ASSERT(coords.x() >= 0);
    while (true) {
        coords.ry()++;
        if (coords.y() >= m_grid.height()) {
            if (wrap) {
                coords.setY(0);
            } else {
                // Already at the bottom-most desktop
                return desktop;
            }
        }
        if (VirtualDesktop *vd = m_grid.at(coords)) {
            return vd;
        }
    }
    return nullptr;
}

uint VirtualDesktopManager::toLeft(uint id, bool wrap) const
{
    auto vd = toLeft(desktopForX11Id(id), wrap);
    return vd ? vd->x11DesktopNumber() : 0;
}

VirtualDesktop *VirtualDesktopManager::toLeft(VirtualDesktop *desktop, bool wrap) const
{
    Q_ASSERT(m_current);
    if (!desktop) {
        desktop = m_current;
    }
    QPoint coords = m_grid.gridCoords(desktop);
    Q_ASSERT(coords.x() >= 0);
    while (true) {
        coords.rx()--;
        if (coords.x() < 0) {
            if (wrap) {
                coords.setX(m_grid.width() - 1);
            } else {
                return desktop; // Already at the left-most desktop
            }
        }
        if (VirtualDesktop *vd = m_grid.at(coords)) {
            return vd;
        }
    }
    return nullptr;
}

VirtualDesktop *VirtualDesktopManager::next(VirtualDesktop *desktop, bool wrap) const
{
    Q_ASSERT(m_current);
    if (!desktop) {
        desktop = m_current;
    }
    auto it = std::find(m_desktops.begin(), m_desktops.end(), desktop);
    Q_ASSERT(it != m_desktops.end());
    it++;
    if (it == m_desktops.end()) {
        if (wrap) {
            return m_desktops.first();
        } else {
            return desktop;
        }
    }
    return *it;
}

VirtualDesktop *VirtualDesktopManager::previous(VirtualDesktop *desktop, bool wrap) const
{
    Q_ASSERT(m_current);
    if (!desktop) {
        desktop = m_current;
    }
    auto it = std::find(m_desktops.begin(), m_desktops.end(), desktop);
    Q_ASSERT(it != m_desktops.end());
    if (it == m_desktops.begin()) {
        if (wrap) {
            return m_desktops.last();
        } else {
            return desktop;
        }
    }
    it--;
    return *it;
}

VirtualDesktop *VirtualDesktopManager::desktopForX11Id(uint id) const
{
    if (id == 0 || id > count()) {
        return nullptr;
    }
    return m_desktops.at(id - 1);
}

VirtualDesktop *VirtualDesktopManager::desktopForId(const QByteArray &id) const
{
    auto desk = std::find_if(
        m_desktops.constBegin(),
        m_desktops.constEnd(),
        [id] (const VirtualDesktop *desk ) {
            return desk->id() == id;
        }
    );

    if (desk != m_desktops.constEnd()) {
        return *desk;
    }

    return nullptr;
}

VirtualDesktop *VirtualDesktopManager::createVirtualDesktop(uint position, const QString &name)
{
    //too many, can't insert new ones
    if ((uint)m_desktops.count() == VirtualDesktopManager::maximum()) {
        return nullptr;
    }

    position = qBound(0u, position, static_cast<uint>(m_desktops.count()));

    auto *vd = new VirtualDesktop(this);
    vd->setX11DesktopNumber(position + 1);
    vd->setId(generateDesktopId());
    vd->setName(name);

    connect(vd, &VirtualDesktop::nameChanged, this,
        [this, vd]() {
            if (m_rootInfo) {
                m_rootInfo->setDesktopName(vd->x11DesktopNumber(), vd->name().toUtf8().data());
            }
        }
    );

    if (m_rootInfo) {
        m_rootInfo->setDesktopName(vd->x11DesktopNumber(), vd->name().toUtf8().data());
    }

    m_desktops.insert(position, vd);

    //update the id of displaced desktops
    for (uint i = position + 1; i < (uint)m_desktops.count(); ++i) {
        m_desktops[i]->setX11DesktopNumber(i + 1);
        if (m_rootInfo) {
            m_rootInfo->setDesktopName(i + 1, m_desktops[i]->name().toUtf8().data());
        }
    }

    save();

    updateRootInfo();
    emit desktopCreated(vd);
    emit countChanged(m_desktops.count()-1, m_desktops.count());
    return vd;
}

void VirtualDesktopManager::removeVirtualDesktop(const QByteArray &id)
{
    //don't end up without any desktop
    if (m_desktops.count() == 1) {
        return;
    }
    auto desktop = desktopForId(id);
    if (!desktop) {
        return;
    }

    const uint oldCurrent = m_current->x11DesktopNumber();
    const uint i = desktop->x11DesktopNumber() - 1;
    m_desktops.remove(i);

    for (uint j = i; j < (uint)m_desktops.count(); ++j) {
        m_desktops[j]->setX11DesktopNumber(j + 1);
        if (m_rootInfo) {
            m_rootInfo->setDesktopName(j + 1, m_desktops[j]->name().toUtf8().data());
        }
    }

    const uint newCurrent = qMin(oldCurrent, (uint)m_desktops.count());
    m_current = m_desktops.at(newCurrent - 1);
    if (oldCurrent != newCurrent) {
        emit currentChanged(oldCurrent, newCurrent);
    }

    save();

    updateRootInfo();
    emit desktopRemoved(desktop);
    emit countChanged(m_desktops.count()+1, m_desktops.count());

    desktop->deleteLater();
}

uint VirtualDesktopManager::current() const
{
    return m_current ? m_current->x11DesktopNumber() : 0;
}

VirtualDesktop *VirtualDesktopManager::currentDesktop() const
{
    return m_current;
}

bool VirtualDesktopManager::setCurrent(uint newDesktop)
{
    if (newDesktop < 1 || newDesktop > count() || newDesktop == current()) {
        return false;
    }
    auto d = desktopForX11Id(newDesktop);
    Q_ASSERT(d);
    return setCurrent(d);
}

bool VirtualDesktopManager::setCurrent(VirtualDesktop *newDesktop)
{
    Q_ASSERT(newDesktop);
    if (m_current == newDesktop) {
        return false;
    }
    const uint oldDesktop = current();
    m_current = newDesktop;
    emit currentChanged(oldDesktop, newDesktop->x11DesktopNumber());
    return true;
}

void VirtualDesktopManager::setCount(uint count)
{
    count = qBound<uint>(1, count, VirtualDesktopManager::maximum());
    if (count == uint(m_desktops.count())) {
        // nothing to change
        return;
    }
    QList<VirtualDesktop *> newDesktops;
    const uint oldCount = m_desktops.count();
    //this explicit check makes it more readable
    if ((uint)m_desktops.count() > count) {
        const auto desktopsToRemove = m_desktops.mid(count);
        m_desktops.resize(count);
        if (m_current) {
            uint oldCurrent = current();
            uint newCurrent = qMin(oldCurrent, count);
            m_current = m_desktops.at(newCurrent - 1);
            if (oldCurrent != newCurrent) {
                emit currentChanged(oldCurrent, newCurrent);
            }
        }
        for (auto desktop : desktopsToRemove) {
            emit desktopRemoved(desktop);
            desktop->deleteLater();
        }
    } else {
        while (uint(m_desktops.count()) < count) {
            auto vd = new VirtualDesktop(this);
            const int x11Number = m_desktops.count() + 1;
            vd->setX11DesktopNumber(x11Number);
            vd->setName(defaultName(x11Number));
            if (!s_loadingDesktopSettings) {
                vd->setId(generateDesktopId());
            }
            m_desktops << vd;
            newDesktops << vd;
            connect(vd, &VirtualDesktop::nameChanged, this,
                [this, vd] {
                    if (m_rootInfo) {
                        m_rootInfo->setDesktopName(vd->x11DesktopNumber(), vd->name().toUtf8().data());
                    }
                }
            );
            if (m_rootInfo) {
                m_rootInfo->setDesktopName(vd->x11DesktopNumber(), vd->name().toUtf8().data());
            }
        }
    }

    updateRootInfo();

    if (!s_loadingDesktopSettings) {
        save();
    }
    for (auto vd : newDesktops) {
        emit desktopCreated(vd);
    }
    emit countChanged(oldCount, m_desktops.count());
}


uint VirtualDesktopManager::rows() const
{
    return m_rows;
}

void VirtualDesktopManager::setRows(uint rows)
{
    if (rows == 0 || rows > count() || rows == m_rows) {
        return;
    }

    m_rows = rows;

    int columns = count() / m_rows;
    if (count() % m_rows > 0) {
        columns++;
    }
    if (m_rootInfo) {
        m_rootInfo->setDesktopLayout(NET::OrientationHorizontal, columns, m_rows, NET::DesktopLayoutCornerTopLeft);
        m_rootInfo->activate();
    }

    updateLayout();

    //rowsChanged will be emitted by setNETDesktopLayout called by updateLayout
}

void VirtualDesktopManager::updateRootInfo()
{
    if (!m_rootInfo) {
        // Make sure the layout is still valid
        updateLayout();
        return;
    }
    const int n = count();
    m_rootInfo->setNumberOfDesktops(n);
    NETPoint *viewports = new NETPoint[n];
    m_rootInfo->setDesktopViewport(n, *viewports);
    delete[] viewports;
    // Make sure the layout is still valid
    updateLayout();
}

void VirtualDesktopManager::updateLayout()
{
    m_rows = qMin(m_rows, count());
    int columns = count() / m_rows;
    Qt::Orientation orientation = Qt::Horizontal;
    if (m_rootInfo) {
        // TODO: Is there a sane way to avoid overriding the existing grid?
        columns = m_rootInfo->desktopLayoutColumnsRows().width();
        m_rows = qMax(1, m_rootInfo->desktopLayoutColumnsRows().height());
        orientation = m_rootInfo->desktopLayoutOrientation() == NET::OrientationHorizontal ? Qt::Horizontal : Qt::Vertical;
    }

    if (columns == 0) {
        // Not given, set default layout
        m_rows = count() == 1u ? 1 : 2;
        columns = count() / m_rows;
    }
    setNETDesktopLayout(orientation,
        columns, m_rows, 0 //rootInfo->desktopLayoutCorner() // Not really worth implementing right now.
    );
}

void VirtualDesktopManager::load()
{
    s_loadingDesktopSettings = true;
    if (!m_config) {
        return;
    }

    KConfigGroup group(m_config, QStringLiteral("Desktops"));
    const int n = group.readEntry("Number", 1);
    setCount(n);

    for (int i = 1; i <= n; i++) {
        QString s = group.readEntry(QStringLiteral("Name_%1").arg(i), i18n("Desktop %1", i));
        if (m_rootInfo) {
            m_rootInfo->setDesktopName(i, s.toUtf8().data());
        }
        m_desktops[i-1]->setName(s.toUtf8().data());

        const QString sId = group.readEntry(QStringLiteral("Id_%1").arg(i), QString());

        if (m_desktops[i-1]->id().isEmpty()) {
            m_desktops[i-1]->setId(sId.isEmpty() ? generateDesktopId() : sId.toUtf8());
        } else {
            Q_ASSERT(sId.isEmpty() || m_desktops[i-1]->id() == sId.toUtf8().data());
        }

        // TODO: update desktop focus chain, why?
//         m_desktopFocusChain.value()[i-1] = i;
    }

    int rows = group.readEntry<int>("Rows", 2);
    m_rows = qBound(1, rows, n);

    s_loadingDesktopSettings = false;
}

void VirtualDesktopManager::save()
{
    if (s_loadingDesktopSettings) {
        return;
    }
    if (!m_config) {
        return;
    }
    KConfigGroup group(m_config, QStringLiteral("Desktops"));

    for (int i = count() + 1;  group.hasKey(QStringLiteral("Id_%1").arg(i)); i++) {
        group.deleteEntry(QStringLiteral("Id_%1").arg(i));
        group.deleteEntry(QStringLiteral("Name_%1").arg(i));
    }

    group.writeEntry("Number", count());
    for (uint i = 1; i <= count(); ++i) {
        QString s = name(i);
        const QString defaultvalue = defaultName(i);
        if (s.isEmpty()) {
            s = defaultvalue;
            if (m_rootInfo) {
                m_rootInfo->setDesktopName(i, s.toUtf8().data());
            }
        }

        if (s != defaultvalue) {
            group.writeEntry(QStringLiteral("Name_%1").arg(i), s);
        } else {
            QString currentvalue = group.readEntry(QStringLiteral("Name_%1").arg(i), QString());
            if (currentvalue != defaultvalue) {
                group.deleteEntry(QStringLiteral("Name_%1").arg(i));
            }
        }
        group.writeEntry(QStringLiteral("Id_%1").arg(i), m_desktops[i-1]->id());
    }

    group.writeEntry("Rows", m_rows);

    // Save to disk
    group.sync();
}

QString VirtualDesktopManager::defaultName(int desktop) const
{
    return i18n("Desktop %1", desktop);
}

void VirtualDesktopManager::setNETDesktopLayout(Qt::Orientation orientation, uint width, uint height, int startingCorner)
{
    Q_UNUSED(startingCorner);   // Not really worth implementing right now.
    const uint count = m_desktops.count();

    // Calculate valid grid size
    Q_ASSERT(width > 0 || height > 0);
    if ((width <= 0) && (height > 0)) {
        width = (count + height - 1) / height;
    } else if ((height <= 0) && (width > 0)) {
        height = (count + width - 1) / width;
    }
    while (width * height < count) {
        if (orientation == Qt::Horizontal) {
            ++width;
        } else {
            ++height;
        }
    }

    m_rows = qMax(1u, height);

    m_grid.update(QSize(width, height), orientation, m_desktops);
    // TODO: why is there no call to m_rootInfo->setDesktopLayout?
    emit layoutChanged(width, height);
    emit rowsChanged(height);
}

void VirtualDesktopManager::initShortcuts()
{
    initSwitchToShortcuts();

    QAction *nextAction = addAction(QStringLiteral("Switch to Next Desktop"), i18n("Switch to Next Desktop"), &VirtualDesktopManager::slotNext);
    input()->registerTouchpadSwipeShortcut(SwipeDirection::Right, nextAction);
    QAction *previousAction = addAction(QStringLiteral("Switch to Previous Desktop"), i18n("Switch to Previous Desktop"), &VirtualDesktopManager::slotPrevious);
    input()->registerTouchpadSwipeShortcut(SwipeDirection::Left, previousAction);
    QAction *slotRightAction = addAction(QStringLiteral("Switch One Desktop to the Right"), i18n("Switch One Desktop to the Right"), &VirtualDesktopManager::slotRight);
    KGlobalAccel::setGlobalShortcut(slotRightAction, QKeySequence(Qt::CTRL + Qt::META + Qt::Key_Right));
    QAction *slotLeftAction = addAction(QStringLiteral("Switch One Desktop to the Left"), i18n("Switch One Desktop to the Left"), &VirtualDesktopManager::slotLeft);
    KGlobalAccel::setGlobalShortcut(slotLeftAction, QKeySequence(Qt::CTRL + Qt::META + Qt::Key_Left));
    QAction *slotUpAction = addAction(QStringLiteral("Switch One Desktop Up"), i18n("Switch One Desktop Up"), &VirtualDesktopManager::slotUp);
    KGlobalAccel::setGlobalShortcut(slotUpAction, QKeySequence(Qt::CTRL + Qt::META + Qt::Key_Up));
    QAction *slotDownAction = addAction(QStringLiteral("Switch One Desktop Down"), i18n("Switch One Desktop Down"), &VirtualDesktopManager::slotDown);
    KGlobalAccel::setGlobalShortcut(slotDownAction, QKeySequence(Qt::CTRL + Qt::META + Qt::Key_Down));

    // axis events
    input()->registerAxisShortcut(Qt::ControlModifier | Qt::AltModifier, PointerAxisDown,
                                  findChild<QAction*>(QStringLiteral("Switch to Next Desktop")));
    input()->registerAxisShortcut(Qt::ControlModifier | Qt::AltModifier, PointerAxisUp,
                                  findChild<QAction*>(QStringLiteral("Switch to Previous Desktop")));
}

void VirtualDesktopManager::initSwitchToShortcuts()
{
    const QString toDesktop = QStringLiteral("Switch to Desktop %1");
    const KLocalizedString toDesktopLabel = ki18n("Switch to Desktop %1");
    addAction(toDesktop, toDesktopLabel, 1, QKeySequence(Qt::CTRL + Qt::Key_F1), &VirtualDesktopManager::slotSwitchTo);
    addAction(toDesktop, toDesktopLabel, 2, QKeySequence(Qt::CTRL + Qt::Key_F2), &VirtualDesktopManager::slotSwitchTo);
    addAction(toDesktop, toDesktopLabel, 3, QKeySequence(Qt::CTRL + Qt::Key_F3), &VirtualDesktopManager::slotSwitchTo);
    addAction(toDesktop, toDesktopLabel, 4, QKeySequence(Qt::CTRL + Qt::Key_F4), &VirtualDesktopManager::slotSwitchTo);

    for (uint i = 5; i <= maximum(); ++i) {
        addAction(toDesktop, toDesktopLabel, i, QKeySequence(), &VirtualDesktopManager::slotSwitchTo);
    }
}

QAction *VirtualDesktopManager::addAction(const QString &name, const KLocalizedString &label, uint value, const QKeySequence &key, void (VirtualDesktopManager::*slot)())
{
    QAction *a = new QAction(this);
    a->setProperty("componentName", QStringLiteral(KWIN_NAME));
    a->setObjectName(name.arg(value));
    a->setText(label.subs(value).toString());
    a->setData(value);
    KGlobalAccel::setGlobalShortcut(a, key);
    input()->registerShortcut(key, a, this, slot);
    return a;
}

QAction *VirtualDesktopManager::addAction(const QString &name, const QString &label, void (VirtualDesktopManager::*slot)())
{
    QAction *a = new QAction(this);
    a->setProperty("componentName", QStringLiteral(KWIN_NAME));
    a->setObjectName(name);
    a->setText(label);
    KGlobalAccel::setGlobalShortcut(a, QKeySequence());
    input()->registerShortcut(QKeySequence(), a, this, slot);
    return a;
}

void VirtualDesktopManager::slotSwitchTo()
{
    QAction *act = qobject_cast<QAction*>(sender());
    if (!act) {
        return;
    }
    bool ok = false;
    const uint i = act->data().toUInt(&ok);
    if (!ok) {
        return;
    }
    setCurrent(i);
}

void VirtualDesktopManager::setNavigationWrappingAround(bool enabled)
{
    if (enabled == m_navigationWrapsAround) {
        return;
    }
    m_navigationWrapsAround = enabled;
    emit navigationWrappingAroundChanged();
}

void VirtualDesktopManager::slotDown()
{
    moveTo<DesktopBelow>(isNavigationWrappingAround());
}

void VirtualDesktopManager::slotLeft()
{
    moveTo<DesktopLeft>(isNavigationWrappingAround());
}

void VirtualDesktopManager::slotPrevious()
{
    moveTo<DesktopPrevious>(isNavigationWrappingAround());
}

void VirtualDesktopManager::slotNext()
{
    moveTo<DesktopNext>(isNavigationWrappingAround());
}

void VirtualDesktopManager::slotRight()
{
    moveTo<DesktopRight>(isNavigationWrappingAround());
}

void VirtualDesktopManager::slotUp()
{
    moveTo<DesktopAbove>(isNavigationWrappingAround());
}

} // KWin
