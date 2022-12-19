/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <kwinglobals.h>

#include <QObject>
#include <QRegion>
#include <QTimer>
#include <memory>

namespace KWin
{

class Output;
class CompositorSelectionOwner;
class CursorScene;
class CursorView;
class RenderBackend;
class RenderLayer;
class RenderLoop;
class RenderTarget;
class WorkspaceScene;
class Window;
class X11Window;
class X11SyncManager;

class KWIN_EXPORT Compositor : public QObject
{
    Q_OBJECT
public:
    enum class State {
        On = 0,
        Off,
        Starting,
        Stopping
    };

    ~Compositor() override;
    static Compositor *self();

    /**
     * Schedules a new repaint if no repaint is currently scheduled.
     */
    void scheduleRepaint();

    /**
     * Toggles compositing, that is if the Compositor is suspended it will be resumed
     * and if the Compositor is active it will be suspended.
     * Invoked by keybinding (shortcut default: Shift + Alt + F12).
     */
    virtual void toggleCompositing() = 0;

    /**
     * Re-initializes the Compositor completely.
     * Connected to the D-Bus signal org.kde.KWin /KWin reinitCompositing
     */
    virtual void reinitialize();

    /**
     * Whether the Compositor is active. That is a Scene is present and the Compositor is
     * not shutting down itself.
     */
    bool isActive();

    WorkspaceScene *scene() const
    {
        return m_scene.get();
    }
    CursorScene *cursorScene() const
    {
        return m_cursorScene.get();
    }
    RenderBackend *backend() const
    {
        return m_backend.get();
    }

    /**
     * @brief Static check to test whether the Compositor is available and active.
     *
     * @return bool @c true if there is a Compositor and it is active, @c false otherwise
     */
    static bool compositing()
    {
        return s_compositor != nullptr && s_compositor->isActive();
    }

    // for delayed supportproperty management of effects
    void keepSupportProperty(xcb_atom_t atom);
    void removeSupportProperty(xcb_atom_t atom);

    /**
     * Whether Compositing is possible in the Platform.
     * Returning @c false in this method makes only sense if requiresCompositing returns @c false.
     *
     * The default implementation returns @c true.
     * @see requiresCompositing
     */
    virtual bool compositingPossible() const;
    /**
     * Returns a user facing text explaining why compositing is not possible in case
     * compositingPossible returns @c false.
     *
     * The default implementation returns an empty string.
     * @see compositingPossible
     */
    virtual QString compositingNotPossibleReason() const;
    /**
     * Whether OpenGL compositing is broken.
     * The Platform can implement this method if it is able to detect whether OpenGL compositing
     * broke (e.g. triggered a crash in a previous run).
     *
     * Default implementation returns @c false.
     * @see createOpenGLSafePoint
     */
    virtual bool openGLCompositingIsBroken() const;
    enum class OpenGLSafePoint {
        PreInit,
        PostInit,
        PreFrame,
        PostFrame,
        PostLastGuardedFrame
    };
    /**
     * This method is invoked before and after creating the OpenGL rendering Scene.
     * An implementing Platform can use it to detect crashes triggered by the OpenGL implementation.
     * This can be used for openGLCompositingIsBroken.
     *
     * The default implementation does nothing.
     * @see openGLCompositingIsBroken.
     */
    virtual void createOpenGLSafePoint(OpenGLSafePoint safePoint);

Q_SIGNALS:
    void compositingToggled(bool active);
    void aboutToDestroy();
    void aboutToToggleCompositing();
    void sceneCreated();

protected:
    explicit Compositor(QObject *parent = nullptr);

    virtual void start() = 0;
    virtual void stop();

    /**
     * @brief Prepares start.
     * @return bool @c true if start should be continued and @c if not.
     */
    bool setupStart();
    /**
     * Continues the startup after Scene And Workspace are created
     */
    void startupWithWorkspace();

