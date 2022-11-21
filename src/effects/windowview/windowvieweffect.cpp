/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowvieweffect.h"
#include "windowview1adaptor.h"
#include "windowviewconfig.h"

#include <QAction>
#include <QQuickItem>
#include <QTimer>

#include <KGlobalAccel>
#include <KLocalizedString>

namespace KWin
{

static const QString s_dbusServiceName = QStringLiteral("org.kde.KWin.Effect.WindowView1");
static const QString s_dbusObjectPath = QStringLiteral("/org/kde/KWin/Effect/WindowView1");

WindowViewEffect::WindowViewEffect()
    : m_shutdownTimer(new QTimer(this))
    , m_exposeAction(new QAction(this))
    , m_exposeAllAction(new QAction(this))
    , m_exposeClassAction(new QAction(this))
    , m_exposeClassCurrentDesktopAction(new QAction(this))
{
    qmlRegisterUncreatableType<WindowViewEffect>("org.kde.KWin.Effect.WindowView", 1, 0, "WindowView", QStringLiteral("WindowView cannot be created in QML"));
    initConfig<WindowViewConfig>();
    new WindowView1Adaptor(this);

    QDBusConnection::sessionBus().registerObject(s_dbusObjectPath, this);
    QDBusConnection::sessionBus().registerService(s_dbusServiceName);

    m_shutdownTimer->setSingleShot(true);
    connect(m_shutdownTimer, &QTimer::timeout, this, &WindowViewEffect::realDeactivate);
    connect(effects, &EffectsHandler::screenAboutToLock, this, &WindowViewEffect::realDeactivate);

    setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/windowview/qml/main.qml"))));

    m_exposeAction->setObjectName(QStringLiteral("Expose"));
    m_exposeAction->setText(i18n("Toggle Present Windows (Current desktop)"));
    KGlobalAccel::self()->setDefaultShortcut(m_exposeAction, QList<QKeySequence>() << (Qt::CTRL | Qt::Key_F9));
    KGlobalAccel::self()->setShortcut(m_exposeAction, QList<QKeySequence>() << (Qt::CTRL | Qt::Key_F9));
    m_shortcut = KGlobalAccel::self()->shortcut(m_exposeAction);
    connect(m_exposeAction, &QAction::triggered, this, [this]() {
        toggleMode(ModeCurrentDesktop);
    });

    m_exposeAllAction->setObjectName(QStringLiteral("ExposeAll"));
    m_exposeAllAction->setText(i18n("Toggle Present Windows (All desktops)"));
    KGlobalAccel::self()->setDefaultShortcut(m_exposeAllAction, QList<QKeySequence>() << (Qt::CTRL | Qt::Key_F10) << Qt::Key_LaunchC);
    KGlobalAccel::self()->setShortcut(m_exposeAllAction, QList<QKeySequence>() << (Qt::CTRL | Qt::Key_F10) << Qt::Key_LaunchC);
    m_shortcutAll = KGlobalAccel::self()->shortcut(m_exposeAllAction);
    connect(m_exposeAllAction, &QAction::triggered, this, [this]() {
        toggleMode(ModeAllDesktops);
    });

    m_exposeClassAction->setObjectName(QStringLiteral("ExposeClass"));
    m_exposeClassAction->setText(i18n("Toggle Present Windows (Window class)"));
    KGlobalAccel::self()->setDefaultShortcut(m_exposeClassAction, QList<QKeySequence>() << (Qt::CTRL | Qt::Key_F7));
    KGlobalAccel::self()->setShortcut(m_exposeClassAction, QList<QKeySequence>() << (Qt::CTRL | Qt::Key_F7));
    m_shortcutClass = KGlobalAccel::self()->shortcut(m_exposeClassAction);
    connect(m_exposeClassAction, &QAction::triggered, this, [this]() {
        toggleMode(ModeWindowClass);
    });

    m_exposeClassCurrentDesktopAction->setObjectName(QStringLiteral("ExposeClassCurrentDesktop"));
    m_exposeClassCurrentDesktopAction->setText(i18n("Toggle Present Windows (Window class on current desktop)"));
    KGlobalAccel::self()->setDefaultShortcut(m_exposeClassCurrentDesktopAction, QList<QKeySequence>()); // no default shortcut
    KGlobalAccel::self()->setShortcut(m_exposeClassCurrentDesktopAction, QList<QKeySequence>());
    m_shortcutClassCurrentDesktop = KGlobalAccel::self()->shortcut(m_exposeClassCurrentDesktopAction);
    connect(m_exposeClassCurrentDesktopAction, &QAction::triggered, this, [this]() {
        toggleMode(ModeWindowClassCurrentDesktop);
    });

    connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, this, [this](QAction *action, const QKeySequence &seq) {
        if (action->objectName() == QStringLiteral("Expose")) {
            m_shortcut.clear();
            m_shortcut.append(seq);
        } else if (action->objectName() == QStringLiteral("ExposeAll")) {
            m_shortcutAll.clear();
            m_shortcutAll.append(seq);
        } else if (action->objectName() == QStringLiteral("ExposeClass")) {
            m_shortcutClass.clear();
            m_shortcutClass.append(seq);
        } else if (action->objectName() == QStringLiteral("ExposeClassCurrentDesktop")) {
            m_shortcutClassCurrentDesktop.clear();
            m_shortcutClassCurrentDesktop.append(seq);
        }
    });

