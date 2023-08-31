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
#include "core/renderlayerdelegate.h"
#include "ftrace.h"
#include "options.h"
#include "scene/surfaceitem_x11.h"
#include "scene/workspacescene.h"
#include "utils/common.h"
#include "utils/xcbutils.h"
#include "workspace.h"
#include "x11syncmanager.h"
#include "x11window.h"

#include <KCrash>
#include <KGlobalAccel>
#include <KLocalizedString>
#if KWIN_BUILD_NOTIFICATIONS
#include <KNotification>
#endif

#include <QAction>
#include <QOpenGLContext>
#include <QThread>

Q_DECLARE_METATYPE(KWin::X11Compositor::SuspendReason)

namespace KWin
{

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
}

X11Compositor::~X11Compositor()
{
    Q_EMIT aboutToDestroy();
    if (m_openGLFreezeProtectionThread) {
        m_openGLFreezeProtectionThread->quit();
        m_openGLFreezeProtectionThread->wait();
    }
    stop(); // this can't be called in the destructor of Compositor
}

X11SyncManager *X11Compositor::syncManager() const
{
    return m_syncManager.get();
}

void X11Compositor::toggleCompositing()
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
    Compositor::reinitialize();
}

void X11Compositor::configChanged()
{
    if (m_suspended) {
        stop();
        return;
    }
    Compositor::configChanged();
}

void X11Compositor::suspend(X11Compositor::SuspendReason reason)
{
    Q_ASSERT(reason != NoReasonSuspend);
    m_suspended |= reason;

    if (reason & ScriptSuspend) {
        // When disabled show a shortcut how the user can get back compositing.
        const auto shortcuts = KGlobalAccel::self()->shortcut(workspace()->findChild<QAction *>(QStringLiteral("Suspend Compositing")));
        if (!shortcuts.isEmpty()) {
            // Display notification only if there is the shortcut.
            const QString message =
                i18n("Desktop effects have been suspended by another application.<br/>"
                     "You can resume using the '%1' shortcut.",
                     shortcuts.first().toString(QKeySequence::NativeText));
#if KWIN_BUILD_NOTIFICATIONS
            KNotification::event(QStringLiteral("compositingsuspendeddbus"), message);
#endif
        }
    }
    stop();
}

void X11Compositor::resume(X11Compositor::SuspendReason reason)
{
    Q_ASSERT(reason != NoReasonSuspend);
    m_suspended &= ~reason;
    start();
}

void X11Compositor::start()
{
    if (kwinApp()->isTerminating()) {
        // Don't start while KWin is terminating. An event to restart might be lingering
        // in the event queue due to graphics reset.
        return;
    }

    if (m_state != State::Off) {
        return;
    }

    if (m_suspended) {
        QStringList reasons;
        if (m_suspended & UserSuspend) {
            reasons << QStringLiteral("Disabled by User");
        }
        if (m_suspended & BlockRuleSuspend) {
            reasons << QStringLiteral("Disabled by Window");
        }
        if (m_suspended & ScriptSuspend) {
            reasons << QStringLiteral("Disabled by Script");
        }
        qCInfo(KWIN_CORE) << "Compositing is suspended, reason:" << reasons;
        return;
    } else if (!compositingPossible()) {
        qCWarning(KWIN_CORE) << "Compositing is not possible";
        return;
    }

    m_state = State::Starting;

    options->reloadCompositingSettings(true);

    initializeX11();

    Q_EMIT aboutToToggleCompositing();

    const QVector<CompositingType> availableCompositors = kwinApp()->outputBackend()->supportedCompositors();
    QVector<CompositingType> candidateCompositors;

    // If compositing has been restarted, try to use the last used compositing type.
    if (m_selectedCompositor != NoCompositing) {
        candidateCompositors.append(m_selectedCompositor);
    } else {
        candidateCompositors = availableCompositors;

        const auto userConfigIt = std::find(candidateCompositors.begin(), candidateCompositors.end(), options->compositingMode());
        if (userConfigIt != candidateCompositors.end()) {
            candidateCompositors.erase(userConfigIt);
            candidateCompositors.prepend(options->compositingMode());
        } else {
            qCWarning(KWIN_CORE) << "Configured compositor not supported by Platform. Falling back to defaults";
        }
    }

    for (auto type : std::as_const(candidateCompositors)) {
        bool stop = false;
        switch (type) {
        case OpenGLCompositing:
            qCDebug(KWIN_CORE) << "Attempting to load the OpenGL scene";
            stop = attemptOpenGLCompositing();
            break;
        case QPainterCompositing:
            qCDebug(KWIN_CORE) << "Attempting to load the QPainter scene";
            stop = attemptQPainterCompositing();
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

        xcb_composite_unredirect_subwindows(kwinApp()->x11Connection(), kwinApp()->x11RootWindow(), XCB_COMPOSITE_REDIRECT_MANUAL);
        cleanupX11();
        return;
    }

    m_selectedCompositor = m_backend->compositingType();
    Q_EMIT sceneCreated();

    Q_ASSERT(m_scene);
    m_scene->initialize();

    const QList<Output *> outputs = workspace()->outputs();
    auto workspaceLayer = new RenderLayer(outputs.constFirst()->renderLoop());
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

    Q_EMIT compositingToggled(true);

    if (m_releaseSelectionTimer.isActive()) {
        m_releaseSelectionTimer.stop();
    }

    m_syncManager.reset(X11SyncManager::create());
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
        if (auto *con = kwinApp()->x11Connection()) {
            xcb_composite_unredirect_subwindows(con, kwinApp()->x11RootWindow(), XCB_COMPOSITE_REDIRECT_MANUAL);
        }
    }

    const auto superlayers = m_superlayers;
    for (auto it = superlayers.begin(); it != superlayers.end(); ++it) {
        removeSuperLayer(*it);
    }

    m_scene.reset();
    m_backend.reset();
    m_syncManager.reset();

    m_state = State::Off;
    Q_EMIT compositingToggled(false);
}