    virtual void configChanged();

    void destroyCompositorSelection();

    static Compositor *s_compositor;

protected Q_SLOTS:
    virtual void composite(RenderLoop *renderLoop);

private Q_SLOTS:
    void handleFrameRequested(RenderLoop *renderLoop);

private:
    void initializeX11();
    void cleanupX11();

    void releaseCompositorSelection();
    void deleteUnusedSupportProperties();

    bool attemptOpenGLCompositing();
    bool attemptQPainterCompositing();

    Output *findOutput(RenderLoop *loop) const;
    void addOutput(Output *output);
    void removeOutput(Output *output);

    void addSuperLayer(RenderLayer *layer);
    void removeSuperLayer(RenderLayer *layer);

    void prePaintPass(RenderLayer *layer);
    void postPaintPass(RenderLayer *layer);
    void preparePaintPass(RenderLayer *layer, QRegion *repaint);
    void paintPass(RenderLayer *layer, RenderTarget *target, const QRegion &region);

    State m_state = State::Off;
    std::unique_ptr<CompositorSelectionOwner> m_selectionOwner;
    QTimer m_releaseSelectionTimer;
    QList<xcb_atom_t> m_unusedSupportProperties;
    QTimer m_unusedSupportPropertyTimer;
    std::unique_ptr<WorkspaceScene> m_scene;
    std::unique_ptr<CursorScene> m_cursorScene;
    std::unique_ptr<RenderBackend> m_backend;
    QHash<RenderLoop *, RenderLayer *> m_superlayers;
    CompositingType m_selectedCompositor = NoCompositing;
};

class KWIN_EXPORT WaylandCompositor final : public Compositor
{
    Q_OBJECT
public:
    static WaylandCompositor *create(QObject *parent = nullptr);
    ~WaylandCompositor() override;

    void toggleCompositing() override;

protected:
    void start() override;

private:
    explicit WaylandCompositor(QObject *parent);
};

class KWIN_EXPORT X11Compositor final : public Compositor
{
    Q_OBJECT
public:
    enum SuspendReason {
        NoReasonSuspend = 0,
        UserSuspend = 1 << 0,
        BlockRuleSuspend = 1 << 1,
        ScriptSuspend = 1 << 2,
        AllReasonSuspend = 0xff
    };
    Q_DECLARE_FLAGS(SuspendReasons, SuspendReason)
    Q_ENUM(SuspendReason)
    Q_FLAG(SuspendReasons)

    static X11Compositor *create(QObject *parent = nullptr);
    ~X11Compositor() override;

    X11SyncManager *syncManager() const;

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
    void suspend(SuspendReason reason);

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
    void resume(SuspendReason reason);

    void toggleCompositing() override;
    void reinitialize() override;
    void configChanged() override;
    bool compositingPossible() const override;
    QString compositingNotPossibleReason() const override;
    bool openGLCompositingIsBroken() const override;
    void createOpenGLSafePoint(OpenGLSafePoint safePoint) override;

    /**
     * Checks whether @p w is the Scene's overlay window.
     */
    bool checkForOverlayWindow(WId w) const;

    /**
     * @returns Whether the Scene's Overlay X Window is visible.
     */
    bool isOverlayWindowVisible() const;

    void updateClientCompositeBlocking(X11Window *client = nullptr);

    static X11Compositor *self();

protected:
    void start() override;
    void stop() override;
    void composite(RenderLoop *renderLoop) override;

private:
    explicit X11Compositor(QObject *parent);

    std::unique_ptr<QThread> m_openGLFreezeProtectionThread;
    std::unique_ptr<QTimer> m_openGLFreezeProtection;
    std::unique_ptr<X11SyncManager> m_syncManager;
    /**
     * Whether the Compositor is currently suspended, 8 bits encoding the reason
     */
    SuspendReasons m_suspended;
    int m_framesToTestForSafety = 3;
};

}