    m_realtimeToggleAction = new QAction(this);
    connect(m_realtimeToggleAction, &QAction::triggered, this, [this]() {
        if (m_status == Status::Deactivating) {
            if (m_partialActivationFactor < 0.5) {
                deactivate(animationDuration());
            } else {
                cancelPartialDeactivate();
            }
        } else if (m_status == Status::Activating) {
            if (m_partialActivationFactor > 0.5) {
                activate();
            } else {
                cancelPartialActivate();
            }
        }
    });

    const auto gestureCallback = [this](qreal progress) {
        if (!effects->hasActiveFullScreenEffect() || effects->activeFullScreenEffect() == this) {
            switch (m_status) {
            case Status::Inactive:
            case Status::Activating:
                setMode(ModeAllDesktops);
                partialActivate(progress);
                break;
            case Status::Active:
            case Status::Deactivating:
                partialDeactivate(progress);
                break;
            }
        }
    };
    effects->registerRealtimeTouchpadSwipeShortcut(SwipeDirection::Down, 4, m_realtimeToggleAction, gestureCallback);
    effects->registerTouchscreenSwipeShortcut(SwipeDirection::Down, 3, m_realtimeToggleAction, gestureCallback);

    reconfigure(ReconfigureAll);
}

WindowViewEffect::~WindowViewEffect()
{
    QDBusConnection::sessionBus().unregisterService(s_dbusServiceName);
    QDBusConnection::sessionBus().unregisterObject(s_dbusObjectPath);
}

QVariantMap WindowViewEffect::initialProperties(EffectScreen *screen)
{
    return QVariantMap{
        {QStringLiteral("effect"), QVariant::fromValue(this)},
        {QStringLiteral("targetScreen"), QVariant::fromValue(screen)},
        {QStringLiteral("selectedIds"), QVariant::fromValue(m_windowIds)},
    };
}

int WindowViewEffect::animationDuration() const
{
    return m_animationDuration;
}

void WindowViewEffect::setAnimationDuration(int duration)
{
    if (m_animationDuration != duration) {
        m_animationDuration = duration;
        Q_EMIT animationDurationChanged();
    }
}

int WindowViewEffect::layout() const
{
    return m_layout;
}

void WindowViewEffect::setLayout(int layout)
{
    if (m_layout != layout) {
        m_layout = layout;
        Q_EMIT layoutChanged();
    }
}

bool WindowViewEffect::ignoreMinimized() const
{
    return WindowViewConfig::ignoreMinimized();
}

int WindowViewEffect::requestedEffectChainPosition() const
{
    return 70;
}

