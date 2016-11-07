/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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
#include "virtualdesktops.h"
#include "input.h"
// KDE
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <NETWM>
// Qt
#include <QAction>

namespace KWin {

extern int screen_number;

VirtualDesktop::VirtualDesktop(QObject *parent)
    : QObject(parent)
{
}

VirtualDesktop::~VirtualDesktop() = default;

void VirtualDesktop::setId(const QByteArray &id)
{
    Q_ASSERT(m_id.isEmpty());
    m_id = id;
}

void VirtualDesktop::setX11DesktopNumber(uint number)
{
    Q_ASSERT(m_x11DesktopNumber == 0);
    m_x11DesktopNumber = number;
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
    , m_grid(new uint[2])
{
    // Initializing grid array
    m_grid[0] = 0;
    m_grid[1] = 0;
}

VirtualDesktopGrid::~VirtualDesktopGrid()
{
    delete[] m_grid;
}

void VirtualDesktopGrid::update(const QSize &size, Qt::Orientation orientation)
{
    // Set private variables
    delete[] m_grid;
    m_size = size;
    const uint width = size.width();
    const uint height = size.height();
    const uint length = width * height;
    const uint desktopCount = VirtualDesktopManager::self()->count();
    m_grid = new uint[length];

    // Populate grid
    uint desktop = 1;
    if (orientation == Qt::Horizontal) {
        for (uint y = 0; y < height; ++y) {
            for (uint x = 0; x < width; ++x) {
                m_grid[y * width + x] = (desktop <= desktopCount ? desktop++ : 0);
            }
        }
    } else {
        for (uint x = 0; x < width; ++x) {
            for (uint y = 0; y < height; ++y) {
                m_grid[y * width + x] = (desktop <= desktopCount ? desktop++ : 0);
            }
        }
    }
}

QPoint VirtualDesktopGrid::gridCoords(uint id) const
{
    for (int y = 0; y < m_size.height(); ++y) {
        for (int x = 0; x < m_size.width(); ++x) {
            if (m_grid[y * m_size.width() + x] == id) {
                return QPoint(x, y);
            }
        }
    }
    return QPoint(-1, -1);
}

KWIN_SINGLETON_FACTORY_VARIABLE(VirtualDesktopManager, s_manager)

VirtualDesktopManager::VirtualDesktopManager(QObject *parent)
    : QObject(parent)
    , m_navigationWrapsAround(false)
    , m_rootInfo(NULL)
{
}

VirtualDesktopManager::~VirtualDesktopManager()
{
    s_manager = NULL;
}

QString VirtualDesktopManager::name(uint desktop) const
{
    if (!m_rootInfo) {
        return defaultName(desktop);
    }
    return QString::fromUtf8(m_rootInfo->desktopName(desktop));
}

uint VirtualDesktopManager::above(uint id, bool wrap) const
{
    if (id == 0) {
        id = current();
    }
    QPoint coords = m_grid.gridCoords(id);
    Q_ASSERT(coords.x() >= 0);
    while (true) {
        coords.ry()--;
        if (coords.y() < 0) {
            if (wrap) {
                coords.setY(m_grid.height() - 1);
            } else {
                return id; // Already at the top-most desktop
            }
        }
        const uint desktop = m_grid.at(coords);
        if (desktop > 0) {
            return desktop;
        }
    }
}

uint VirtualDesktopManager::toRight(uint id, bool wrap) const
{
    if (id == 0) {
        id = current();
    }
    QPoint coords = m_grid.gridCoords(id);
    Q_ASSERT(coords.x() >= 0);
    while (true) {
        coords.rx()++;
        if (coords.x() >= m_grid.width()) {
            if (wrap) {
                coords.setX(0);
            } else {
                return id; // Already at the right-most desktop
            }
        }
        const uint desktop = m_grid.at(coords);
        if (desktop > 0) {
            return desktop;
        }
    }
}

uint VirtualDesktopManager::below(uint id, bool wrap) const
{
    if (id == 0) {
        id = current();
    }
    QPoint coords = m_grid.gridCoords(id);
    Q_ASSERT(coords.x() >= 0);
    while (true) {
        coords.ry()++;
        if (coords.y() >= m_grid.height()) {
            if (wrap) {
                coords.setY(0);
            } else {
                // Already at the bottom-most desktop
                return id;
            }
        }
        const uint desktop = m_grid.at(coords);
        if (desktop > 0) {
            return desktop;
        }
    }
}

uint VirtualDesktopManager::toLeft(uint id, bool wrap) const
{
    if (id == 0) {
        id = current();
    }
    QPoint coords = m_grid.gridCoords(id);
    Q_ASSERT(coords.x() >= 0);
    while (true) {
        coords.rx()--;
        if (coords.x() < 0) {
            if (wrap) {
                coords.setX(m_grid.width() - 1);
            } else {
                return id; // Already at the left-most desktop
            }
        }
        const uint desktop = m_grid.at(coords);
        if (desktop > 0) {
            return desktop;
        }
    }
}

uint VirtualDesktopManager::next(uint id, bool wrap) const
{
    if (id == 0) {
        id = current();
    }
    const uint desktop = id + 1;
    if (desktop > count()) {
        if (wrap) {
            return 1;
        } else {
            // are at the last desktop, without wrap return current
            return id;
        }
    }
    return desktop;
}

uint VirtualDesktopManager::previous(uint id, bool wrap) const
{
    if (id == 0) {
        id = current();
    }
    const uint desktop = id - 1;
    if (desktop == 0) {
        if (wrap) {
            return count();
        } else {
            // are at the first desktop, without wrap return current
            return id;
        }
    }
    return desktop;
}

uint VirtualDesktopManager::current() const
{
    return m_current ? m_current->x11DesktopNumber() : 0;
}

bool VirtualDesktopManager::setCurrent(uint newDesktop)
{
    if (newDesktop < 1 || newDesktop > count() || newDesktop == current()) {
        return false;
    }
    auto it = std::find_if(m_desktops.constBegin(), m_desktops.constEnd(),
        [newDesktop] (VirtualDesktop *d) {
            return d->x11DesktopNumber() == newDesktop;
        }
    );
    Q_ASSERT(it != m_desktops.constEnd());
    const uint oldDesktop = current();

    // change the desktop
    m_current = *it;
    emit currentChanged(oldDesktop, newDesktop);
    return true;
}

void VirtualDesktopManager::setCount(uint count)
{
    count = qBound<uint>(1, count, VirtualDesktopManager::maximum());
    if (count == uint(m_desktops.count())) {
        // nothing to change
        return;
    }
    const uint oldCount = m_desktops.count();
    const uint oldCurrent = current();
    while (uint(m_desktops.count()) > count) {
        delete m_desktops.takeLast();
    }
    while (uint(m_desktops.count()) < count) {
        auto vd = new VirtualDesktop(this);
        vd->setX11DesktopNumber(m_desktops.count() + 1);
        m_desktops << vd;
    }
    if (oldCount > count) {
        handleDesktopsRemoved(oldCount, oldCurrent);
    }

    updateRootInfo();

    save();
    emit countChanged(oldCount, m_desktops.count());
}

void VirtualDesktopManager::handleDesktopsRemoved(uint previousCount, uint previousCurrent)
{
    if (!m_current) {
        m_current = m_desktops.last();
        emit currentChanged(previousCurrent, m_current->x11DesktopNumber());
    }
    emit desktopsRemoved(previousCount);
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
    int width = 0;
    int height = 0;
    Qt::Orientation orientation = Qt::Horizontal;
    if (m_rootInfo) {
        // TODO: Is there a sane way to avoid overriding the existing grid?
        width = m_rootInfo->desktopLayoutColumnsRows().width();
        height = m_rootInfo->desktopLayoutColumnsRows().height();
        orientation = m_rootInfo->desktopLayoutOrientation() == NET::OrientationHorizontal ? Qt::Horizontal : Qt::Vertical;
    }
    if (width == 0 && height == 0) {
        // Not given, set default layout
        height = count() == 1u ? 1 : 2;
    }
    setNETDesktopLayout(orientation,
        width, height, 0 //rootInfo->desktopLayoutCorner() // Not really worth implementing right now.
    );
}

static bool s_loadingDesktopSettings = false;

void VirtualDesktopManager::load()
{
    s_loadingDesktopSettings = true;
    if (!m_config) {
        return;
    }
    QString groupname;
    if (screen_number == 0) {
        groupname = QStringLiteral("Desktops");
    } else {
        groupname.sprintf("Desktops-screen-%d", screen_number);
    }
    KConfigGroup group(m_config, groupname);
    const int n = group.readEntry("Number", 1);
    setCount(n);
    if (m_rootInfo) {
        for (int i = 1; i <= n; i++) {
            QString s = group.readEntry(QStringLiteral("Name_%1").arg(i), i18n("Desktop %1", i));
            m_rootInfo->setDesktopName(i, s.toUtf8().data());
            // TODO: update desktop focus chain, why?
//         m_desktopFocusChain.value()[i-1] = i;
        }

        int rows = group.readEntry<int>("Rows", 2);
        rows = qBound(1, rows, n);
        // avoid weird cases like having 3 rows for 4 desktops, where the last row is unused
        int columns = n / rows;
        if (n % rows > 0) {
            columns++;
        }
        m_rootInfo->setDesktopLayout(NET::OrientationHorizontal, columns, rows, NET::DesktopLayoutCornerTopLeft);
        m_rootInfo->activate();
    }
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
    QString groupname;
    if (screen_number == 0) {
        groupname = QStringLiteral("Desktops");
    } else {
        groupname.sprintf("Desktops-screen-%d", screen_number);
    }
    KConfigGroup group(m_config, groupname);

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
    }

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

    m_grid.update(QSize(width, height), orientation);
    // TODO: why is there no call to m_rootInfo->setDesktopLayout?
    emit layoutChanged(width, height);
}

void VirtualDesktopManager::initShortcuts()
{
    initSwitchToShortcuts();

    addAction(QStringLiteral("Switch to Next Desktop"), i18n("Switch to Next Desktop"), &VirtualDesktopManager::slotNext);
    addAction(QStringLiteral("Switch to Previous Desktop"), i18n("Switch to Previous Desktop"), &VirtualDesktopManager::slotPrevious);
    addAction(QStringLiteral("Switch One Desktop to the Right"), i18n("Switch One Desktop to the Right"), &VirtualDesktopManager::slotRight);
    addAction(QStringLiteral("Switch One Desktop to the Left"), i18n("Switch One Desktop to the Left"), &VirtualDesktopManager::slotLeft);
    addAction(QStringLiteral("Switch One Desktop Up"), i18n("Switch One Desktop Up"), &VirtualDesktopManager::slotUp);
    addAction(QStringLiteral("Switch One Desktop Down"), i18n("Switch One Desktop Down"), &VirtualDesktopManager::slotDown);

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

void VirtualDesktopManager::addAction(const QString &name, const KLocalizedString &label, uint value, const QKeySequence &key, void (VirtualDesktopManager::*slot)())
{
    QAction *a = new QAction(this);
    a->setProperty("componentName", QStringLiteral(KWIN_NAME));
    a->setObjectName(name.arg(value));
    a->setText(label.subs(value).toString());
    a->setData(value);
    KGlobalAccel::setGlobalShortcut(a, key);
    input()->registerShortcut(key, a, this, slot);
}

void VirtualDesktopManager::addAction(const QString &name, const QString &label, void (VirtualDesktopManager::*slot)())
{
    QAction *a = new QAction(this);
    a->setProperty("componentName", QStringLiteral(KWIN_NAME));
    a->setObjectName(name);
    a->setText(label);
    KGlobalAccel::setGlobalShortcut(a, QKeySequence());
    input()->registerShortcut(QKeySequence(), a, this, slot);
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
