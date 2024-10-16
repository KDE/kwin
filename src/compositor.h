/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "config-kwin.h"
#include <kwin_export.h>

#if KWIN_BUILD_X11
#include <xcb/xcb.h>
#endif

#include <QHash>
#include <QObject>
#include <QRegion>
#include <QTimer>
#include <memory>

namespace KWin
{

class Output;
class CursorScene;
class RenderBackend;
class RenderLayer;
class RenderLoop;
class RenderTarget;
class WorkspaceScene;
class Window;
class OutputFrame;

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

    virtual void start() = 0;
    virtual void stop() = 0;

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

#if KWIN_BUILD_X11
    // for delayed supportproperty management of effects
    void keepSupportProperty(xcb_atom_t atom);
    void removeSupportProperty(xcb_atom_t atom);
#endif

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

    virtual void inhibit(Window *window);
    virtual void uninhibit(Window *window);

Q_SIGNALS:
    void compositingToggled(bool active);
    void aboutToDestroy();
    void aboutToToggleCompositing();
    void sceneCreated();

protected:
    explicit Compositor(QObject *parent = nullptr);

    static Compositor *s_compositor;

protected Q_SLOTS:
    virtual void composite(RenderLoop *renderLoop) = 0;

private Q_SLOTS:
    void handleFrameRequested(RenderLoop *renderLoop);

protected:
#if KWIN_BUILD_X11
    void deleteUnusedSupportProperties();
#endif

    Output *findOutput(RenderLoop *loop) const;

    void addSuperLayer(RenderLayer *layer);
    void removeSuperLayer(RenderLayer *layer);

    void prePaintPass(RenderLayer *layer, QRegion *damage);
    void postPaintPass(RenderLayer *layer);
    void paintPass(RenderLayer *layer, const RenderTarget &renderTarget, const QRegion &region);
    void framePass(RenderLayer *layer, OutputFrame *frame);

    State m_state = State::Off;
#if KWIN_BUILD_X11
    QList<xcb_atom_t> m_unusedSupportProperties;
    QTimer m_unusedSupportPropertyTimer;
#endif
    std::unique_ptr<WorkspaceScene> m_scene;
    std::unique_ptr<CursorScene> m_cursorScene;
    std::unique_ptr<RenderBackend> m_backend;
    QHash<RenderLoop *, RenderLayer *> m_superlayers;
};

} // namespace KWin
