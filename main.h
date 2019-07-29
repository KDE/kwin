/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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

#ifndef MAIN_H
#define MAIN_H

#include <kwinglobals.h>
#include <config-kwin.h>

#include <KSharedConfig>
// Qt
#include <QApplication>
#include <QAbstractNativeEventFilter>
#include <QProcessEnvironment>

class KPluginMetaData;
class QCommandLineParser;

namespace KWin
{

class Platform;

class XcbEventFilter : public QAbstractNativeEventFilter
{
public:
    bool nativeEventFilter(const QByteArray &eventType, void *message, long int *result) override;
};

class KWIN_EXPORT Application : public  QApplication
{
    Q_OBJECT
    Q_PROPERTY(quint32 x11Time READ x11Time WRITE setX11Time)
    Q_PROPERTY(quint32 x11RootWindow READ x11RootWindow CONSTANT)
    Q_PROPERTY(void *x11Connection READ x11Connection NOTIFY x11ConnectionChanged)
    Q_PROPERTY(int x11ScreenNumber READ x11ScreenNumber CONSTANT)
    Q_PROPERTY(KSharedConfigPtr config READ config WRITE setConfig)
    Q_PROPERTY(KSharedConfigPtr kxkbConfig READ kxkbConfig WRITE setKxkbConfig)
    Q_PROPERTY(KSharedConfigPtr inputConfig READ inputConfig WRITE setInputConfig)
public:
    /**
     * @brief This enum provides the various operation modes of KWin depending on the available
     * Windowing Systems at startup. For example whether KWin only talks to X11 or also to a Wayland
     * Compositor.
     *
     */
    enum OperationMode {
        /**
         * @brief KWin uses only X11 for managing windows and compositing
         */
        OperationModeX11,
        /**
         * @brief KWin uses only Wayland
         */
        OperationModeWaylandOnly,
        /**
         * @brief KWin uses Wayland and controls a nested Xwayland server.
         */
        OperationModeXwayland
    };
    ~Application() override;

    void setConfigLock(bool lock);

    KSharedConfigPtr config() const {
        return m_config;
    }
    void setConfig(KSharedConfigPtr config) {
        m_config = config;
    }

    KSharedConfigPtr kxkbConfig() const {
        return m_kxkbConfig;
    }
    void setKxkbConfig(KSharedConfigPtr config) {
        m_kxkbConfig = config;
    }

    KSharedConfigPtr inputConfig() const {
        return m_inputConfig;
    }
    void setInputConfig(KSharedConfigPtr config) {
        m_inputConfig = config;
    }

    void start();
    /**
     * @brief The operation mode used by KWin.
     *
     * @return OperationMode
     */
    OperationMode operationMode() const;
    void setOperationMode(OperationMode mode);
    bool shouldUseWaylandForCompositing() const;

    void setupTranslator();
    void setupCommandLine(QCommandLineParser *parser);
    void processCommandLine(QCommandLineParser *parser);

    xcb_timestamp_t x11Time() const {
        return m_x11Time;
    }
    enum class TimestampUpdate {
        OnlyIfLarger,
        Always
    };
    void setX11Time(xcb_timestamp_t timestamp, TimestampUpdate force = TimestampUpdate::OnlyIfLarger) {
        if ((timestamp > m_x11Time || force == TimestampUpdate::Always) && timestamp != 0) {
            m_x11Time = timestamp;
        }
    }
    void updateX11Time(xcb_generic_event_t *event);
    void createScreens();

    static void setCrashCount(int count);
    static bool wasCrash();

    /**
     * Creates the KAboutData object for the KWin instance and registers it as
     * KAboutData::setApplicationData.
     */
    static void createAboutData();

    /**
     * @returns the X11 Screen number. If not applicable it's set to @c -1.
     */
    static int x11ScreenNumber();
    /**
     * Sets the X11 screen number of this KWin instance to @p screenNumber.
     */
    static void setX11ScreenNumber(int screenNumber);
    /**
     * @returns whether this is a multi head setup on X11.
     */
    static bool isX11MultiHead();
    /**
     * Sets whether this is a multi head setup on X11.
     */
    static void setX11MultiHead(bool multiHead);

    /**
     * @returns the X11 root window.
     */
    xcb_window_t x11RootWindow() const {
        return m_rootWindow;
    }

    /**
     * @returns the X11 xcb connection
     */
    xcb_connection_t *x11Connection() const {
        return m_connection;
    }

#ifdef KWIN_BUILD_ACTIVITIES
    bool usesKActivities() const {
        return m_useKActivities;
    }
    void setUseKActivities(bool use) {
        m_useKActivities = use;
    }
#endif

    virtual QProcessEnvironment processStartupEnvironment() const;

    void initPlatform(const KPluginMetaData &plugin);
    Platform *platform() const {
        return m_platform;
    }

    bool isTerminating() const {
        return m_terminating;
    }

    static void setupMalloc();
    static void setupLocalizedString();

    static bool usesLibinput();
    static void setUseLibinput(bool use);

Q_SIGNALS:
    void x11ConnectionChanged();
    void x11ConnectionAboutToBeDestroyed();
    void workspaceCreated();
    void screensCreated();
    void virtualTerminalCreated();

protected:
    Application(OperationMode mode, int &argc, char **argv);
    virtual void performStartup() = 0;

    void notifyKSplash();
    void createInput();
    void createWorkspace();
    void createAtoms();
    void createOptions();
    void createCompositor();
    void setupEventFilters();
    void destroyWorkspace();
    void destroyCompositor();
    /**
     * Inheriting classes should use this method to set the X11 root window
     * before accessing any X11 specific code pathes.
     */
    void setX11RootWindow(xcb_window_t root) {
        m_rootWindow = root;
    }
    /**
     * Inheriting classes should use this method to set the xcb connection
     * before accessing any X11 specific code pathes.
     */
    void setX11Connection(xcb_connection_t *c) {
        m_connection = c;
        emit x11ConnectionChanged();
    }
    void destroyAtoms();

    void setTerminating() {
        m_terminating = true;
    }

protected:
    QString m_originalSessionKey;
    static int crashes;

private Q_SLOTS:
    void resetCrashesCount();

private:
    QScopedPointer<XcbEventFilter> m_eventFilter;
    bool m_configLock;
    KSharedConfigPtr m_config;
    KSharedConfigPtr m_kxkbConfig;
    KSharedConfigPtr m_inputConfig;
    OperationMode m_operationMode;
    xcb_timestamp_t m_x11Time = XCB_TIME_CURRENT_TIME;
    xcb_window_t m_rootWindow = XCB_WINDOW_NONE;
    xcb_connection_t *m_connection = nullptr;
#ifdef KWIN_BUILD_ACTIVITIES
    bool m_useKActivities = true;
#endif
    Platform *m_platform = nullptr;
    bool m_terminating = false;
};

inline static Application *kwinApp()
{
    return static_cast<Application*>(QCoreApplication::instance());
}

namespace Xwl
{
class Xwayland;
}

class KWIN_EXPORT ApplicationWaylandAbstract : public Application
{
    Q_OBJECT
public:
    ~ApplicationWaylandAbstract() override = 0;
protected:
    friend class Xwl::Xwayland;

    ApplicationWaylandAbstract(OperationMode mode, int &argc, char **argv);
    virtual void setProcessStartupEnvironment(const QProcessEnvironment &environment) {
        Q_UNUSED(environment);
    }
    virtual void startSession() {}
};


} // namespace

#endif
