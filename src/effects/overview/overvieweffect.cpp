/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "overvieweffect.h"
#include "overviewconfig.h"

#include <KGlobalAccel>
#include <KLocalizedString>

#include <QAction>
#include <QDebug>
#include <QQuickItem>
#include <QTimer>

#include <algorithm>

namespace KWin
{
OverviewEffect::OverviewEffect()
    : QuickSceneEffect("Overview", "overview")
{
    const QKeySequence defaultToggleShortcut = Qt::META | Qt::Key_W;
    m_toggleAction = new QAction(this);
    connect(m_toggleAction, &QAction::triggered, this, &OverviewEffect::toggle);
    m_toggleAction->setObjectName(QStringLiteral("Overview"));
    m_toggleAction->setText(i18n("Toggle Overview"));
    KGlobalAccel::self()->setDefaultShortcut(m_toggleAction, {defaultToggleShortcut});
    KGlobalAccel::self()->setShortcut(m_toggleAction, {defaultToggleShortcut});
    m_toggleShortcut = KGlobalAccel::self()->shortcut(m_toggleAction);
    effects->registerGlobalShortcut({defaultToggleShortcut}, m_toggleAction);

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

    initConfig<OverviewConfig>();
    reconfigure(ReconfigureAll);

    setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/overview/qml/ScreenView.qml"))));
}

OverviewEffect::~OverviewEffect()
{
}

QVariantMap OverviewEffect::initialProperties(EffectScreen *screen)
{
    return QVariantMap{
        {QStringLiteral("effect"), QVariant::fromValue(this)},
        {QStringLiteral("targetScreen"), QVariant::fromValue(screen)},
    };
}

void OverviewEffect::reconfigure(ReconfigureFlags)
{
    OverviewConfig::self()->read();
    setLayout(OverviewConfig::layoutMode());
    setBlurBackground(OverviewConfig::blurBackground());

    for (const ElectricBorder &border : qAsConst(m_borderActivate)) {
        effects->unreserveElectricBorder(border, this);
    }

    m_borderActivate.clear();

    const QList<int> activateBorders = OverviewConfig::borderActivate();
    for (const int &border : activateBorders) {
        m_borderActivate.append(ElectricBorder(border));
        effects->reserveElectricBorder(ElectricBorder(border), this);
    }
}

int OverviewEffect::layout() const
{
    return m_layout;
}

bool OverviewEffect::ignoreMinimized() const
{
    return OverviewConfig::ignoreMinimized();
}

void OverviewEffect::setLayout(int layout)
{
    if (m_layout != layout) {
        m_layout = layout;
        Q_EMIT layoutChanged();
    }
}

bool OverviewEffect::blurBackground() const
{
    return m_blurBackground;
}

void OverviewEffect::setBlurBackground(bool blur)
{
    if (m_blurBackground != blur) {
        m_blurBackground = blur;
        Q_EMIT blurBackgroundChanged();
    }
}

int OverviewEffect::requestedEffectChainPosition() const
{
    return 70;
}

bool OverviewEffect::borderActivated(ElectricBorder border)
{
    if (m_borderActivate.contains(border)) {
        toggle();
        return true;
    }
    return false;
}

void OverviewEffect::toggle()
{
    if (state() == EffectContext::State::Active || state() == EffectContext::State::Activating) {
        p_context->ungrabActive();
    } else {
        p_context->grabActive();
    }

    p_partialActivationFactor = 0;
    Q_EMIT gestureInProgressChanged();
    Q_EMIT partialActivationFactorChanged();
}

void OverviewEffect::grabbedKeyboardEvent(QKeyEvent *keyEvent)
{
    if (m_toggleShortcut.contains(keyEvent->key() | keyEvent->modifiers())) {
        if (keyEvent->type() == QEvent::KeyPress) {
            toggle();
        }
        return;
    }
    QuickSceneEffect::grabbedKeyboardEvent(keyEvent);
}

} // namespace KWin