void X11Compositor::composite(RenderLoop *renderLoop)
{
    if (backend()->overlayWindow() && !isOverlayWindowVisible()) {
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

    if (m_backend->checkGraphicsReset()) {
        qCDebug(KWIN_CORE) << "Graphics reset occurred";
#if KWIN_BUILD_NOTIFICATIONS
        KNotification::event(QStringLiteral("graphicsreset"), i18n("Desktop effects were restarted due to a graphics reset"));
#endif
        reinitialize();
    } else {
        Output *output = findOutput(renderLoop);
        OutputLayer *primaryLayer = m_backend->primaryLayer(output);
        fTraceDuration("Paint (", output->name(), ")");

        RenderLayer *superLayer = m_superlayers[renderLoop];
        superLayer->setOutputLayer(primaryLayer);

        renderLoop->prepareNewFrame();

        if (superLayer->needsRepaint()) {
            renderLoop->beginPaint();
            superLayer->delegate()->prePaint();

            QRegion surfaceDamage = primaryLayer->repaints();
            primaryLayer->resetRepaints();

            surfaceDamage += superLayer->repaints() + superLayer->delegate()->repaints();
            superLayer->resetRepaints();

            if (auto beginInfo = primaryLayer->beginFrame()) {
                auto &[renderTarget, repaint] = beginInfo.value();

                const QRegion bufferDamage = surfaceDamage.united(repaint).intersected(superLayer->rect().toAlignedRect());
                superLayer->delegate()->paint(renderTarget, bufferDamage);

                primaryLayer->endFrame(bufferDamage, surfaceDamage);
            }

            superLayer->delegate()->postPaint();
        }

        m_backend->present(output);
    }

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

bool X11Compositor::checkForOverlayWindow(WId w) const
{
    if (!backend()) {
        // No backend, so it cannot be the overlay window.
        return false;
    }
    if (!backend()->overlayWindow()) {
        // No overlay window, it cannot be the overlay.
        return false;
    }
    // Compare the window ID's.
    return w == backend()->overlayWindow()->window();
}

bool X11Compositor::isOverlayWindowVisible() const
{
    if (!backend()) {
        return false;
    }
    if (!backend()->overlayWindow()) {
        return false;
    }
    return backend()->overlayWindow()->isVisible();
}

void X11Compositor::updateClientCompositeBlocking(X11Window *c)
{
    if (c) {
        if (c->isBlockingCompositing()) {
            // Do NOT attempt to call suspend(true) from within the eventchain!
            if (!(m_suspended & BlockRuleSuspend)) {
                QMetaObject::invokeMethod(
                    this, [this]() {
                        suspend(BlockRuleSuspend);
                    },
                    Qt::QueuedConnection);
            }
        }
    } else if (m_suspended & BlockRuleSuspend) {
        // If !c we just check if we can resume in case a blocking client was lost.
        bool shouldResume = true;

        const auto windows = workspace()->windows();
        for (Window *window : windows) {
            X11Window *x11Window = qobject_cast<X11Window *>(window);
            if (x11Window && x11Window->isBlockingCompositing()) {
                shouldResume = false;
                break;
            }
        }
        if (shouldResume) {
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
    auto timestamp = KConfigGroup(kwinApp()->config(), "Compositing").readEntry(QLatin1String("LastFailureTimestamp"), 0);
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
    KConfigGroup gl_workaround_group(kwinApp()->config(), "Compositing");
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
    KConfigGroup gl_workaround_group(kwinApp()->config(), "Compositing");
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
    auto group = KConfigGroup(kwinApp()->config(), "Compositing");
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
                    auto group = KConfigGroup(KSharedConfig::openConfig(configName), "Compositing");
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

#include "moc_compositor_x11.cpp"
