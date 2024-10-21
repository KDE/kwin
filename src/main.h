/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kwin.h"

#include "effect/globals.h"

#include <KSharedConfig>
#include <memory>
// Qt
#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QProcessEnvironment>

#if KWIN_BUILD_X11
#include <xcb/xcb.h>
#endif

class KPluginMetaData;
class QCommandLineParser;

namespace KWin
{

class OutputBackend;
class Session;
class X11EventFilter;
class PluginManager;
class InputMethod;
class ColorManager;
class ScreenLockerWatcher;
class TabletModeManager;
class XwaylandInterface;
class Cursor;
class Edge;
class ScreenEdges;
class Outline;
class OutlineVisual;
class Compositor;
class WorkspaceScene;
class Window;

class XcbEventFilter : public QAbstractNativeEventFilter
{
public:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
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
#if KWIN_BUILD_X11
    Q_PROPERTY(quint32 x11Time READ x11Time WRITE setX11Time)
    Q_PROPERTY(quint32 x11RootWindow READ x11RootWindow CONSTANT)
    Q_PROPERTY(void *x11Connection READ x11Connection NOTIFY x11ConnectionChanged)
#endif
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
         * @brief KWin uses Wayland
         */
        OperationModeWayland,
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

    KSharedConfigPtr inputConfig() const
    {
        return m_inputConfig;
    }
    void setInputConfig(KSharedConfigPtr config)
    {
        m_inputConfig = std::move(config);
    }

    void start();
    /**
     * @brief The operation mode used by KWin.
     *
     * @return OperationMode
     */
    OperationMode operationMode() const;
    bool shouldUseWaylandForCompositing() const;

    void setupCommandLine(QCommandLineParser *parser);
    void processCommandLine(QCommandLineParser *parser);

#if KWIN_BUILD_X11
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
    /**
     * Queries the current X11 time stamp of the X server.
     */
    void updateXTime();
    void updateX11Time(xcb_generic_event_t *event);
#endif

    static void setCrashCount(int count);
    static bool wasCrash();
    void resetCrashesCount();

    /**
     * Creates the KAboutData object for the KWin instance and registers it as
     * KAboutData::setApplicationData.
     */
    static void createAboutData();

#if KWIN_BUILD_X11
    /**
     * @returns the X11 root window.
     */
    xcb_window_t x11RootWindow() const
    {
        return m_rootWindow;
    }

