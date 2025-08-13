/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "effect/globals.h"
#include "kwin_export.h"

#include <QHash>
#include <QObject>
#include <QRegion>

#include <memory>

namespace KWin
{

class ColorDescription;
class GLTexture;
class LogicalOutput;
class BackendOutput;
class RenderBackend;
class OutputLayer;
class RenderLoop;
class RenderTarget;
class WorkspaceScene;
class Window;
class OutputFrame;
class SceneView;
class ItemView;
class RenderLoopDrivenQAnimationDriver;

class KWIN_EXPORT Compositor : public QObject
{
    Q_OBJECT
public:
    static Compositor *create(QObject *parent = nullptr);

    enum class State {
        On = 0,
        Off,
        Starting,
        Stopping
    };

    ~Compositor() override;
    static Compositor *self();

    void start();
    void stop();

    /**
     * Re-initializes the Compositor completely.
     * Connected to the D-Bus signal org.kde.KWin /KWin reinitCompositing
     */
    void reinitialize();

    /**
     * Whether the Compositor is active. That is a Scene is present and the Compositor is
     * not shutting down itself.
     */
    bool isActive();

    WorkspaceScene *scene() const
    {
        return m_scene.get();
    }
    RenderBackend *backend() const
    {
        return m_backend.get();
    }

    void createRenderer();

Q_SIGNALS:
    void compositingToggled(bool active);
    void aboutToDestroy();
    void aboutToToggleCompositing();
    void sceneCreated();

protected:
    explicit Compositor(QObject *parent = nullptr);

    static Compositor *s_compositor;

protected Q_SLOTS:
    void composite(RenderLoop *renderLoop);

private Q_SLOTS:
    void handleFrameRequested(RenderLoop *renderLoop);

protected:
    BackendOutput *findOutput(RenderLoop *loop) const;

    void createScene();
    bool attemptOpenGLCompositing();
    bool attemptQPainterCompositing();
    void handleOutputsChanged();
    void addOutput(BackendOutput *output);
    void removeOutput(BackendOutput *output);
    void assignOutputLayers(BackendOutput *output);

    CompositingType m_selectedCompositor = NoCompositing;

    State m_state = State::Off;
    std::unique_ptr<WorkspaceScene> m_scene;
    std::unique_ptr<RenderBackend> m_backend;
    std::unordered_map<RenderLoop *, std::unique_ptr<SceneView>> m_primaryViews;
    std::unordered_map<RenderLoop *, std::unordered_map<OutputLayer *, std::unique_ptr<ItemView>>> m_overlayViews;
    std::unordered_set<RenderLoop *> m_brokenCursors;
    std::optional<bool> m_allowOverlaysEnv;
    RenderLoopDrivenQAnimationDriver *m_renderLoopDrivenAnimationDriver;
};

} // namespace KWin
