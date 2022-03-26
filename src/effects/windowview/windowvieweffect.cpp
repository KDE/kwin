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

#include <iostream>

namespace KWin
{

static const QString s_dbusServiceName = QStringLiteral("org.kde.KWin.Effect.WindowView1");
static const QString s_dbusObjectPath = QStringLiteral("/org/kde/KWin/Effect/WindowView1");

WindowViewEffect::WindowViewEffect()
    : QuickSceneEffect("Window View", "windowview")
    , m_exposeAction(new QAction(this))
    , m_exposeAllAction(new QAction(this))
    , m_exposeClassAction(new QAction(this))
{
    qmlRegisterUncreatableType<WindowViewEffect>("org.kde.KWin.Effect.WindowView", 1, 0, "WindowView", QStringLiteral("WindowView cannot be created in QML"));
    initConfig<WindowViewConfig>();
    new WindowView1Adaptor(this);

    QDBusConnection::sessionBus().registerObject(s_dbusObjectPath, this);
    QDBusConnection::sessionBus().registerService(s_dbusServiceName);

    setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/windowview/qml/main.qml"))));

    connect(p_context.get(), &EffectContext::activated, [this]() {
        QuickSceneEffect::activated();
    });
    connect(p_context.get(), &EffectContext::activating, [this](qreal progress) {
        QuickSceneEffect::partialActivate(std::clamp(progress, 0.0, 1.0));
    });
    connect(p_context.get(), &EffectContext::deactivating, [this](qreal progress) {
        QuickSceneEffect::partialActivate(std::clamp(1 - progress, 0.0, 1.0));
    });
    connect(p_context.get(), &EffectContext::deactivated, [this]() {
        QuickSceneEffect::deactivated();
    });

    Parameter windows = Parameter{"Windows Shown", "windows", QMetaType::QString, "ModeAllDesktops"};
    //                      Human readable label                    Values
    windows.possibleValues["Present Windows - Current Desktop"] = "ModeCurrentDesktop";
    windows.possibleValues["Present Windows - All Desktops"] = "ModeAllDesktops";
    windows.possibleValues["Present Windows - Current Application"] = "ModeWindowClass";
    p_context->addActivationParameter(windows);

    connect(p_context.get(), &EffectContext::setActivationParameters, [this](const QHash<QString, QVariant> parameters) {
        // All given parameters are guaranteed to be present in parameters
        QString mode = parameters["windows"].toString();

        std::cout << "PresentWindoes: " << mode.toStdString() << std::endl;
        if (mode == "ModeCurrentDesktop") {
            setMode(PresentWindowsMode::ModeCurrentDesktop);
        } else if (mode == "ModeAllDesktops") {
            setMode(PresentWindowsMode::ModeAllDesktops);
        } else if (mode == "ModeWindowClass") {
            setMode(PresentWindowsMode::ModeWindowClass);
        }
    });

    m_exposeAction->setObjectName(QStringLiteral("Expose"));
    m_exposeAction->setText(i18n("Toggle Present Windows (Current desktop)"));
    KGlobalAccel::self()->setDefaultShortcut(m_exposeAction, QList<QKeySequence>() << (Qt::CTRL | Qt::Key_F9));
    KGlobalAccel::self()->setShortcut(m_exposeAction, QList<QKeySequence>() << (Qt::CTRL | Qt::Key_F9));
    m_shortcut = KGlobalAccel::self()->shortcut(m_exposeAction);
    effects->registerGlobalShortcut(Qt::CTRL | Qt::Key_F9, m_exposeAction);
    connect(m_exposeAction, &QAction::triggered, this, [this]() {
        toggleMode(ModeCurrentDesktop);
    });

    m_exposeAllAction->setObjectName(QStringLiteral("ExposeAll"));
    m_exposeAllAction->setText(i18n("Toggle Present Windows (All desktops)"));
    KGlobalAccel::self()->setDefaultShortcut(m_exposeAllAction, QList<QKeySequence>() << (Qt::CTRL | Qt::Key_F10) << Qt::Key_LaunchC);
    KGlobalAccel::self()->setShortcut(m_exposeAllAction, QList<QKeySequence>() << (Qt::CTRL | Qt::Key_F10) << Qt::Key_LaunchC);
    m_shortcutAll = KGlobalAccel::self()->shortcut(m_exposeAllAction);
    effects->registerGlobalShortcut(Qt::CTRL + Qt::Key_F10, m_exposeAllAction);
    connect(m_exposeAllAction, &QAction::triggered, this, [this]() {
        toggleMode(ModeAllDesktops);
    });

    m_exposeClassAction->setObjectName(QStringLiteral("ExposeClass"));
    m_exposeClassAction->setText(i18n("Toggle Present Windows (Window class)"));
    KGlobalAccel::self()->setDefaultShortcut(m_exposeClassAction, QList<QKeySequence>() << (Qt::CTRL | Qt::Key_F7));
    KGlobalAccel::self()->setShortcut(m_exposeClassAction, QList<QKeySequence>() << (Qt::CTRL | Qt::Key_F7));
    effects->registerGlobalShortcut(Qt::CTRL | Qt::Key_F7, m_exposeClassAction);
    connect(m_exposeClassAction, &QAction::triggered, this, [this]() {
        toggleMode(ModeWindowClass);
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
        }
    });

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
    setLayout(WindowViewConfig::layoutMode());

    for (ElectricBorder border : qAsConst(m_borderActivate)) {
        effects->unreserveElectricBorder(border, this);
    }
    for (ElectricBorder border : qAsConst(m_borderActivateAll)) {
        effects->unreserveElectricBorder(border, this);
    }
    for (ElectricBorder border : qAsConst(m_borderActivateClass)) {
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
        } else if (e->key() == Qt::Key_Escape) {
            p_context->ungrabActive();
        }
    }
    QuickSceneEffect::grabbedKeyboardEvent(e);
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
        m_searchText = "";
        p_context->grabActive();
    }
}

void WindowViewEffect::activated()
{
    m_searchText = "";
    p_context->grabActive();
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
    if (state() == EffectContext::State::Inactive || state() == EffectContext::State::Deactivating) {
        setMode(mode);
        p_context->grabActive();
    } else {
        if (m_mode != mode) {
            setMode(mode);
        } else {
            p_context->ungrabActive();
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
    } else {
        return false;
    }

    p_context->grabActive();
    return true;
}

} // namespace KWin
