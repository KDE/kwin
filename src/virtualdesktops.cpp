/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtualdesktops.h"
#include "input.h"
#include "wayland/plasmavirtualdesktop.h"
// KDE
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLocalizedString>
#if KWIN_BUILD_X11
#include <NETWM>
#endif

// Qt
#include <QAction>
#include <QDebug>
#include <QUuid>

#include <algorithm>

namespace KWin
{

static bool s_loadingDesktopSettings = false;
static const double GESTURE_SWITCH_THRESHOLD = .25;

static QString generateDesktopId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

VirtualDesktop::VirtualDesktop(QObject *parent)
    : QObject(parent)
{
}

VirtualDesktop::~VirtualDesktop()
{
    Q_EMIT aboutToBeDestroyed();
}

void VirtualDesktopManager::setVirtualDesktopManagement(PlasmaVirtualDesktopManagementInterface *management)
{
    Q_ASSERT(!m_virtualDesktopManagement);
    m_virtualDesktopManagement = management;

    auto createPlasmaVirtualDesktop = [this](VirtualDesktop *desktop) {
        PlasmaVirtualDesktopInterface *pvd = m_virtualDesktopManagement->createDesktop(desktop->id(), desktop->x11DesktopNumber() - 1);
        pvd->setName(desktop->name());
        pvd->sendDone();

        connect(desktop, &VirtualDesktop::nameChanged, pvd, [desktop, pvd]() {
            pvd->setName(desktop->name());
            pvd->sendDone();
        });
        connect(pvd, &PlasmaVirtualDesktopInterface::activateRequested, this, [this, desktop]() {
            setCurrent(desktop);
        });
    };

    connect(this, &VirtualDesktopManager::desktopAdded, m_virtualDesktopManagement, createPlasmaVirtualDesktop);

    connect(this, &VirtualDesktopManager::rowsChanged, m_virtualDesktopManagement, [this](uint rows) {
        m_virtualDesktopManagement->setRows(rows);
        m_virtualDesktopManagement->sendDone();
    });

    // handle removed: from VirtualDesktopManager to the wayland interface
    connect(this, &VirtualDesktopManager::desktopRemoved, m_virtualDesktopManagement, [this](VirtualDesktop *desktop) {
        m_virtualDesktopManagement->removeDesktop(desktop->id());
    });

    // create a new desktop when the client asks to
    connect(m_virtualDesktopManagement, &PlasmaVirtualDesktopManagementInterface::desktopCreateRequested, this, [this](const QString &name, quint32 position) {
        createVirtualDesktop(position, name);
    });

    // remove when the client asks to
    connect(m_virtualDesktopManagement, &PlasmaVirtualDesktopManagementInterface::desktopRemoveRequested, this, [this](const QString &id) {
        // here there can be some nice kauthorized check?
        // remove only from VirtualDesktopManager, the other connections will remove it from m_virtualDesktopManagement as well
        removeVirtualDesktop(id);
    });

    connect(this, &VirtualDesktopManager::currentChanged, m_virtualDesktopManagement, [this]() {
        const QList<PlasmaVirtualDesktopInterface *> deskIfaces = m_virtualDesktopManagement->desktops();
        for (auto *deskInt : deskIfaces) {
            if (deskInt->id() == currentDesktop()->id()) {
                deskInt->setActive(true);
            } else {
                deskInt->setActive(false);
            }
        }
    });

    std::for_each(m_desktops.constBegin(), m_desktops.constEnd(), createPlasmaVirtualDesktop);

    m_virtualDesktopManagement->setRows(rows());
    m_virtualDesktopManagement->sendDone();
}

void VirtualDesktop::setId(const QString &id)
{
    Q_ASSERT(m_id.isEmpty());
    m_id = id;
}

void VirtualDesktop::setX11DesktopNumber(uint number)
{
    // x11DesktopNumber can be changed now
    if (static_cast<uint>(m_x11DesktopNumber) == number) {
        return;
    }

    m_x11DesktopNumber = number;

    if (m_x11DesktopNumber != 0) {
        Q_EMIT x11DesktopNumberChanged();
    }
}

void VirtualDesktop::setName(const QString &name)
{
    if (m_name == name) {
        return;
    }
    m_name = name;
    Q_EMIT nameChanged();
}

VirtualDesktopGrid::VirtualDesktopGrid()
    : m_size(1, 2) // Default to tow rows
    , m_grid(QList<QList<VirtualDesktop *>>{QList<VirtualDesktop *>{}, QList<VirtualDesktop *>{}})
{
}

VirtualDesktopGrid::~VirtualDesktopGrid() = default;

void VirtualDesktopGrid::update(const QSize &size, const QList<VirtualDesktop *> &desktops)
{
    // Set private variables
    m_size = size;
    const uint width = size.width();
    const uint height = size.height();

    m_grid.clear();
    auto it = desktops.begin();
    auto end = desktops.end();
    for (uint y = 0; y < height; ++y) {
        QList<VirtualDesktop *> row;
        for (uint x = 0; x < width && it != end; ++x) {
            row << *it;
            it++;
        }
        m_grid << row;
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
#if KWIN_BUILD_X11
    , m_rootInfo(nullptr)
#endif
    , m_swipeGestureReleasedY(new QAction(this))
    , m_swipeGestureReleasedX(new QAction(this))
{
}

VirtualDesktopManager::~VirtualDesktopManager()
{
    s_manager = nullptr;
}

void VirtualDesktopManager::setRootInfo(NETRootInfo *info)
{
#if KWIN_BUILD_X11
    m_rootInfo = info;

    // Nothing will be connected to rootInfo
    if (m_rootInfo) {
        updateRootInfo();
        m_rootInfo->setCurrentDesktop(currentDesktop()->x11DesktopNumber());
        for (auto *vd : std::as_const(m_desktops)) {
            m_rootInfo->setDesktopName(vd->x11DesktopNumber(), vd->name().toUtf8().data());
        }
    }
#endif
}

VirtualDesktop *VirtualDesktopManager::inDirection(VirtualDesktop *desktop, Direction direction, bool wrap)
{
    switch (direction) {
    case Direction::Up:
        return above(desktop, wrap);
    case Direction::Down:
        return below(desktop, wrap);
    case Direction::Right:
        return toRight(desktop, wrap);
    case Direction::Left:
        return toLeft(desktop, wrap);
    case Direction::Next:
        return next(desktop, wrap);
    case Direction::Previous:
        return previous(desktop, wrap);
    }
    Q_UNREACHABLE();
}

uint VirtualDesktopManager::inDirection(uint desktop, Direction direction, bool wrap)
{
    return inDirection(desktopForX11Id(desktop), direction, wrap)->x11DesktopNumber();
}

void VirtualDesktopManager::moveTo(Direction direction, bool wrap)
{
    setCurrent(inDirection(nullptr, direction, wrap));
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

VirtualDesktop *VirtualDesktopManager::desktopForId(const QString &id) const
{
    auto desk = std::find_if(
        m_desktops.constBegin(),
        m_desktops.constEnd(),
        [id](const VirtualDesktop *desk) {
            return desk->id() == id;
        });

    if (desk != m_desktops.constEnd()) {
        return *desk;
    }

    return nullptr;
}

VirtualDesktop *VirtualDesktopManager::createVirtualDesktop(uint position, const QString &name)
{
    // too many, can't insert new ones
    if ((uint)m_desktops.count() == VirtualDesktopManager::maximum()) {
        return nullptr;
    }

    position = std::clamp(position, 0u, static_cast<uint>(m_desktops.count()));

    QString desktopName = name;
    if (desktopName.isEmpty()) {
        desktopName = defaultName(position + 1);
    }

    auto *vd = new VirtualDesktop(this);
    vd->setX11DesktopNumber(position + 1);
    vd->setId(generateDesktopId());
    vd->setName(desktopName);

#if KWIN_BUILD_X11
    connect(vd, &VirtualDesktop::nameChanged, this, [this, vd]() {
        if (m_rootInfo) {
            m_rootInfo->setDesktopName(vd->x11DesktopNumber(), vd->name().toUtf8().data());
        }
    });

    if (m_rootInfo) {
        m_rootInfo->setDesktopName(vd->x11DesktopNumber(), vd->name().toUtf8().data());
    }
#endif

    m_desktops.insert(position, vd);

    // update the id of displaced desktops
    for (uint i = position + 1; i < (uint)m_desktops.count(); ++i) {
        m_desktops[i]->setX11DesktopNumber(i + 1);
#if KWIN_BUILD_X11
        if (m_rootInfo) {
            m_rootInfo->setDesktopName(i + 1, m_desktops[i]->name().toUtf8().data());
        }
#endif
    }

    save();

    updateLayout();
    updateRootInfo();
    Q_EMIT desktopAdded(vd);
    Q_EMIT countChanged(m_desktops.count() - 1, m_desktops.count());
    return vd;
}

void VirtualDesktopManager::removeVirtualDesktop(const QString &id)
{
    auto desktop = desktopForId(id);
    if (desktop) {
        removeVirtualDesktop(desktop);
    }
}

void VirtualDesktopManager::removeVirtualDesktop(VirtualDesktop *desktop)
{
    // don't end up without any desktop
    if (m_desktops.count() == 1) {
        return;
    }

    const int i = m_desktops.indexOf(desktop);
    m_desktops.remove(i);

    for (int j = i; j < m_desktops.count(); ++j) {
        m_desktops[j]->setX11DesktopNumber(j + 1);
#if KWIN_BUILD_X11
        if (m_rootInfo) {
            m_rootInfo->setDesktopName(j + 1, m_desktops[j]->name().toUtf8().data());
        }
#endif
    }

    if (m_current == desktop) {
        m_current = (i < m_desktops.count()) ? m_desktops.at(i) : m_desktops.constLast();
        Q_EMIT currentChanged(desktop, m_current);
    }

    updateLayout();
    updateRootInfo();
    save();

    Q_EMIT desktopRemoved(desktop);
    Q_EMIT countChanged(m_desktops.count() + 1, m_desktops.count());

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
    if (newDesktop < 1 || newDesktop > count()) {
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
    VirtualDesktop *oldDesktop = currentDesktop();
    m_current = newDesktop;
    Q_EMIT currentChanged(oldDesktop, newDesktop);
    return true;
}

void VirtualDesktopManager::setCount(uint count)
{
    count = std::clamp<uint>(count, 1, VirtualDesktopManager::maximum());
    if (count == uint(m_desktops.count())) {
        // nothing to change
        return;
    }
    QList<VirtualDesktop *> newDesktops;
    const uint oldCount = m_desktops.count();
    // this explicit check makes it more readable
    if ((uint)m_desktops.count() > count) {
        const auto desktopsToRemove = m_desktops.mid(count);
        m_desktops.resize(count);
        if (m_current && desktopsToRemove.contains(m_current)) {
            VirtualDesktop *oldCurrent = m_current;
            m_current = m_desktops.last();
            Q_EMIT currentChanged(oldCurrent, m_current);
        }
        for (auto desktop : desktopsToRemove) {
            Q_EMIT desktopRemoved(desktop);
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
#if KWIN_BUILD_X11
            connect(vd, &VirtualDesktop::nameChanged, this, [this, vd]() {
                if (m_rootInfo) {
                    m_rootInfo->setDesktopName(vd->x11DesktopNumber(), vd->name().toUtf8().data());
                }
            });
            if (m_rootInfo) {
                m_rootInfo->setDesktopName(vd->x11DesktopNumber(), vd->name().toUtf8().data());
            }
#endif
        }
    }

    if (!m_current) {
        m_current = m_desktops.at(0);
    }

    updateLayout();
    updateRootInfo();

    if (!s_loadingDesktopSettings) {
        save();
    }
    for (auto vd : std::as_const(newDesktops)) {
        Q_EMIT desktopAdded(vd);
    }
    Q_EMIT countChanged(oldCount, m_desktops.count());
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

    updateLayout();
    updateRootInfo();
}

void VirtualDesktopManager::updateRootInfo()
{
#if KWIN_BUILD_X11
    if (m_rootInfo) {
        const int n = count();
        m_rootInfo->setNumberOfDesktops(n);
        NETPoint *viewports = new NETPoint[n];
        m_rootInfo->setDesktopViewport(n, *viewports);
        delete[] viewports;
        m_rootInfo->setDesktopLayout(NET::OrientationHorizontal, m_grid.width(), m_grid.height(), NET::DesktopLayoutCornerTopLeft);
    }
#endif
}

void VirtualDesktopManager::updateLayout()
{
    m_rows = std::min(m_rows, count());

    int columns = count() / m_rows;
    if (count() % m_rows > 0) {
        columns++;
    }

    m_grid.update(QSize(columns, m_rows), m_desktops);

    Q_EMIT layoutChanged(columns, m_rows);
    Q_EMIT rowsChanged(m_rows);
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
#if KWIN_BUILD_X11
        if (m_rootInfo) {
            m_rootInfo->setDesktopName(i, s.toUtf8().data());
        }
#endif
        m_desktops[i - 1]->setName(s);

        const QString sId = group.readEntry(QStringLiteral("Id_%1").arg(i), QString());

        if (m_desktops[i - 1]->id().isEmpty()) {
            m_desktops[i - 1]->setId(sId.isEmpty() ? generateDesktopId() : sId);
        } else {
            Q_ASSERT(sId.isEmpty() || m_desktops[i - 1]->id() == sId);
        }

        // TODO: update desktop focus chain, why?
        //         m_desktopFocusChain.value()[i-1] = i;
    }

    int rows = group.readEntry<int>("Rows", 2);
    m_rows = std::clamp(rows, 1, n);

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

    for (int i = count() + 1; group.hasKey(QStringLiteral("Id_%1").arg(i)); i++) {
        group.deleteEntry(QStringLiteral("Id_%1").arg(i));
        group.deleteEntry(QStringLiteral("Name_%1").arg(i));
    }

    group.writeEntry("Number", count());
    for (VirtualDesktop *desktop : std::as_const(m_desktops)) {
        const uint position = desktop->x11DesktopNumber();

        QString s = desktop->name();
        const QString defaultvalue = defaultName(position);
        if (s.isEmpty()) {
            s = defaultvalue;
#if KWIN_BUILD_X11
            if (m_rootInfo) {
                m_rootInfo->setDesktopName(position, s.toUtf8().data());
            }
#endif
        }

        if (s != defaultvalue) {
            group.writeEntry(QStringLiteral("Name_%1").arg(position), s);
        } else {
            QString currentvalue = group.readEntry(QStringLiteral("Name_%1").arg(position), QString());
            if (currentvalue != defaultvalue) {
                group.deleteEntry(QStringLiteral("Name_%1").arg(position));
            }
        }
        group.writeEntry(QStringLiteral("Id_%1").arg(position), desktop->id());
    }

    group.writeEntry("Rows", m_rows);

    // Save to disk
    group.sync();
}

QString VirtualDesktopManager::defaultName(int desktop) const
{
    return i18n("Desktop %1", desktop);
}

void VirtualDesktopManager::initShortcuts()
{
    initSwitchToShortcuts();

    addAction(QStringLiteral("Switch to Next Desktop"), i18n("Switch to Next Desktop"), QKeySequence(), &VirtualDesktopManager::slotNext);
    addAction(QStringLiteral("Switch to Previous Desktop"), i18n("Switch to Previous Desktop"), QKeySequence(), &VirtualDesktopManager::slotPrevious);

    // shortcuts
    addAction(QStringLiteral("Switch One Desktop to the Right"), i18n("Switch One Desktop to the Right"), QKeySequence(Qt::CTRL | Qt::META | Qt::Key_Right), &VirtualDesktopManager::slotRight);
    addAction(QStringLiteral("Switch One Desktop to the Left"), i18n("Switch One Desktop to the Left"), QKeySequence(Qt::CTRL | Qt::META | Qt::Key_Left), &VirtualDesktopManager::slotLeft);
    addAction(QStringLiteral("Switch One Desktop Up"), i18n("Switch One Desktop Up"), QKeySequence(Qt::CTRL | Qt::META | Qt::Key_Up), &VirtualDesktopManager::slotUp);
    addAction(QStringLiteral("Switch One Desktop Down"), i18n("Switch One Desktop Down"), QKeySequence(Qt::CTRL | Qt::META | Qt::Key_Down), &VirtualDesktopManager::slotDown);

    // Gestures
    // These connections decide which desktop to end on after gesture ends
    connect(m_swipeGestureReleasedX.get(), &QAction::triggered, this, &VirtualDesktopManager::gestureReleasedX);
    connect(m_swipeGestureReleasedY.get(), &QAction::triggered, this, &VirtualDesktopManager::gestureReleasedY);

    const auto left = [this](qreal cb) {
        if (grid().width() > 1) {
            m_currentDesktopOffset.setX(cb);
            Q_EMIT currentChanging(currentDesktop(), m_currentDesktopOffset);
        }
    };
    const auto right = [this](qreal cb) {
        if (grid().width() > 1) {
            m_currentDesktopOffset.setX(-cb);
            Q_EMIT currentChanging(currentDesktop(), m_currentDesktopOffset);
        }
    };
    input()->registerTouchpadSwipeShortcut(SwipeDirection::Left, 3, m_swipeGestureReleasedX.get(), left);
    input()->registerTouchpadSwipeShortcut(SwipeDirection::Right, 3, m_swipeGestureReleasedX.get(), right);
    input()->registerTouchpadSwipeShortcut(SwipeDirection::Left, 4, m_swipeGestureReleasedX.get(), left);
    input()->registerTouchpadSwipeShortcut(SwipeDirection::Right, 4, m_swipeGestureReleasedX.get(), right);
    input()->registerTouchpadSwipeShortcut(SwipeDirection::Down, 3, m_swipeGestureReleasedY.get(), [this](qreal cb) {
        if (grid().height() > 1) {
            m_currentDesktopOffset.setY(-cb);
            Q_EMIT currentChanging(currentDesktop(), m_currentDesktopOffset);
        }
    });
    input()->registerTouchpadSwipeShortcut(SwipeDirection::Up, 3, m_swipeGestureReleasedY.get(), [this](qreal cb) {
        if (grid().height() > 1) {
            m_currentDesktopOffset.setY(cb);
            Q_EMIT currentChanging(currentDesktop(), m_currentDesktopOffset);
        }
    });
    input()->registerTouchscreenSwipeShortcut(SwipeDirection::Left, 3, m_swipeGestureReleasedX.get(), left);
    input()->registerTouchscreenSwipeShortcut(SwipeDirection::Right, 3, m_swipeGestureReleasedX.get(), right);

    // axis events
    input()->registerAxisShortcut(Qt::MetaModifier | Qt::AltModifier, PointerAxisDown,
                                  findChild<QAction *>(QStringLiteral("Switch to Next Desktop")));
    input()->registerAxisShortcut(Qt::MetaModifier | Qt::AltModifier, PointerAxisUp,
                                  findChild<QAction *>(QStringLiteral("Switch to Previous Desktop")));
}

void VirtualDesktopManager::gestureReleasedY()
{
    // Note that if desktop wrapping is disabled and there's no desktop above or below,
    // above() and below() will return the current desktop.
    VirtualDesktop *target = m_current;
    if (m_currentDesktopOffset.y() <= -GESTURE_SWITCH_THRESHOLD) {
        target = above(m_current, isNavigationWrappingAround());
    } else if (m_currentDesktopOffset.y() >= GESTURE_SWITCH_THRESHOLD) {
        target = below(m_current, isNavigationWrappingAround());
    }

    // If the current desktop has not changed, consider that the gesture has been canceled.
    if (m_current != target) {
        setCurrent(target);
    } else {
        Q_EMIT currentChangingCancelled();
    }
    m_currentDesktopOffset = QPointF(0, 0);
}

void VirtualDesktopManager::gestureReleasedX()
{
    // Note that if desktop wrapping is disabled and there's no desktop to left or right,
    // toLeft() and toRight() will return the current desktop.
    VirtualDesktop *target = m_current;
    if (m_currentDesktopOffset.x() <= -GESTURE_SWITCH_THRESHOLD) {
        target = toLeft(m_current, isNavigationWrappingAround());
    } else if (m_currentDesktopOffset.x() >= GESTURE_SWITCH_THRESHOLD) {
        target = toRight(m_current, isNavigationWrappingAround());
    }

    // If the current desktop has not changed, consider that the gesture has been canceled.
    if (m_current != target) {
        setCurrent(target);
    } else {
        Q_EMIT currentChangingCancelled();
    }
    m_currentDesktopOffset = QPointF(0, 0);
}

void VirtualDesktopManager::initSwitchToShortcuts()
{
    const QString toDesktop = QStringLiteral("Switch to Desktop %1");
    const KLocalizedString toDesktopLabel = ki18n("Switch to Desktop %1");
    addAction(toDesktop, toDesktopLabel, 1, QKeySequence(Qt::CTRL | Qt::Key_F1), &VirtualDesktopManager::slotSwitchTo);
    addAction(toDesktop, toDesktopLabel, 2, QKeySequence(Qt::CTRL | Qt::Key_F2), &VirtualDesktopManager::slotSwitchTo);
    addAction(toDesktop, toDesktopLabel, 3, QKeySequence(Qt::CTRL | Qt::Key_F3), &VirtualDesktopManager::slotSwitchTo);
    addAction(toDesktop, toDesktopLabel, 4, QKeySequence(Qt::CTRL | Qt::Key_F4), &VirtualDesktopManager::slotSwitchTo);

    for (uint i = 5; i <= maximum(); ++i) {
        addAction(toDesktop, toDesktopLabel, i, QKeySequence(), &VirtualDesktopManager::slotSwitchTo);
    }
}

QAction *VirtualDesktopManager::addAction(const QString &name, const KLocalizedString &label, uint value, const QKeySequence &key, void (VirtualDesktopManager::*slot)())
{
    QAction *a = new QAction(this);
    a->setProperty("componentName", QStringLiteral("kwin"));
    a->setObjectName(name.arg(value));
    a->setText(label.subs(value).toString());
    a->setData(value);
    KGlobalAccel::setGlobalShortcut(a, key);
    connect(a, &QAction::triggered, this, slot);
    return a;
}

QAction *VirtualDesktopManager::addAction(const QString &name, const QString &label, const QKeySequence &key, void (VirtualDesktopManager::*slot)())
{
    QAction *a = new QAction(this);
    a->setProperty("componentName", QStringLiteral("kwin"));
    a->setObjectName(name);
    a->setText(label);
    KGlobalAccel::setGlobalShortcut(a, key);
    connect(a, &QAction::triggered, this, slot);
    return a;
}

void VirtualDesktopManager::slotSwitchTo()
{
    QAction *act = qobject_cast<QAction *>(sender());
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
    Q_EMIT navigationWrappingAroundChanged();
}

void VirtualDesktopManager::slotDown()
{
    moveTo(Direction::Down, isNavigationWrappingAround());
}

void VirtualDesktopManager::slotLeft()
{
    moveTo(Direction::Left, isNavigationWrappingAround());
}

void VirtualDesktopManager::slotPrevious()
{
    moveTo(Direction::Previous, isNavigationWrappingAround());
}

void VirtualDesktopManager::slotNext()
{
    moveTo(Direction::Next, isNavigationWrappingAround());
}

void VirtualDesktopManager::slotRight()
{
    moveTo(Direction::Right, isNavigationWrappingAround());
}

void VirtualDesktopManager::slotUp()
{
    moveTo(Direction::Up, isNavigationWrappingAround());
}

} // KWin

#include "moc_virtualdesktops.cpp"
