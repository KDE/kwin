/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_DBUS_INTERFACE_H
#define KWIN_DBUS_INTERFACE_H

#include <QObject>
#include <QtDBus>

#include "virtualdesktopsdbustypes.h"

namespace KWin
{

class Compositor;
class VirtualDesktopManager;

/**
 * @brief This class is a wrapper for the org.kde.KWin D-Bus interface.
 *
 * The main purpose of this class is to be exported on the D-Bus as object /KWin.
 * It is a pure wrapper to provide the deprecated D-Bus methods which have been
 * removed from Workspace which used to implement the complete D-Bus interface.
 *
 * Nowadays the D-Bus interfaces are distributed, parts of it are exported on
 * /Compositor, parts on /Effects and parts on /KWin. The implementation in this
 * class just delegates the method calls to the actual implementation in one of the
 * three singletons.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
 */
class DBusInterface: public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin")
public:
    explicit DBusInterface(QObject *parent);
    ~DBusInterface() override;

public: // PROPERTIES
public Q_SLOTS: // METHODS
    Q_NOREPLY void cascadeDesktop();
    int currentDesktop();
    Q_NOREPLY void killWindow();
    void nextDesktop();
    void previousDesktop();
    Q_NOREPLY void reconfigure();
    bool setCurrentDesktop(int desktop);
    bool startActivity(const QString &in0);
    bool stopActivity(const QString &in0);
    QString supportInformation();
    Q_NOREPLY void unclutterDesktop();
    Q_NOREPLY void showDebugConsole();

    QVariantMap queryWindowInfo();
    QVariantMap getWindowInfo(const QString &uuid);

private Q_SLOTS:
    void becomeKWinService(const QString &service);

private:
    void announceService();
    QString m_serviceName;
    QDBusMessage m_replyQueryWindowInfo;
};

class CompositorDBusInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.Compositing")
    /**
     * @brief Whether the Compositor is active. That is a Scene is present and the Compositor is
     * not shutting down itself.
     */
    Q_PROPERTY(bool active READ isActive)
    /**
     * @brief Whether compositing is possible. Mostly means whether the required X extensions
     * are available.
     */
    Q_PROPERTY(bool compositingPossible READ isCompositingPossible)
    /**
     * @brief The reason why compositing is not possible. Empty String if compositing is possible.
     */
    Q_PROPERTY(QString compositingNotPossibleReason READ compositingNotPossibleReason)
    /**
     * @brief Whether OpenGL has failed badly in the past (crash) and is considered as broken.
     */
    Q_PROPERTY(bool openGLIsBroken READ isOpenGLBroken)
    /**
     * The type of the currently used Scene:
     * @li @c none No Compositing
     * @li @c xrender XRender
     * @li @c gl1 OpenGL 1
     * @li @c gl2 OpenGL 2
     * @li @c gles OpenGL ES 2
     */
    Q_PROPERTY(QString compositingType READ compositingType)
    /**
     * @brief All currently supported OpenGLPlatformInterfaces.
     *
     * Possible values:
     * @li glx
     * @li egl
     *
     * Values depend on operation mode and compile time options.
     */
    Q_PROPERTY(QStringList supportedOpenGLPlatformInterfaces READ supportedOpenGLPlatformInterfaces)
    Q_PROPERTY(bool platformRequiresCompositing READ platformRequiresCompositing)
public:
    explicit CompositorDBusInterface(Compositor *parent);
    ~CompositorDBusInterface() override = default;

    bool isActive() const;
    bool isCompositingPossible() const;
    QString compositingNotPossibleReason() const;
    bool isOpenGLBroken() const;
    QString compositingType() const;
    QStringList supportedOpenGLPlatformInterfaces() const;
    bool platformRequiresCompositing() const;

public Q_SLOTS:
    /**
     * @brief Suspends the Compositor if it is currently active.
     *
     * Note: it is possible that the Compositor is not able to suspend. Use isActive to check
     * whether the Compositor has been suspended.
     *
     * @return void
     * @see resume
     * @see isActive
     */
    void suspend();
    /**
     * @brief Resumes the Compositor if it is currently suspended.
     *
     * Note: it is possible that the Compositor cannot be resumed, that is there might be Clients
     * blocking the usage of Compositing or the Scene might be broken. Use isActive to check
     * whether the Compositor has been resumed. Also check isCompositingPossible and
     * isOpenGLBroken.
     *
     * Note: The starting of the Compositor can require some time and is partially done threaded.
     * After this method returns the setup may not have been completed.
     *
     * @return void
     * @see suspend
     * @see isActive
     * @see isCompositingPossible
     * @see isOpenGLBroken
     */
    void resume();
    /**
     * @brief Used by Compositing KCM after settings change.
     *
     * On signal Compositor reloads settings and restarts.
     */
    void reinitialize();

Q_SIGNALS:
    void compositingToggled(bool active);

private:
    Compositor *m_compositor;
};

//TODO: disable all of this in case of kiosk?

class VirtualDesktopManagerDBusInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.VirtualDesktopManager")

    /**
     * The number of virtual desktops currently available.
     * The ids of the virtual desktops are in the range [1, VirtualDesktopManager::maximum()].
     */
    Q_PROPERTY(uint count READ count NOTIFY countChanged)
    /**
     * The number of rows the virtual desktops will be laid out in
     */
    Q_PROPERTY(uint rows READ rows WRITE setRows NOTIFY rowsChanged)
    /**
     * The id of the virtual desktop which is currently in use.
     */
    Q_PROPERTY(QString current READ current WRITE setCurrent NOTIFY currentChanged)
    /**
     * Whether navigation in the desktop layout wraps around at the borders.
     */
    Q_PROPERTY(bool navigationWrappingAround READ isNavigationWrappingAround WRITE setNavigationWrappingAround NOTIFY navigationWrappingAroundChanged)

    /**
     * list of key/value pairs which every one of them is representing a desktop
     */
    Q_PROPERTY(KWin::DBusDesktopDataVector desktops READ desktops NOTIFY desktopsChanged);

public:
    VirtualDesktopManagerDBusInterface(VirtualDesktopManager *parent);
    ~VirtualDesktopManagerDBusInterface() override = default;

    uint count() const;

    void setRows(uint rows);
    uint rows() const;

    void setCurrent(const QString &id);
    QString current() const;

    void setNavigationWrappingAround(bool wraps);
    bool isNavigationWrappingAround() const;

    KWin::DBusDesktopDataVector desktops() const;

Q_SIGNALS:
    void countChanged(uint count);
    void rowsChanged(uint rows);
    void currentChanged(const QString &id);
    void navigationWrappingAroundChanged(bool wraps);
    void desktopsChanged(KWin::DBusDesktopDataVector);
    void desktopDataChanged(const QString &id, KWin::DBusDesktopDataStruct);
    void desktopCreated(const QString &id, KWin::DBusDesktopDataStruct);
    void desktopRemoved(const QString &id);

public Q_SLOTS:
    /**
     * Create a desktop with a new name at a given position
     * note: the position starts from 1
     */
    void createDesktop(uint position, const QString &name);
    void setDesktopName(const QString &id, const QString &name);
    void removeDesktop(const QString &id);

private:
    VirtualDesktopManager *m_manager;
};

} // namespace

#endif // KWIN_DBUS_INTERFACE_H
