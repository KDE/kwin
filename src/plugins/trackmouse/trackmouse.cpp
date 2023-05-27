/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010 Jorge Mata <matamax123@gmail.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "trackmouse.h"

// KConfigSkeleton
#include "trackmouseconfig.h"

#include <QAction>
#include <QMatrix4x4>
#include <QPainter>
#include <QTime>

#include "libkwineffects/kwinconfig.h"
#include "libkwineffects/kwinglutils.h"
#include "libkwineffects/rendertarget.h"
#include "libkwineffects/renderviewport.h"

#include <KGlobalAccel>
#include <KLocalizedString>

#include <cmath>

namespace KWin
{

TrackMouseEffect::TrackMouseEffect()
{
    initConfig<TrackMouseConfig>();
    m_mousePolling = false;

    m_action = new QAction(this);
    m_action->setObjectName(QStringLiteral("TrackMouse"));
    m_action->setText(i18n("Track mouse"));
    KGlobalAccel::self()->setDefaultShortcut(m_action, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(m_action, QList<QKeySequence>());

    connect(m_action, &QAction::triggered, this, &TrackMouseEffect::toggle);

    connect(effects, &EffectsHandler::mouseChanged, this, &TrackMouseEffect::slotMouseChanged);
    reconfigure(ReconfigureAll);
}

TrackMouseEffect::~TrackMouseEffect()
{
    if (m_mousePolling) {
        effects->stopMousePolling();
    }
}

void TrackMouseEffect::reconfigure(ReconfigureFlags)
{
    m_modifiers = Qt::KeyboardModifiers();
    TrackMouseConfig::self()->read();
    if (TrackMouseConfig::shift()) {
        m_modifiers |= Qt::ShiftModifier;
    }
    if (TrackMouseConfig::alt()) {
        m_modifiers |= Qt::AltModifier;
    }
    if (TrackMouseConfig::control()) {
        m_modifiers |= Qt::ControlModifier;
    }
    if (TrackMouseConfig::meta()) {
        m_modifiers |= Qt::MetaModifier;
    }

    if (m_modifiers) {
        if (!m_mousePolling) {
            effects->startMousePolling();
        }
        m_mousePolling = true;
    } else if (m_mousePolling) {
        effects->stopMousePolling();
        m_mousePolling = false;
    }
}

void TrackMouseEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    // TODO, once per screen on multiscreen
    if (m_scene) {
        // should guard, this but we know we rotate
        m_scene->update();
    }
}

void TrackMouseEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, EffectScreen *screen)
{
    effects->paintScreen(renderTarget, viewport, mask, region, screen);

    // TODO if visible in viewport
    if (m_scene) {
        effects->renderOffscreenQuickView(renderTarget, viewport, m_scene.get());
    }
}

void TrackMouseEffect::toggle()
{
    switch (m_state) {
    case State::ActivatedByModifiers:
        m_state = State::ActivatedByShortcut;
        break;

    case State::ActivatedByShortcut:
        m_state = State::Inactive;
        break;

    case State::Inactive:
        m_state = State::ActivatedByShortcut;
        break;

    default:
        Q_UNREACHABLE();
        break;
    }
}

void TrackMouseEffect::slotMouseChanged(const QPointF &pos, const QPointF &,
                                        Qt::MouseButtons, Qt::MouseButtons,
                                        Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers)
{
    if (!m_mousePolling) { // we didn't ask for it but maybe someone else did...
        return;
    }
    switch (m_state) {
    case State::ActivatedByModifiers:
        if (modifiers == m_modifiers) {
            break;
        }
        m_state = State::Inactive;
        break;

    case State::ActivatedByShortcut:
        return;

    case State::Inactive:
        if (modifiers != m_modifiers) {
            break;
        }
        m_state = State::ActivatedByModifiers;
        break;

    default:
        Q_UNREACHABLE();
        break;
    }

    if (m_state != State::Inactive) {
        if (!m_scene) {
            m_scene.reset(new OffscreenQuickScene(nullptr));
            m_scene->setAutomaticRepaint(false);
            auto glowPtr = m_scene.get();

            connect(m_scene.get(), &OffscreenQuickView::renderRequested, this, [glowPtr] {
                effects->addRepaint(glowPtr->geometry());
            });
            connect(m_scene.get(), &OffscreenQuickView::sceneChanged, this, [glowPtr] {
                effects->addRepaint(glowPtr->geometry());
            });
            connect(m_scene.get(), &OffscreenQuickView::geometryChanged, this, [](const QRect &oldGeom, const QRect &newGeom) {
                effects->addRepaint(oldGeom);
                effects->addRepaint(newGeom);
            });

            m_scene->setSource(QUrl::fromLocalFile("/home/david/projects/temp/qml/mousemark2.qml"));
        }

        QRect rect(0, 0, 300, 300);
        rect.moveCenter(pos.toPoint());
        m_scene->setGeometry(rect);

    } else if (m_scene) {
        effects->addRepaint(m_scene->geometry());
        qDebug() << "RELEASE";

        m_scene.reset();
    }
}

bool TrackMouseEffect::isActive() const
{
    return m_state != State::Inactive;
}

} // namespace