    /**
     * @returns the X11 composite overlay window handle.
     */
    xcb_window_t x11CompositeWindow() const
    {
        return m_compositeWindow;
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
    void setX11CompositeWindow(xcb_window_t window)
    {
        m_compositeWindow = window;
    }
#endif

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

    OutputBackend *outputBackend() const
    {
        return m_outputBackend.get();
    }
    void setOutputBackend(std::unique_ptr<OutputBackend> &&backend);

    Session *session() const
    {
        return m_session.get();
    }
    void setSession(std::unique_ptr<Session> &&session);
    void setFollowLocale1(bool follow)
    {
        m_followLocale1 = follow;
    }
    bool followLocale1() const
    {
        return m_followLocale1;
    }

    bool isTerminating() const
    {
        return m_terminating;
    }

    bool initiallyLocked() const;
    void setInitiallyLocked(bool locked);

    bool supportsLockScreen() const;
    void setSupportsLockScreen(bool set);

    bool supportsGlobalShortcuts() const;
    void setSupportsGlobalShortcuts(bool set);

    void installNativeX11EventFilter();
    void removeNativeX11EventFilter();

    void createAtoms();
    void destroyAtoms();

    virtual std::unique_ptr<Edge> createScreenEdge(ScreenEdges *parent);
    virtual std::unique_ptr<Cursor> createPlatformCursor();
    virtual std::unique_ptr<OutlineVisual> createOutline(Outline *outline);
    virtual void createEffectsHandler(Compositor *compositor, WorkspaceScene *scene);

    static void setupMalloc();
    static void setupLocalizedString();

    PluginManager *pluginManager() const;
    InputMethod *inputMethod() const;
    ColorManager *colorManager() const;
    virtual XwaylandInterface *xwayland() const;
#if KWIN_BUILD_SCREENLOCKER
    ScreenLockerWatcher *screenLockerWatcher() const;
#endif
    TabletModeManager *tabletModeManager() const;

    /**
     * Starts an interactive window selection process.
     *
     * Once the user selected a window the @p callback is invoked with the selected Window as
     * argument. In case the user cancels the interactive window selection or selecting a window is currently
     * not possible (e.g. screen locked) the @p callback is invoked with a @c nullptr argument.
     *
     * During the interactive window selection the cursor is turned into a crosshair cursor unless
     * @p cursorName is provided. The argument @p cursorName is a QByteArray instead of Qt::CursorShape
     * to support the "pirate" cursor for kill window which is not wrapped by Qt::CursorShape.
     *
     * The default implementation forwards to InputRedirection.
     *
     * @param callback The function to invoke once the interactive window selection ends
     * @param cursorName The optional name of the cursor shape to use, default is crosshair
     */
    virtual void startInteractiveWindowSelection(std::function<void(KWin::Window *)> callback, const QByteArray &cursorName = QByteArray());

    /**
     * Starts an interactive position selection process.
     *
     * Once the user selected a position on the screen the @p callback is invoked with
     * the selected point as argument. In case the user cancels the interactive position selection
     * or selecting a position is currently not possible (e.g. screen locked) the @p callback
     * is invoked with a point at @c -1 as x and y argument.
     *
     * During the interactive window selection the cursor is turned into a crosshair cursor.
     *
     * The default implementation forwards to InputRedirection.
     *
     * @param callback The function to invoke once the interactive position selection ends
     */
    virtual void startInteractivePositionSelection(std::function<void(const QPointF &)> callback);

    /**
     * Returns a PlatformCursorImage. By default this is created by softwareCursor and
     * softwareCursorHotspot. An implementing subclass can use this to provide a better
     * suited PlatformCursorImage.
     *
     * @see softwareCursor
     * @see softwareCursorHotspot
     * @since 5.9
     */
    virtual PlatformCursorImage cursorImage() const;

Q_SIGNALS:
    void x11ConnectionChanged();
    void x11ConnectionAboutToBeDestroyed();
    void xwaylandScaleChanged();
    void workspaceCreated();
    void virtualTerminalCreated();

protected:
    Application(OperationMode mode, int &argc, char **argv);
    virtual void performStartup() = 0;

    void createInput();
    void createWorkspace();
    void createOptions();
    void createPlugins();
    void createColorManager();
    void createInputMethod();
    void createTabletModeManager();
    void destroyInput();
    void destroyWorkspace();
    void destroyCompositor();
    void destroyPlugins();
    void destroyColorManager();
    void destroyInputMethod();
    void destroyPlatform();
    void applyXwaylandScale();

    void setTerminating()
    {
        m_terminating = true;
    }

protected:
    static int crashes;

private:
#if KWIN_BUILD_X11
    QList<QPointer<X11EventFilterContainer>> m_eventFilters;
    QList<QPointer<X11EventFilterContainer>> m_genericEventFilters;
    std::unique_ptr<XcbEventFilter> m_eventFilter;
#endif
    bool m_followLocale1 = false;
    bool m_configLock;
    bool m_initiallyLocked = false;
    bool m_supportsLockScreen = true;
    bool m_supportsGlobalShortcuts = true;
    KSharedConfigPtr m_config;
    KSharedConfigPtr m_kxkbConfig;
    KSharedConfigPtr m_inputConfig;
    OperationMode m_operationMode;
#if KWIN_BUILD_X11
    xcb_timestamp_t m_x11Time = XCB_TIME_CURRENT_TIME;
    xcb_window_t m_rootWindow = XCB_WINDOW_NONE;
    xcb_window_t m_compositeWindow = XCB_WINDOW_NONE;
    xcb_connection_t *m_connection = nullptr;
#endif
#if KWIN_BUILD_ACTIVITIES
    bool m_useKActivities = true;
#endif
    std::unique_ptr<Session> m_session;
    std::unique_ptr<OutputBackend> m_outputBackend;
    bool m_terminating = false;
    qreal m_xwaylandScale = 1;
    QProcessEnvironment m_processEnvironment;
    std::unique_ptr<PluginManager> m_pluginManager;
    std::unique_ptr<InputMethod> m_inputMethod;
    std::unique_ptr<ColorManager> m_colorManager;
    std::unique_ptr<TabletModeManager> m_tabletModeManager;
#if KWIN_BUILD_SCREENLOCKER
    std::unique_ptr<ScreenLockerWatcher> m_screenLockerWatcher;
#endif
    std::unique_ptr<Cursor> m_platformCursor;
};

inline bool Application::initiallyLocked() const
{
    return m_initiallyLocked;
}

inline void Application::setInitiallyLocked(bool locked)
{
    m_initiallyLocked = locked;
}

inline bool Application::supportsLockScreen() const
{
    return m_supportsLockScreen;
}

inline void Application::setSupportsLockScreen(bool set)
{
    m_supportsLockScreen = set;
}

inline bool Application::supportsGlobalShortcuts() const
{
    return m_supportsGlobalShortcuts;
}

inline void Application::setSupportsGlobalShortcuts(bool set)
{
    m_supportsGlobalShortcuts = set;
}

inline static Application *kwinApp()
{
    Q_ASSERT(qobject_cast<Application *>(QCoreApplication::instance()));

    return static_cast<Application *>(QCoreApplication::instance());
}

} // namespace
