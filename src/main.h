/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef MAIN_H
#define MAIN_H

#include <config-kwin.h>
#include <kwinglobals.h>

#include <KSharedConfig>
#include <memory>
// Qt
#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QProcessEnvironment>

class KPluginMetaData;
class QCommandLineParser;

namespace KWin
{

class Platform;
class Session;
class X11EventFilter;
class PluginManager;
class InputMethod;
class ColorManager;
class ScreenLockerWatcher;

class XcbEventFilter : public QAbstractNativeEventFilter
{
public:
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray &eventType, void *message, long int *result) override;
#else
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
#endif
};

class X11EventFilterContainer : public QObject
{
    Q_OBJECT

public:
    explicit X11EventFilterContainer(X11EventFilter *filter);

    X11EventFilter *filter() const;

private:
    X11EventFilter *m_filter;
};

class KWIN_EXPORT Application : public QApplication
{
    Q_OBJECT
    Q_PROPERTY(quint32 x11Time READ x11Time WRITE setX11Time)
    Q_PROPERTY(quint32 x11RootWindow READ x11RootWindow CONSTANT)
    Q_PROPERTY(void *x11Connection READ x11Connection NOTIFY x11ConnectionChanged)
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
    Q_ENUM(OperationMode)
    ~Application() override;

    void setConfigLock(bool lock);

    KSharedConfigPtr config() const
    {
        return m_config;
    }
    void setConfig(KSharedConfigPtr config)
    {
        m_config = std::move(config);
    }

    KSharedConfigPtr kxkbConfig() const
    {
        return m_kxkbConfig;
    }
    void setKxkbConfig(KSharedConfigPtr config)
    {
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

    void registerEventFilter(X11EventFilter *filter);
    void unregisterEventFilter(X11EventFilter *filter);
    bool dispatchEvent(xcb_generic_event_t *event);

    xcb_timestamp_t x11Time() const
    {
        return m_x11Time;
    }
    enum class TimestampUpdate {
        OnlyIfLarger,
        Always
    };
    void setX11Time(xcb_timestamp_t timestamp, TimestampUpdate force = TimestampUpdate::OnlyIfLarger)
    {
        if ((timestamp > m_x11Time || force == TimestampUpdate::Always) && timestamp != 0) {
            m_x11Time = timestamp;
        }
    }
    void updateX11Time(xcb_generic_event_t *event);

    static void setCrashCount(int count);
    static bool wasCrash();
    void resetCrashesCount();

    /**
     * Creates the KAboutData object for the KWin instance and registers it as
     * KAboutData::setApplicationData.
     */
    static void createAboutData();

    /**
     * @returns the X11 root window.
     */
    xcb_window_t x11RootWindow() const
    {
        return m_rootWindow;
    }

    /**
     * @returns the X11 xcb connection
     */
    xcb_connection_t *x11Connection() const
    {
        return m_connection;
    }

    /**
     * Inheriting classes should use this method to set the X11 root window
     * before accessing any X11 specific code pathes.
     */
    void setX11RootWindow(xcb_window_t root)
    {
        m_rootWindow = root;
    }
    /**
     * Inheriting classes should use this method to set the xcb connection
     * before accessing any X11 specific code pathes.
     */
    void setX11Connection(xcb_connection_t *c)
    {
        m_connection = c;
    }

    qreal xwaylandScale() const
    {
        return m_xwaylandScale;
    }

    void setXwaylandScale(qreal scale);

#if KWIN_BUILD_ACTIVITIES
    bool usesKActivities() const
    {
        return m_useKActivities;
    }
    void setUseKActivities(bool use)
    {
        m_useKActivities = use;
    }
#endif

    QProcessEnvironment processStartupEnvironment() const;
    void setProcessStartupEnvironment(const QProcessEnvironment &environment);

    Platform *platform() const
    {
        return m_platform.get();
    }
    void setPlatform(std::unique_ptr<Platform> &&platform);

    Session *session() const
    {
        return m_session.get();
    }
    void setSession(std::unique_ptr<Session> &&session);

    bool isTerminating() const
    {
        return m_terminating;
    }

    void installNativeX11EventFilter();
    void removeNativeX11EventFilter();

    void createAtoms();
    void destroyAtoms();

    static void setupMalloc();
    static void setupLocalizedString();

    PluginManager *pluginManager() const;
    InputMethod *inputMethod() const;
    ColorManager *colorManager() const;
#if KWIN_BUILD_SCREENLOCKER
    ScreenLockerWatcher *screenLockerWatcher() const;
#endif

Q_SIGNALS:
    void x11ConnectionChanged();
    void x11ConnectionAboutToBeDestroyed();
    void xwaylandScaleChanged();
    void workspaceCreated();
    void virtualTerminalCreated();
    void started();

protected:
    Application(OperationMode mode, int &argc, char **argv);
    virtual void performStartup() = 0;

    void notifyKSplash();
    void notifyStarted();
    void createInput();
    void createWorkspace();
    void createOptions();
    void createPlugins();
    void createColorManager();
    void createInputMethod();
    void destroyInput();
    void destroyWorkspace();
    void destroyCompositor();
    void destroyPlugins();
    void destroyColorManager();
    void destroyInputMethod();
    void destroyPlatform();

    void setTerminating()
    {
        m_terminating = true;
    }

protected:
    static int crashes;

private:
    QList<QPointer<X11EventFilterContainer>> m_eventFilters;
    QList<QPointer<X11EventFilterContainer>> m_genericEventFilters;
    std::unique_ptr<XcbEventFilter> m_eventFilter;
    bool m_configLock;
    KSharedConfigPtr m_config;
    KSharedConfigPtr m_kxkbConfig;
    OperationMode m_operationMode;
    xcb_timestamp_t m_x11Time = XCB_TIME_CURRENT_TIME;
    xcb_window_t m_rootWindow = XCB_WINDOW_NONE;
    xcb_connection_t *m_connection = nullptr;
#if KWIN_BUILD_ACTIVITIES
    bool m_useKActivities = true;
#endif
    std::unique_ptr<Session> m_session;
    std::unique_ptr<Platform> m_platform;
    bool m_terminating = false;
    qreal m_xwaylandScale = 1;
    QProcessEnvironment m_processEnvironment;
    std::unique_ptr<PluginManager> m_pluginManager;
    std::unique_ptr<InputMethod> m_inputMethod;
    std::unique_ptr<ColorManager> m_colorManager;
#if KWIN_BUILD_SCREENLOCKER
    std::unique_ptr<ScreenLockerWatcher> m_screenLockerWatcher;
#endif
};

inline static Application *kwinApp()
{
    return static_cast<Application *>(QCoreApplication::instance());
}

} // namespace

#endif
