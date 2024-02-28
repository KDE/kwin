/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "compositor_x11.h"
#include "core/outputbackend.h"
#include "core/overlaywindow.h"
#include "core/renderbackend.h"
#include "core/renderlayer.h"
#include "effect/effecthandler.h"
#include "opengl/glplatform.h"
#include "options.h"
#include "platformsupport/scenes/opengl/openglbackend.h"
#include "scene/surfaceitem_x11.h"
#include "scene/workspacescene_opengl.h"
#include "utils/common.h"
#include "utils/xcbutils.h"
#include "window.h"
#include "workspace.h"
#include "x11syncmanager.h"

#include <KCrash>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KSelectionOwner>

#include <QAction>
#include <QOpenGLContext>
#include <QThread>

Q_DECLARE_METATYPE(KWin::X11Compositor::SuspendReason)

namespace KWin
{

class X11CompositorSelectionOwner : public KSelectionOwner
{
    Q_OBJECT

public:
    X11CompositorSelectionOwner(const char *selection)
        : KSelectionOwner(selection, kwinApp()->x11Connection(), kwinApp()->x11RootWindow())
        , m_owning(false)
    {
        connect(this, &X11CompositorSelectionOwner::lostOwnership, this, [this]() {
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

X11Compositor *X11Compositor::create(QObject *parent)
{
    Q_ASSERT(!s_compositor);
    auto *compositor = new X11Compositor(parent);
    s_compositor = compositor;
    return compositor;
}

X11Compositor::X11Compositor(QObject *parent)
    : Compositor(parent)
    , m_suspended(options->isUseCompositing() ? NoReasonSuspend : UserSuspend)
{
    if (qEnvironmentVariableIsSet("KWIN_MAX_FRAMES_TESTED")) {
        m_framesToTestForSafety = qEnvironmentVariableIntValue("KWIN_MAX_FRAMES_TESTED");
    }

    connect(options, &Options::configChanged, this, [this]() {
        if (m_suspended) {
            stop();
        } else {
            reinitialize();
        }
    });

    m_releaseSelectionTimer.setSingleShot(true);
    m_releaseSelectionTimer.setInterval(2000);
    connect(&m_releaseSelectionTimer, &QTimer::timeout, this, &X11Compositor::releaseCompositorSelection);

    QAction *toggleAction = new QAction(this);
    toggleAction->setProperty("componentName", QStringLiteral("kwin"));
    toggleAction->setObjectName("Suspend Compositing");
    toggleAction->setText(i18n("Suspend Compositing"));
    KGlobalAccel::self()->setDefaultShortcut(toggleAction, QList<QKeySequence>() << (Qt::SHIFT | Qt::ALT | Qt::Key_F12));
    KGlobalAccel::self()->setShortcut(toggleAction, QList<QKeySequence>() << (Qt::SHIFT | Qt::ALT | Qt::Key_F12));
    connect(toggleAction, &QAction::triggered, this, &X11Compositor::toggle);
}

X11Compositor::~X11Compositor()
{
    Q_EMIT aboutToDestroy();
    if (m_openGLFreezeProtectionThread) {
        m_openGLFreezeProtectionThread->quit();
        m_openGLFreezeProtectionThread->wait();
    }
    stop(); // this can't be called in the destructor of Compositor
    destroyCompositorSelection();
}

X11SyncManager *X11Compositor::syncManager() const
{
    return m_syncManager.get();
}

void X11Compositor::toggle()
{
    if (m_suspended) {
        // Direct user call; clear all bits.
        resume(AllReasonSuspend);
    } else {
        // But only set the user one (sufficient to suspend).
        suspend(UserSuspend);
    }
}

void X11Compositor::reinitialize()
{
    // Resume compositing if suspended.
    m_suspended = NoReasonSuspend;
    m_inhibitors.clear();
    Compositor::reinitialize();
}

void X11Compositor::suspend(X11Compositor::SuspendReason reason)
{
    Q_ASSERT(reason != NoReasonSuspend);
    m_suspended |= reason;
    stop();
}

void X11Compositor::resume(X11Compositor::SuspendReason reason)
{
    Q_ASSERT(reason != NoReasonSuspend);
    m_suspended &= ~reason;
    if (reason & BlockRuleSuspend) {
        m_inhibitors.clear();
    }
    start();
}

void X11Compositor::destroyCompositorSelection()
{
    m_selectionOwner.reset();
}

void X11Compositor::releaseCompositorSelection()
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

bool X11Compositor::attemptOpenGLCompositing()
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
        if (GLPlatform::instance()->recommendedCompositor() < OpenGLCompositing) {
            qCDebug(KWIN_CORE) << "Driver does not recommend OpenGL compositing";
            return false;
        }
    }

    // We only support the OpenGL 2+ shader API, not GL_ARB_shader_objects
    if (!backend->openglContext()->hasVersion(Version(2, 0))) {
        qCDebug(KWIN_CORE) << "OpenGL 2.0 is not supported";
        return false;
    }

    m_scene = std::make_unique<WorkspaceSceneOpenGL>(backend.get());
    m_backend = std::move(backend);

    // set strict binding
    if (options->isGlStrictBindingFollowsDriver()) {
        options->setGlStrictBinding(!GLPlatform::instance()->isLooseBinding());
    }

    qCDebug(KWIN_CORE) << "OpenGL compositing has been successfully initialized";
    return true;
}

void X11Compositor::start()
{
    if (m_suspended) {
        QStringList reasons;
        if (m_suspended & UserSuspend) {
            reasons << QStringLiteral("Disabled by User");
        }
        if (m_suspended & BlockRuleSuspend) {
            reasons << QStringLiteral("Disabled by Window");
        }
        qCInfo(KWIN_CORE) << "Compositing is suspended, reason:" << reasons;
        return;
    } else if (!compositingPossible()) {
        qCWarning(KWIN_CORE) << "Compositing is not possible";
        return;
    }

    if (kwinApp()->isTerminating()) {
        return;
    }
    if (m_state != State::Off) {
        return;
    }

    Q_EMIT aboutToToggleCompositing();
    m_state = State::Starting;

    // Claim special _NET_WM_CM_S0 selection and redirect child windows of the root window.
    if (!m_selectionOwner) {
        m_selectionOwner = std::make_unique<X11CompositorSelectionOwner>("_NET_WM_CM_S0");
        connect(m_selectionOwner.get(), &X11CompositorSelectionOwner::lostOwnership, this, &X11Compositor::stop);
    }
    if (!m_selectionOwner->owning()) {
        // Force claim ownership.
        m_selectionOwner->claim(true);
        m_selectionOwner->setOwning(true);
    }

    xcb_composite_redirect_subwindows(kwinApp()->x11Connection(),
                                      kwinApp()->x11RootWindow(),
                                      XCB_COMPOSITE_REDIRECT_MANUAL);

    // Decide what compositing types can be used.
    QList<CompositingType> candidateCompositors = kwinApp()->outputBackend()->supportedCompositors();
    const auto userConfigIt = std::find(candidateCompositors.begin(), candidateCompositors.end(), options->compositingMode());
    if (userConfigIt != candidateCompositors.end()) {
        candidateCompositors.erase(userConfigIt);
        candidateCompositors.prepend(options->compositingMode());
    } else {
        qCWarning(KWIN_CORE) << "Configured compositor not supported by Platform. Falling back to defaults";
    }

    for (auto type : std::as_const(candidateCompositors)) {
        bool stop = false;
        switch (type) {
        case OpenGLCompositing:
            qCDebug(KWIN_CORE) << "Attempting to load the OpenGL scene";
            stop = attemptOpenGLCompositing();
            break;
        case QPainterCompositing:
            qCDebug(KWIN_CORE) << "QPainter compositing is unsupported on X11";
            break;
        case NoCompositing:
            qCDebug(KWIN_CORE) << "Starting without compositing...";
            stop = true;
            break;
        }

        if (stop) {
            break;
        } else if (qEnvironmentVariableIsSet("KWIN_COMPOSE")) {
            qCCritical(KWIN_CORE) << "Could not fulfill the requested compositing mode in KWIN_COMPOSE:" << type << ". Exiting.";
            qApp->quit();
        }
    }

    if (!m_backend) {
        m_state = State::Off;

        xcb_composite_unredirect_subwindows(kwinApp()->x11Connection(),
                                            kwinApp()->x11RootWindow(),
                                            XCB_COMPOSITE_REDIRECT_MANUAL);
        if (m_selectionOwner) {
            m_selectionOwner->setOwning(false);
            m_selectionOwner->release();
        }
        return;
    }

    Q_EMIT sceneCreated();

    kwinApp()->setX11CompositeWindow(backend()->overlayWindow()->window());

    auto workspaceLayer = new RenderLayer(workspace()->outputs()[0]->renderLoop());
    workspaceLayer->setDelegate(std::make_unique<SceneDelegate>(m_scene.get(), nullptr));
    workspaceLayer->setGeometry(workspace()->geometry());
    connect(workspace(), &Workspace::geometryChanged, workspaceLayer, [workspaceLayer]() {
        workspaceLayer->setGeometry(workspace()->geometry());
    });
    addSuperLayer(workspaceLayer);

    m_state = State::On;

    const auto windows = workspace()->windows();
    for (Window *window : windows) {
        window->setupCompositing();
    }

    // Sets also the 'effects' pointer.
    kwinApp()->createEffectsHandler(this, m_scene.get());

    m_syncManager.reset(X11SyncManager::create(m_backend.get()));
    if (m_releaseSelectionTimer.isActive()) {
        m_releaseSelectionTimer.stop();
    }

    Q_EMIT compositingToggled(true);
}

void X11Compositor::stop()
{
    if (m_state == State::Off || m_state == State::Stopping) {
        return;
    }
    m_state = State::Stopping;
    Q_EMIT aboutToToggleCompositing();

    m_releaseSelectionTimer.start();

    // Some effects might need access to effect windows when they are about to
    // be destroyed, for example to unreference deleted windows, so we have to
    // make sure that effect windows outlive effects.
    delete effects;
    effects = nullptr;

    if (Workspace::self()) {
        const auto windows = workspace()->windows();
        for (Window *window : windows) {
            window->finishCompositing();
        }
        xcb_composite_unredirect_subwindows(kwinApp()->x11Connection(),
                                            kwinApp()->x11RootWindow(),
                                            XCB_COMPOSITE_REDIRECT_MANUAL);
    }

    if (m_backend->compositingType() == OpenGLCompositing) {
        // some layers need a context current for destruction
        static_cast<OpenGLBackend *>(m_backend.get())->makeCurrent();
    }

    const auto superlayers = m_superlayers;
    for (auto it = superlayers.begin(); it != superlayers.end(); ++it) {
        removeSuperLayer(*it);
    }

    m_syncManager.reset();
    m_scene.reset();
    m_backend.reset();

    kwinApp()->setX11CompositeWindow(XCB_WINDOW_NONE);

    m_state = State::Off;
    Q_EMIT compositingToggled(false);
}

void X11Compositor::composite(RenderLoop *renderLoop)
{
    if (backend()->overlayWindow() && !backend()->overlayWindow()->isVisible()) {
        // Return since nothing is visible.
        return;
    }

    QList<Window *> windows = workspace()->stackingOrder();
    QList<SurfaceItemX11 *> dirtyItems;

    // Reset the damage state of each window and fetch the damage region
    // without waiting for a reply
    for (Window *window : std::as_const(windows)) {
        SurfaceItemX11 *surfaceItem = static_cast<SurfaceItemX11 *>(window->surfaceItem());
        if (surfaceItem->fetchDamage()) {
            dirtyItems.append(surfaceItem);
        }
    }

    if (dirtyItems.count() > 0) {
        if (m_syncManager) {
            m_syncManager->triggerFence();
        }
        xcb_flush(kwinApp()->x11Connection());
    }

    // Get the replies
    for (SurfaceItemX11 *item : std::as_const(dirtyItems)) {
        item->waitForDamage();
    }

    if (m_framesToTestForSafety > 0 && (backend()->compositingType() & OpenGLCompositing)) {
        createOpenGLSafePoint(OpenGLSafePoint::PreFrame);
    }

    Compositor::composite(renderLoop);

    if (m_syncManager) {
        if (!m_syncManager->endFrame()) {
            qCDebug(KWIN_CORE) << "Aborting explicit synchronization with the X command stream.";
            qCDebug(KWIN_CORE) << "Future frames will be rendered unsynchronized.";
            m_syncManager.reset();
        }
    }

    if (m_framesToTestForSafety > 0) {
        if (backend()->compositingType() & OpenGLCompositing) {
            createOpenGLSafePoint(OpenGLSafePoint::PostFrame);
        }
        m_framesToTestForSafety--;
        if (m_framesToTestForSafety == 0 && (backend()->compositingType() & OpenGLCompositing)) {
            createOpenGLSafePoint(OpenGLSafePoint::PostLastGuardedFrame);
        }
    }
}

void X11Compositor::inhibit(Window *window)
{
    m_inhibitors.insert(window);
    // Do NOT attempt to call suspend(true) from within the eventchain!
    if (!(m_suspended & BlockRuleSuspend)) {
        QMetaObject::invokeMethod(
            this, [this]() {
                suspend(BlockRuleSuspend);
            },
            Qt::QueuedConnection);
    }
}

void X11Compositor::uninhibit(Window *window)
{
    if (!m_inhibitors.remove(window)) {
        return;
    }
    if (m_suspended & BlockRuleSuspend) {
        if (m_inhibitors.isEmpty()) {
            // Do NOT attempt to call suspend(false) from within the eventchain!
            QMetaObject::invokeMethod(
                this, [this]() {
                    resume(BlockRuleSuspend);
                },
                Qt::QueuedConnection);
        }
    }
}

X11Compositor *X11Compositor::self()
{
    return qobject_cast<X11Compositor *>(Compositor::self());
}

bool X11Compositor::openGLCompositingIsBroken() const
{
    auto timestamp = KConfigGroup(kwinApp()->config(), QStringLiteral("Compositing")).readEntry(QLatin1String("LastFailureTimestamp"), 0);
    if (timestamp > 0) {
        if (QDateTime::currentSecsSinceEpoch() - timestamp < 60) {
            return true;
        }
    }

    return false;
}

QString X11Compositor::compositingNotPossibleReason() const
{
    // first off, check whether we figured that we'll crash on detection because of a buggy driver
    KConfigGroup gl_workaround_group(kwinApp()->config(), QStringLiteral("Compositing"));
    if (gl_workaround_group.readEntry("Backend", "OpenGL") == QLatin1String("OpenGL") && openGLCompositingIsBroken()) {
        return i18n("<b>OpenGL compositing (the default) has crashed KWin in the past.</b><br>"
                    "This was most likely due to a driver bug."
                    "<p>If you think that you have meanwhile upgraded to a stable driver,<br>"
                    "you can reset this protection but <b>be aware that this might result in an immediate crash!</b></p>");
    }

    if (!Xcb::Extensions::self()->isCompositeAvailable() || !Xcb::Extensions::self()->isDamageAvailable()) {
        return i18n("Required X extensions (XComposite and XDamage) are not available.");
    }
    if (!Xcb::Extensions::self()->hasGlx()) {
        return i18n("GLX/OpenGL is not available.");
    }
    return QString();
}

bool X11Compositor::compositingPossible() const
{
    // first off, check whether we figured that we'll crash on detection because of a buggy driver
    KConfigGroup gl_workaround_group(kwinApp()->config(), QStringLiteral("Compositing"));
    if (gl_workaround_group.readEntry("Backend", "OpenGL") == QLatin1String("OpenGL") && openGLCompositingIsBroken()) {
        qCWarning(KWIN_CORE) << "Compositing disabled: video driver seems unstable. If you think it's a false positive, please try again in a few minutes.";
        return false;
    }

    if (!Xcb::Extensions::self()->isCompositeAvailable()) {
        qCWarning(KWIN_CORE) << "Compositing disabled: no composite extension available";
        return false;
    }
    if (!Xcb::Extensions::self()->isDamageAvailable()) {
        qCWarning(KWIN_CORE) << "Compositing disabled: no damage extension available";
        return false;
    }
    if (Xcb::Extensions::self()->hasGlx()) {
        return true;
    }
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES) {
        return true;
    } else if (qstrcmp(qgetenv("KWIN_COMPOSE"), "O2ES") == 0) {
        return true;
    }
    qCWarning(KWIN_CORE) << "Compositing disabled: no OpenGL support";
    return false;
}

void X11Compositor::createOpenGLSafePoint(OpenGLSafePoint safePoint)
{
    auto group = KConfigGroup(kwinApp()->config(), QStringLiteral("Compositing"));
    switch (safePoint) {
    case OpenGLSafePoint::PreInit:
        // Explicitly write the failure timestamp so that if we crash during
        // OpenGL init, we know we should not try again.
        group.writeEntry(QLatin1String("LastFailureTimestamp"), QDateTime::currentSecsSinceEpoch());
        group.sync();
        // Deliberately continue with PreFrame
        Q_FALLTHROUGH();
    case OpenGLSafePoint::PreFrame:
        if (m_openGLFreezeProtectionThread == nullptr) {
            Q_ASSERT(m_openGLFreezeProtection == nullptr);
            m_openGLFreezeProtectionThread = std::make_unique<QThread>();
            m_openGLFreezeProtectionThread->setObjectName("FreezeDetector");
            m_openGLFreezeProtectionThread->start();
            m_openGLFreezeProtection = std::make_unique<QTimer>();
            m_openGLFreezeProtection->setInterval(15000);
            m_openGLFreezeProtection->setSingleShot(true);
            m_openGLFreezeProtection->start();
            const QString configName = kwinApp()->config()->name();
            m_openGLFreezeProtection->moveToThread(m_openGLFreezeProtectionThread.get());
            connect(
                m_openGLFreezeProtection.get(), &QTimer::timeout, m_openGLFreezeProtection.get(),
                [configName] {
                    auto group = KConfigGroup(KSharedConfig::openConfig(configName), QStringLiteral("Compositing"));
                    group.writeEntry(QLatin1String("LastFailureTimestamp"), QDateTime::currentSecsSinceEpoch());
                    group.sync();
                    KCrash::setDrKonqiEnabled(false);
                    qFatal("Freeze in OpenGL initialization detected");
                },
                Qt::DirectConnection);
        } else {
            Q_ASSERT(m_openGLFreezeProtection);
            QMetaObject::invokeMethod(m_openGLFreezeProtection.get(), QOverload<>::of(&QTimer::start), Qt::QueuedConnection);
        }
        break;
    case OpenGLSafePoint::PostInit:
        group.deleteEntry(QLatin1String("LastFailureTimestamp"));
        group.sync();
        // Deliberately continue with PostFrame
        Q_FALLTHROUGH();
    case OpenGLSafePoint::PostFrame:
        QMetaObject::invokeMethod(m_openGLFreezeProtection.get(), &QTimer::stop, Qt::QueuedConnection);
        break;
    case OpenGLSafePoint::PostLastGuardedFrame:
        m_openGLFreezeProtectionThread->quit();
        m_openGLFreezeProtectionThread->wait();
        m_openGLFreezeProtectionThread.reset();
        m_openGLFreezeProtection.reset();
        break;
    }
}

} // namespace KWin

#include "compositor_x11.moc"
#include "moc_compositor_x11.cpp"
