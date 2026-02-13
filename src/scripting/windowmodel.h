/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "virtualdesktops.h"

#include <QAbstractListModel>
#include <QPointer>
#include <QSortFilterProxyModel>

#include <optional>

namespace KWin
{
class Window;
class LogicalOutput;

/*!
 * \qmltype WindowModel
 * \inqmlmodule org.kde.kwin
 * \brief Window Model.
 *
 * This model provides the following roles:
 * \list
 * \li display (string)
 * \li window (Window)
 * \li output (LogicalOutput)
 * \li desktop (list<VirtualDesktop>)
 * \li activity (list<string>)
 * \endlist
 *
 * \sa WindowFilterModel
 */
class WindowModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        WindowRole = Qt::UserRole + 1,
        OutputRole,
        DesktopRole,
        ActivityRole
    };

    explicit WindowModel(QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

private:
    void markRoleChanged(Window *window, int role);

    void handleWindowAdded(Window *window);
    void handleWindowRemoved(Window *window);
    void setupWindowConnections(Window *window);

    QList<Window *> m_windows;
};

/*!
 * \qmltype WindowFilterModel
 * \inqmlmodule org.kde.kwin
 *
 * \brief Filter a WindowModel
 */
class WindowFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT

    /*!
     * \qmlproperty WindowModel WindowFilterModel::windowModel
     */
    Q_PROPERTY(WindowModel *windowModel READ windowModel WRITE setWindowModel NOTIFY windowModelChanged)

    /*!
     * \qmlproperty string WindowFilterModel::activity
     */
    Q_PROPERTY(QString activity READ activity WRITE setActivity RESET resetActivity NOTIFY activityChanged)

    /*!
     * \qmlproperty VirtualDesktop WindowFilterModel::desktop
     */
    Q_PROPERTY(KWin::VirtualDesktop *desktop READ desktop WRITE setDesktop RESET resetDesktop NOTIFY desktopChanged)

    /*!
     * \qmlproperty string WindowFilterModel::filter
     */
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)

    /*!
     * \qmlproperty string WindowFilterModel::screenName
     */
    Q_PROPERTY(QString screenName READ screenName WRITE setScreenName RESET resetScreenName NOTIFY screenNameChanged)

    /*!
     * \qmlproperty enumeration WindowFilterModel::windowType
     * \qmlenumeratorsfrom KWin::WindowTypes
     */
    Q_PROPERTY(WindowTypes windowType READ windowType WRITE setWindowType RESET resetWindowType NOTIFY windowTypeChanged)

    /*!
     * \qmlproperty bool WindowFilterModel::minimizedWindows
     */
    Q_PROPERTY(bool minimizedWindows READ minimizedWindows WRITE setMinimizedWindows NOTIFY minimizedWindowsChanged)

public:
    /*!
     * \value Normal
     * \value Dialog
     * \value Dock
     * \value Desktop
     * \value Notification
     * \value CriticalNotification
     */
    enum WindowType {
        Normal = 0x1,
        Dialog = 0x2,
        Dock = 0x4,
        Desktop = 0x8,
        Notification = 0x10,
        CriticalNotification = 0x20,
    };
    Q_DECLARE_FLAGS(WindowTypes, WindowType)
    Q_FLAG(WindowTypes)

    explicit WindowFilterModel(QObject *parent = nullptr);

    WindowModel *windowModel() const;
    void setWindowModel(WindowModel *windowModel);

    QString activity() const;
    void setActivity(const QString &activity);
    void resetActivity();

    VirtualDesktop *desktop() const;
    void setDesktop(VirtualDesktop *desktop);
    void resetDesktop();

    QString filter() const;
    void setFilter(const QString &filter);

    QString screenName() const;
    void setScreenName(const QString &screenName);
    void resetScreenName();

    WindowTypes windowType() const;
    void setWindowType(WindowTypes windowType);
    void resetWindowType();

    void setMinimizedWindows(bool show);
    bool minimizedWindows() const;

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

Q_SIGNALS:
    void activityChanged();
    void desktopChanged();
    void screenNameChanged();
    void windowModelChanged();
    void filterChanged();
    void windowTypeChanged();
    void minimizedWindowsChanged();

private:
    WindowTypes windowTypeMask(Window *window) const;

    WindowModel *m_windowModel = nullptr;
    std::optional<QString> m_activity;
    QPointer<LogicalOutput> m_output;
    QPointer<VirtualDesktop> m_desktop;
    QString m_filter;
    std::optional<WindowTypes> m_windowType;
    bool m_showMinimizedWindows = true;
};

} // namespace KWin
