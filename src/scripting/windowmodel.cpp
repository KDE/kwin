/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowmodel.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"

namespace KWin
{

WindowModel::WindowModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(workspace(), &Workspace::windowAdded, this, &WindowModel::handleWindowAdded);
    connect(workspace(), &Workspace::windowRemoved, this, &WindowModel::handleWindowRemoved);

    m_windows = workspace()->windows();
    for (Window *window : std::as_const(m_windows)) {
        setupWindowConnections(window);
    }
}

void WindowModel::markRoleChanged(Window *window, int role)
{
    const QModelIndex row = index(m_windows.indexOf(window), 0);
    Q_EMIT dataChanged(row, row, {role});
}

void WindowModel::setupWindowConnections(Window *window)
{
    connect(window, &Window::desktopsChanged, this, [this, window]() {
        markRoleChanged(window, DesktopRole);
    });
    connect(window, &Window::outputChanged, this, [this, window]() {
        markRoleChanged(window, OutputRole);
    });
    connect(window, &Window::activitiesChanged, this, [this, window]() {
        markRoleChanged(window, ActivityRole);
    });
}

void WindowModel::handleWindowAdded(Window *window)
{
    beginInsertRows(QModelIndex(), m_windows.count(), m_windows.count());
    m_windows.append(window);
    endInsertRows();

    setupWindowConnections(window);
}

void WindowModel::handleWindowRemoved(Window *window)
{
    const int index = m_windows.indexOf(window);
    Q_ASSERT(index != -1);

    beginRemoveRows(QModelIndex(), index, index);
    m_windows.removeAt(index);
    endRemoveRows();
}

QHash<int, QByteArray> WindowModel::roleNames() const
{
    return {
        {Qt::DisplayRole, QByteArrayLiteral("display")},
        {WindowRole, QByteArrayLiteral("window")},
        {OutputRole, QByteArrayLiteral("output")},
        {DesktopRole, QByteArrayLiteral("desktop")},
        {ActivityRole, QByteArrayLiteral("activity")},
    };
}

QVariant WindowModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_windows.count()) {
        return QVariant();
    }

    Window *window = m_windows[index.row()];
    switch (role) {
    case Qt::DisplayRole:
    case WindowRole:
        return QVariant::fromValue(window);
    case OutputRole:
        return QVariant::fromValue(window->output());
    case DesktopRole:
        return QVariant::fromValue(window->desktops());
    case ActivityRole:
        return window->activities();
    default:
        return QVariant();
    }
}

int WindowModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_windows.count();
}

WindowFilterModel::WindowFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

WindowModel *WindowFilterModel::windowModel() const
{
    return m_windowModel;
}

void WindowFilterModel::setWindowModel(WindowModel *windowModel)
{
    if (windowModel == m_windowModel) {
        return;
    }
    m_windowModel = windowModel;
    setSourceModel(m_windowModel);
    Q_EMIT windowModelChanged();
}

QString WindowFilterModel::activity() const
{
    return m_activity.value_or(QString());
}

void WindowFilterModel::setActivity(const QString &activity)
{
    if (m_activity != activity) {
        m_activity = activity;
        Q_EMIT activityChanged();
        invalidateFilter();
    }
}

void WindowFilterModel::resetActivity()
{
    if (m_activity.has_value()) {
        m_activity.reset();
        Q_EMIT activityChanged();
        invalidateFilter();
    }
}

VirtualDesktop *WindowFilterModel::desktop() const
{
    return m_desktop;
}

void WindowFilterModel::setDesktop(VirtualDesktop *desktop)
{
    if (m_desktop != desktop) {
        m_desktop = desktop;
        Q_EMIT desktopChanged();
        invalidateFilter();
    }
}

void WindowFilterModel::resetDesktop()
{
    setDesktop(nullptr);
}

QString WindowFilterModel::filter() const
{
    return m_filter;
}

void WindowFilterModel::setFilter(const QString &filter)
{
    if (filter == m_filter) {
        return;
    }
    m_filter = filter;
    Q_EMIT filterChanged();
    invalidateFilter();
}

QString WindowFilterModel::screenName() const
{
    return m_output ? m_output->name() : QString();
}

void WindowFilterModel::setScreenName(const QString &screen)
{
    Output *output = kwinApp()->outputBackend()->findOutput(screen);
    if (m_output != output) {
        m_output = output;
        Q_EMIT screenNameChanged();
        invalidateFilter();
    }
}

void WindowFilterModel::resetScreenName()
{
    if (m_output) {
        m_output = nullptr;
        Q_EMIT screenNameChanged();
        invalidateFilter();
    }
}

WindowFilterModel::WindowTypes WindowFilterModel::windowType() const
{
    return m_windowType.value_or(WindowTypes());
}

void WindowFilterModel::setWindowType(WindowTypes windowType)
{
    if (m_windowType != windowType) {
        m_windowType = windowType;
        Q_EMIT windowTypeChanged();
        invalidateFilter();
    }
}

void WindowFilterModel::resetWindowType()
{
    if (m_windowType.has_value()) {
        m_windowType.reset();
        Q_EMIT windowTypeChanged();
        invalidateFilter();
    }
}

void WindowFilterModel::setMinimizedWindows(bool show)
{
    if (m_showMinimizedWindows == show) {
        return;
    }

    m_showMinimizedWindows = show;
    invalidateFilter();
    Q_EMIT minimizedWindowsChanged();
}

bool WindowFilterModel::minimizedWindows() const
{
    return m_showMinimizedWindows;
}

bool WindowFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (!m_windowModel) {
        return false;
    }
    const QModelIndex index = m_windowModel->index(sourceRow, 0, sourceParent);
    if (!index.isValid()) {
        return false;
    }
    const QVariant data = index.data();
    if (!data.isValid()) {
        // an invalid QVariant is valid data
        return true;
    }

    Window *window = qvariant_cast<Window *>(data);
    if (!window || !window->isClient()) {
        return false;
    }

    if (m_activity.has_value()) {
        if (!window->isOnActivity(*m_activity)) {
            return false;
        }
    }

    if (m_desktop) {
        if (!window->isOnDesktop(m_desktop)) {
            return false;
        }
    }

    if (m_output) {
        if (window->output() != m_output) {
            return false;
        }
    }

    if (m_windowType.has_value()) {
        if (!(windowTypeMask(window) & *m_windowType)) {
            return false;
        }
    }

    if (!m_filter.isEmpty()) {
        if (window->caption().contains(m_filter, Qt::CaseInsensitive)) {
            return true;
        }
        if (window->windowRole().contains(m_filter, Qt::CaseInsensitive)) {
            return true;
        }
        if (window->resourceName().contains(m_filter, Qt::CaseInsensitive)) {
            return true;
        }
        if (window->resourceClass().contains(m_filter, Qt::CaseInsensitive)) {
            return true;
        }
        return false;
    }

    if (!m_showMinimizedWindows) {
        return !window->isMinimized();
    }
    return true;
}

WindowFilterModel::WindowTypes WindowFilterModel::windowTypeMask(Window *window) const
{
    WindowTypes mask;
    if (window->isNormalWindow()) {
        mask |= WindowType::Normal;
    } else if (window->isDialog()) {
        mask |= WindowType::Dialog;
    } else if (window->isDock()) {
        mask |= WindowType::Dock;
    } else if (window->isDesktop()) {
        mask |= WindowType::Desktop;
    } else if (window->isNotification()) {
        mask |= WindowType::Notification;
    } else if (window->isCriticalNotification()) {
        mask |= WindowType::CriticalNotification;
    }
    return mask;
}

} // namespace KWin

#include "moc_windowmodel.cpp"
