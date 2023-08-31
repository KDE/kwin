/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "compositor.h"

#include <config-kwin.h>

#include "core/output.h"
#include "core/outputbackend.h"
#include "core/outputlayer.h"
#include "core/renderlayer.h"
#include "core/renderloop.h"
#include "dbusinterface.h"
#include "effects.h"
#include "ftrace.h"
#include "platformsupport/scenes/opengl/openglbackend.h"
#include "platformsupport/scenes/qpainter/qpainterbackend.h"
#include "scene/cursorscene.h"
#include "scene/itemrenderer_opengl.h"
#include "scene/itemrenderer_qpainter.h"
#include "scene/workspacescene_opengl.h"
#include "scene/workspacescene_qpainter.h"
#include "utils/common.h"

#include "libkwineffects/kwinglplatform.h"

#include <KSelectionOwner>

#include <QQuickWindow>

#include <xcb/composite.h>

namespace KWin
{

Compositor *Compositor::s_compositor = nullptr;
Compositor *Compositor::self()
{
    return s_compositor;
}

class CompositorSelectionOwner : public KSelectionOwner
{
    Q_OBJECT
public:
    CompositorSelectionOwner(const char *selection)
        : KSelectionOwner(selection, kwinApp()->x11Connection(), kwinApp()->x11RootWindow())
        , m_owning(false)
    {
        connect(this, &CompositorSelectionOwner::lostOwnership,
                this, [this]() {
                    m_owning = false;
                });
    }
    bool owning() const
    {
        return m_owning;
    }
    void setOwning(bool own)
    {
        m_owning = own;
    }

private:
    bool m_owning;
};

Compositor::Compositor(QObject *workspace)
    : QObject(workspace)
{
    connect(options, &Options::configChanged, this, &Compositor::configChanged);
    connect(options, &Options::animationSpeedChanged, this, &Compositor::configChanged);

    // 2 sec which should be enough to restart the compositor.
    static const int compositorLostMessageDelay = 2000;

    m_releaseSelectionTimer.setSingleShot(true);
    m_releaseSelectionTimer.setInterval(compositorLostMessageDelay);
    connect(&m_releaseSelectionTimer, &QTimer::timeout,
            this, &Compositor::releaseCompositorSelection);

    m_unusedSupportPropertyTimer.setInterval(compositorLostMessageDelay);
    m_unusedSupportPropertyTimer.setSingleShot(true);
    connect(&m_unusedSupportPropertyTimer, &QTimer::timeout,
            this, &Compositor::deleteUnusedSupportProperties);

    // Delay the call to start by one event cycle.
    // The ctor of this class is invoked from the Workspace ctor, that means before
    // Workspace is completely constructed, so calling Workspace::self() would result
    // in undefined behavior. This is fixed by using a delayed invocation.
    QTimer::singleShot(0, this, &Compositor::start);

    connect(kwinApp(), &Application::x11ConnectionChanged, this, &Compositor::initializeX11);
    connect(kwinApp(), &Application::x11ConnectionAboutToBeDestroyed, this, &Compositor::cleanupX11);

    // register DBus
    new CompositorDBusInterface(this);
    FTraceLogger::create();
}

Compositor::~Compositor()
{
    deleteUnusedSupportProperties();
    destroyCompositorSelection();
    s_compositor = nullptr;
}

bool Compositor::attemptOpenGLCompositing()
{
    // Some broken drivers crash on glXQuery() so to prevent constant KWin crashes:
    if (openGLCompositingIsBroken()) {
        qCWarning(KWIN_CORE) << "KWin has detected that your OpenGL library is unsafe to use";
        return false;
    }

    createOpenGLSafePoint(OpenGLSafePoint::PreInit);
    auto safePointScope = qScopeGuard([this]() {
        createOpenGLSafePoint(OpenGLSafePoint::PostInit);
    });

    std::unique_ptr<OpenGLBackend> backend = kwinApp()->outputBackend()->createOpenGLBackend();
    if (!backend) {
        return false;
    }
    if (!backend->isFailed()) {
        backend->init();
    }
    if (backend->isFailed()) {
        return false;
    }

    const QByteArray forceEnv = qgetenv("KWIN_COMPOSE");
    if (!forceEnv.isEmpty()) {
        if (qstrcmp(forceEnv, "O2") == 0 || qstrcmp(forceEnv, "O2ES") == 0) {
            qCDebug(KWIN_CORE) << "OpenGL 2 compositing enforced by environment variable";
        } else {
            // OpenGL 2 disabled by environment variable
            return false;
        }
    } else {
        if (!backend->isDirectRendering()) {
            return false;
        }
        if (GLPlatform::instance()->recommendedCompositor() < OpenGLCompositing) {
            qCDebug(KWIN_CORE) << "Driver does not recommend OpenGL compositing";
            return false;
        }
    }

    // We only support the OpenGL 2+ shader API, not GL_ARB_shader_objects
    if (!hasGLVersion(2, 0)) {
        qCDebug(KWIN_CORE) << "OpenGL 2.0 is not supported";
        return false;
    }

    m_scene = std::make_unique<WorkspaceSceneOpenGL>(backend.get());
    m_cursorScene = std::make_unique<CursorScene>(std::make_unique<ItemRendererOpenGL>());
    m_backend = std::move(backend);

    // set strict binding
    if (options->isGlStrictBindingFollowsDriver()) {
        options->setGlStrictBinding(!GLPlatform::instance()->supports(GLFeature::LooseBinding));
    }

    qCDebug(KWIN_CORE) << "OpenGL compositing has been successfully initialized";
    return true;
}

bool Compositor::attemptQPainterCompositing()
{
    std::unique_ptr<QPainterBackend> backend(kwinApp()->outputBackend()->createQPainterBackend());
    if (!backend || backend->isFailed()) {
        return false;
    }

    m_scene = std::make_unique<WorkspaceSceneQPainter>(backend.get());
    m_cursorScene = std::make_unique<CursorScene>(std::make_unique<ItemRendererQPainter>());
    m_backend = std::move(backend);

    qCDebug(KWIN_CORE) << "QPainter compositing has been successfully initialized";
    return true;
}

void Compositor::initializeX11()
{
    xcb_connection_t *connection = kwinApp()->x11Connection();
    if (!connection) {
        return;
    }

    if (!m_selectionOwner) {
        m_selectionOwner = std::make_unique<CompositorSelectionOwner>("_NET_WM_CM_S0");
        connect(m_selectionOwner.get(), &CompositorSelectionOwner::lostOwnership, this, &Compositor::stop);
    }
    if (!m_selectionOwner->owning()) {
        // Force claim ownership.
        m_selectionOwner->claim(true);
        m_selectionOwner->setOwning(true);
    }

    xcb_composite_redirect_subwindows(connection, kwinApp()->x11RootWindow(),
                                      XCB_COMPOSITE_REDIRECT_MANUAL);
}

void Compositor::cleanupX11()
{
    m_selectionOwner.reset();
}

void Compositor::addSuperLayer(RenderLayer *layer)
{
    m_superlayers.insert(layer->loop(), layer);
    connect(layer->loop(), &RenderLoop::frameRequested, this, &Compositor::composite);
}

void Compositor::removeSuperLayer(RenderLayer *layer)
{
    m_superlayers.remove(layer->loop());
    disconnect(layer->loop(), &RenderLoop::frameRequested, this, &Compositor::composite);
    delete layer;
}

void Compositor::destroyCompositorSelection()
{
    m_selectionOwner.reset();
}

void Compositor::releaseCompositorSelection()
{
    switch (m_state) {
    case State::On:
        // We are compositing at the moment. Don't release.
        break;
    case State::Off:
        if (m_selectionOwner) {
            qCDebug(KWIN_CORE) << "Releasing compositor selection";
            m_selectionOwner->setOwning(false);
            m_selectionOwner->release();
        }
        break;
    case State::Starting:
    case State::Stopping:
        // Still starting or shutting down the compositor. Starting might fail
        // or after stopping a restart might follow. So test again later on.
        m_releaseSelectionTimer.start();
        break;
    }
}

void Compositor::keepSupportProperty(xcb_atom_t atom)
{
    m_unusedSupportProperties.removeAll(atom);
}

void Compositor::removeSupportProperty(xcb_atom_t atom)
{
    m_unusedSupportProperties << atom;
    m_unusedSupportPropertyTimer.start();
}

void Compositor::deleteUnusedSupportProperties()
{
    if (m_state == State::Starting || m_state == State::Stopping) {
        // Currently still maybe restarting the compositor.
        m_unusedSupportPropertyTimer.start();
        return;
    }
    if (auto *con = kwinApp()->x11Connection()) {
        for (const xcb_atom_t &atom : std::as_const(m_unusedSupportProperties)) {
            // remove property from root window
            xcb_delete_property(con, kwinApp()->x11RootWindow(), atom);
        }
        m_unusedSupportProperties.clear();
    }
}

void Compositor::configChanged()
{
    reinitialize();
}

void Compositor::reinitialize()
{
    // Reparse config. Config options will be reloaded by start()
    kwinApp()->config()->reparseConfiguration();

    // Restart compositing
    stop();
    start();

    if (effects) { // start() may fail
        effects->reconfigure();
    }
}

uint Compositor::outputFormat(Output *output)
{
    OutputLayer *primaryLayer = m_backend->primaryLayer(output);
    return primaryLayer->format();
}

bool Compositor::isActive()
{
    return m_state == State::On;
}

bool Compositor::compositingPossible() const
{
    return true;
}

QString Compositor::compositingNotPossibleReason() const
{
    return QString();
}

bool Compositor::openGLCompositingIsBroken() const
{
    return false;
}

void Compositor::createOpenGLSafePoint(OpenGLSafePoint safePoint)
{
}

} // namespace KWin

// included for CompositorSelectionOwner
#include "compositor.moc"

#include "moc_compositor.cpp"
