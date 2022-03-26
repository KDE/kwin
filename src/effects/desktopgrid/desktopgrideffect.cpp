/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "desktopgrideffect.h"
#include "desktopgridconfig.h"

#include <QAction>
#include <QQuickItem>
#include <QTimer>
#include <cmath>

#include <KGlobalAccel>
#include <KLocalizedString>

namespace KWin
{
DesktopGridEffect::DesktopGridEffect()
    : QuickSceneEffect("Desktop Grid", "desktopgrid")
    , m_toggleAction(new QAction(this))
{
    qmlRegisterUncreatableType<DesktopGridEffect>("org.kde.kwin.private.desktopgrid", 1, 0, "DesktopGridEffect", QStringLiteral("Cannot create elements of type DesktopGridEffect"));

    connect(effects, &EffectsHandler::desktopGridWidthChanged, this, &DesktopGridEffect::gridColumnsChanged);
    connect(effects, &EffectsHandler::desktopGridHeightChanged, this, &DesktopGridEffect::gridRowsChanged);

    m_toggleAction->setObjectName(QStringLiteral("ShowDesktopGrid"));
    m_toggleAction->setText(i18n("Show Desktop Grid"));
    KGlobalAccel::self()->setDefaultShortcut(m_toggleAction, QList<QKeySequence>() << (Qt::META | Qt::Key_F8));
    KGlobalAccel::self()->setShortcut(m_toggleAction, QList<QKeySequence>() << (Qt::META | Qt::Key_F8));
    m_toggleShortcut = KGlobalAccel::self()->shortcut(m_toggleAction);
    effects->registerGlobalShortcut(Qt::META | Qt::Key_F8, m_toggleAction);
    connect(m_toggleAction, &QAction::triggered, this, &DesktopGridEffect::toggle);

    connect(p_context.get(), &EffectContext::activated, [this]() {
        activated();
    });
    connect(p_context.get(), &EffectContext::activating, [this](qreal progress) {
        partialActivate(std::clamp(progress, 0.0, 1.0));
    });
    connect(p_context.get(), &EffectContext::deactivating, [this](qreal progress) {
        partialActivate(std::clamp(1 - progress, 0.0, 1.0));
    });
    connect(p_context.get(), &EffectContext::deactivated, [this]() {
        deactivated();
    });

    initConfig<DesktopGridConfig>();
    reconfigure(ReconfigureAll);

    setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/desktopgrid/qml/main.qml"))));
}

DesktopGridEffect::~DesktopGridEffect()
{
}

QVariantMap DesktopGridEffect::initialProperties(EffectScreen *screen)
{
    return QVariantMap{
        {QStringLiteral("effect"), QVariant::fromValue(this)},
        {QStringLiteral("targetScreen"), QVariant::fromValue(screen)},
    };
}

void DesktopGridEffect::reconfigure(ReconfigureFlags)
{
    DesktopGridConfig::self()->read();
    setLayout(DesktopGridConfig::layoutMode());

    for (const ElectricBorder &border : qAsConst(m_borderActivate)) {
        effects->unreserveElectricBorder(border, this);
    }

    m_borderActivate.clear();

    const QList<int> activateBorders = DesktopGridConfig::borderActivate();
    for (const int &border : activateBorders) {
        m_borderActivate.append(ElectricBorder(border));
        effects->reserveElectricBorder(ElectricBorder(border), this);
    }

    Q_EMIT showAddRemoveChanged();
    Q_EMIT desktopNameAlignmentChanged();
    Q_EMIT desktopLayoutModeChanged();
    Q_EMIT customLayoutRowsChanged();
}

int DesktopGridEffect::requestedEffectChainPosition() const
{
    return 70;
}

bool DesktopGridEffect::borderActivated(ElectricBorder border)
{
    if (m_borderActivate.contains(border)) {
        toggle();
        return true;
    }
    return false;
}

void DesktopGridEffect::grabbedKeyboardEvent(QKeyEvent *keyEvent)
{
    if (m_toggleShortcut.contains(keyEvent->key() | keyEvent->modifiers())) {
        if (keyEvent->type() == QEvent::KeyPress) {
            toggle();
        }
        return;
    }
    QuickSceneEffect::grabbedKeyboardEvent(keyEvent);
}

Qt::AlignmentFlag DesktopGridEffect::desktopNameAlignment() const
{
    return Qt::AlignmentFlag(DesktopGridConfig::desktopNameAlignment());
}

DesktopGridEffect::DesktopLayoutMode DesktopGridEffect::desktopLayoutMode() const
{
    return DesktopGridEffect::DesktopLayoutMode(DesktopGridConfig::desktopLayoutMode());
}

int DesktopGridEffect::customLayoutRows() const
{
    return DesktopGridConfig::customLayoutRows();
}

void DesktopGridEffect::addDesktop() const
{
    effects->setNumberOfDesktops(effects->numberOfDesktops() + 1);
}

void DesktopGridEffect::removeDesktop() const
{
    effects->setNumberOfDesktops(effects->numberOfDesktops() - 1);
}

void DesktopGridEffect::swapDesktops(int from, int to)
{
    QList<EffectWindow *> fromList;
    QList<EffectWindow *> toList;
    for (auto *w : effects->stackingOrder()) {
        if (!w->isNormalWindow()) {
            continue;
        }
        if (w->isOnDesktop(from)) {
            fromList << w;
        } else if (w->isOnDesktop(to)) {
            toList << w;
        }
    }
    for (auto *w : fromList) {
        effects->windowToDesktop(w, to);
    }
    for (auto *w : toList) {
        effects->windowToDesktop(w, from);
    }
}

int DesktopGridEffect::gridRows() const
{
    switch (desktopLayoutMode()) {
    case DesktopLayoutMode::LayoutAutomatic:
        return ceil(sqrt(effects->numberOfDesktops()));
    case DesktopLayoutMode::LayoutCustom:
        return qBound(1, customLayoutRows(), effects->numberOfDesktops());
    case DesktopLayoutMode::LayoutPager:
    default:
        return effects->desktopGridSize().height();
    }
}

int DesktopGridEffect::gridColumns() const
{
    switch (desktopLayoutMode()) {
    case DesktopLayoutMode::LayoutAutomatic:
        return ceil(sqrt(effects->numberOfDesktops()));
    case DesktopLayoutMode::LayoutCustom:
        return qMax(1.0, ceil(qreal(effects->numberOfDesktops()) / customLayoutRows()));
    case DesktopLayoutMode::LayoutPager:
    default:
        return effects->desktopGridSize().width();
    }
}

bool DesktopGridEffect::showAddRemove() const
{
    return DesktopGridConfig::showAddRemove();
}

int DesktopGridEffect::layout() const
{
    return m_layout;
}

void DesktopGridEffect::setLayout(int layout)
{
    if (m_layout != layout) {
        m_layout = layout;
        Q_EMIT layoutChanged();
    }
}

void DesktopGridEffect::toggle()
{
    if (state() == EffectContext::State::Active || state() == EffectContext::State::Activating) {
        p_context->ungrabActive();
    } else {
        p_context->grabActive();
    }
}

} // namespace KWin
