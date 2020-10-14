/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
        m_config = std::move(config);
    }

    KSharedConfigPtr kxkbConfig() const {
        return m_kxkbConfig;
    }
    void setKxkbConfig(KSharedConfigPtr config) {
        m_kxkbConfig = std::move(config);
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

    /**
     * @returns the X11 default screen
     */
    xcb_screen_t *x11DefaultScreen() const {
        return m_defaultScreen;
    }

    /**
     * Returns @c true if we're in the middle of destroying the X11 connection.
     */
    bool isClosingX11Connection() const {
        return m_isClosingX11Connection;
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
    void started();

protected:
    Application(OperationMode mode, int &argc, char **argv);
    virtual void performStartup() = 0;

    void notifyKSplash();
    void notifyStarted();
    void createInput();
    void createWorkspace();
    void createAtoms();
    void createOptions();
    void installNativeX11EventFilter();
    void removeNativeX11EventFilter();
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
    }
    /**
     * Inheriting classes should use this method to set the default screen
     * before accessing any X11 specific code pathes.
     */
    void setX11DefaultScreen(xcb_screen_t *screen) {
        m_defaultScreen = screen;
    }
    void destroyAtoms();
    void destroyPlatform();

    void setTerminating() {
        m_terminating = true;
    }
    void setClosingX11Connection(bool set) {
        m_isClosingX11Connection = set;
    }

protected:
    static int crashes;

private Q_SLOTS:
    void resetCrashesCount();

private:
    QScopedPointer<XcbEventFilter> m_eventFilter;
    bool m_configLock;
    KSharedConfigPtr m_config;
    KSharedConfigPtr m_kxkbConfig;
    OperationMode m_operationMode;
    xcb_timestamp_t m_x11Time = XCB_TIME_CURRENT_TIME;
    xcb_window_t m_rootWindow = XCB_WINDOW_NONE;
    xcb_connection_t *m_connection = nullptr;
    xcb_screen_t *m_defaultScreen = nullptr;
#ifdef KWIN_BUILD_ACTIVITIES
    bool m_useKActivities = true;
#endif
    Platform *m_platform = nullptr;
    bool m_terminating = false;
    bool m_isClosingX11Connection = false;
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