void WindowViewEffect::reconfigure(ReconfigureFlags)
{
    WindowViewConfig::self()->read();
    setAnimationDuration(animationTime(300));
    setLayout(WindowViewConfig::layoutMode());

    for (ElectricBorder border : std::as_const(m_borderActivate)) {
        effects->unreserveElectricBorder(border, this);
    }
    for (ElectricBorder border : std::as_const(m_borderActivateAll)) {
        effects->unreserveElectricBorder(border, this);
    }

    m_borderActivate.clear();
    m_borderActivateAll.clear();
    m_borderActivateClass.clear();

    const auto borderActivate = WindowViewConfig::borderActivate();
    for (int i : borderActivate) {
        m_borderActivate.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i), this);
    }
    const auto activateAll = WindowViewConfig::borderActivateAll();
    for (int i : activateAll) {
        m_borderActivateAll.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i), this);
    }
    const auto activateClass = WindowViewConfig::borderActivateClass();
    for (int i : activateClass) {
        m_borderActivateClass.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i), this);
    }
    const auto activateClassCurrentDesktop = WindowViewConfig::borderActivateClassCurrentDesktop();
    for (int i : activateClassCurrentDesktop) {
        m_borderActivateClassCurrentDesktop.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i), this);
    }

    auto touchCallback = [this](ElectricBorder border, const QPointF &deltaProgress, const EffectScreen *screen) {
        if (m_status == Status::Active) {
            return;
        }
        if (m_touchBorderActivate.contains(border)) {
            setMode(ModeCurrentDesktop);
        } else if (m_touchBorderActivateAll.contains(border)) {
            setMode(ModeAllDesktops);
        } else if (m_touchBorderActivateClass.contains(border)) {
            setMode(ModeWindowClass);
        } else if (m_touchBorderActivateClassCurrentDesktop.contains(border)) {
            setMode(ModeWindowClassCurrentDesktop);
        }
        const int maxDelta = 500; // Arbitrary logical pixels value seems to behave better than scaledScreenSize
        if (border == ElectricTop || border == ElectricBottom) {
            partialActivate(std::min(1.0, std::abs(deltaProgress.y()) / maxDelta));
        } else {
            partialActivate(std::min(1.0, std::abs(deltaProgress.x()) / maxDelta));
        }
    };

    QList<int> touchActivateBorders = WindowViewConfig::touchBorderActivate();
    for (const int &border : touchActivateBorders) {
        m_touchBorderActivate.append(ElectricBorder(border));
        effects->registerRealtimeTouchBorder(ElectricBorder(border), m_realtimeToggleAction, touchCallback);
    }
    touchActivateBorders = WindowViewConfig::touchBorderActivateAll();
    for (const int &border : touchActivateBorders) {
        m_touchBorderActivateAll.append(ElectricBorder(border));
        effects->registerRealtimeTouchBorder(ElectricBorder(border), m_realtimeToggleAction, touchCallback);
    }
    touchActivateBorders = WindowViewConfig::touchBorderActivateClass();
    for (const int &border : touchActivateBorders) {
        m_touchBorderActivateClass.append(ElectricBorder(border));
        effects->registerRealtimeTouchBorder(ElectricBorder(border), m_realtimeToggleAction, touchCallback);
    }
    touchActivateBorders = WindowViewConfig::touchBorderActivateClassCurrentDesktop();
    for (const int &border : touchActivateBorders) {
        m_touchBorderActivateClassCurrentDesktop.append(ElectricBorder(border));
        effects->registerRealtimeTouchBorder(ElectricBorder(border), m_realtimeToggleAction, touchCallback);
    }
}

void WindowViewEffect::grabbedKeyboardEvent(QKeyEvent *e)
{
    if (e->type() == QEvent::KeyPress) {
        // check for global shortcuts
        // HACK: keyboard grab disables the global shortcuts so we have to check for global shortcut (bug 156155)
        if (m_mode == ModeCurrentDesktop && m_shortcut.contains(e->key() | e->modifiers())) {
            toggleMode(ModeCurrentDesktop);
            return;
        } else if (m_mode == ModeAllDesktops && m_shortcutAll.contains(e->key() | e->modifiers())) {
            toggleMode(ModeAllDesktops);
            return;
        } else if (m_mode == ModeWindowClass && m_shortcutClass.contains(e->key() | e->modifiers())) {
            toggleMode(ModeWindowClass);
            return;
        } else if (m_mode == ModeWindowClassCurrentDesktop && m_shortcutClassCurrentDesktop.contains(e->key() | e->modifiers())) {
            toggleMode(ModeWindowClassCurrentDesktop);
            return;
        } else if (e->key() == Qt::Key_Escape) {
            deactivate(animationDuration());
        }
    }
    QuickSceneEffect::grabbedKeyboardEvent(e);
}

qreal WindowViewEffect::partialActivationFactor() const
{
    return m_partialActivationFactor;
}

void WindowViewEffect::setPartialActivationFactor(qreal factor)
{
    if (m_partialActivationFactor != factor) {
        m_partialActivationFactor = factor;
        Q_EMIT partialActivationFactorChanged();
    }
}

bool WindowViewEffect::gestureInProgress() const
{
    return m_gestureInProgress;
}

void WindowViewEffect::setGestureInProgress(bool gesture)
{
    if (m_gestureInProgress != gesture) {
        m_gestureInProgress = gesture;
        Q_EMIT gestureInProgressChanged();
    }
}

void WindowViewEffect::activate(const QStringList &windowIds)
{
    setMode(ModeWindowGroup);
    QList<QUuid> internalIds;
    internalIds.reserve(windowIds.count());
    for (const QString &windowId : windowIds) {
        if (const auto window = effects->findWindow(QUuid(windowId))) {
            internalIds.append(window->internalId());
            continue;
        }

        // On X11, the task manager can pass a list with X11 ids.
        bool ok;
        if (const long legacyId = windowId.toLong(&ok); ok) {
            if (const auto window = effects->findWindow(legacyId)) {
                internalIds.append(window->internalId());
            }
        }
    }
    if (!internalIds.isEmpty()) {
        m_windowIds = internalIds;
        m_searchText = QString();
        setRunning(true);
    }
}

void WindowViewEffect::activate()
{
    if (effects->isScreenLocked()) {
        return;
    }

    m_status = Status::Active;
    m_windowIds.clear();

    setGestureInProgress(false);
    setPartialActivationFactor(0);

    // This one should be the last.
    m_searchText = QString();
    setRunning(true);
}

void WindowViewEffect::partialActivate(qreal factor)
{
    if (effects->isScreenLocked()) {
        return;
    }

    m_status = Status::Activating;

    setPartialActivationFactor(factor);
    setGestureInProgress(true);

    // This one should be the last.
    m_searchText = QString();
    setRunning(true);
}

void WindowViewEffect::cancelPartialActivate()
{
    deactivate(animationDuration());
}

void WindowViewEffect::deactivate(int timeout)
{
    const auto screens = effects->screens();
    for (const auto screen : screens) {
        if (QuickSceneView *view = viewForScreen(screen)) {
            QMetaObject::invokeMethod(view->rootItem(), "stop");
        }
    }
    m_shutdownTimer->start(timeout);

    setGestureInProgress(false);
    setPartialActivationFactor(0.0);
}

void WindowViewEffect::partialDeactivate(qreal factor)
{
    m_status = Status::Deactivating;

    setPartialActivationFactor(1.0 - factor);
    setGestureInProgress(true);
}

void WindowViewEffect::cancelPartialDeactivate()
{
    activate();
}

void WindowViewEffect::realDeactivate()
{
    setRunning(false);
    m_status = Status::Inactive;
}

void WindowViewEffect::setMode(WindowViewEffect::PresentWindowsMode mode)
{
    if (mode == m_mode) {
        return;
    }

    if (mode != ModeWindowGroup) {
        m_windowIds.clear();
    }

    m_mode = mode;
    Q_EMIT modeChanged();
}

void WindowViewEffect::toggleMode(PresentWindowsMode mode)
{
    if (!isRunning()) {
        setMode(mode);
        activate();
    } else {
        if (m_mode != mode) {
            setMode(mode);
        } else {
            deactivate(animationDuration());
        }
    }
}

WindowViewEffect::PresentWindowsMode WindowViewEffect::mode() const
{
    return m_mode;
}

bool WindowViewEffect::borderActivated(ElectricBorder border)
{
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this) {
        return true;
    }

    if (m_borderActivate.contains(border)) {
        toggleMode(ModeCurrentDesktop);
    } else if (m_borderActivateAll.contains(border)) {
        toggleMode(ModeAllDesktops);
    } else if (m_borderActivateClass.contains(border)) {
        toggleMode(ModeWindowClass);
    } else if (m_touchBorderActivateClassCurrentDesktop.contains(border)) {
        toggleMode(ModeWindowClassCurrentDesktop);
    } else {
        return false;
    }

    return true;
}

} // namespace KWin
