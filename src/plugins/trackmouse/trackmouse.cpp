/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2010 Jorge Mata <matamax123@gmail.com>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugins/trackmouse/trackmouse.h"
#include "effect/effecthandler.h"
#include "plugins/trackmouse/trackmouseconfig.h"
#include "scene/imageitem.h"
#include "scene/itemrenderer.h"
#include "scene/workspacescene.h"

#include <KGlobalAccel>
#include <KLocalizedString>

#include <QAction>
#include <QTime>

namespace KWin
{

RotatingArcsItem::RotatingArcsItem(Item *parentItem)
    : Item(parentItem)
{
    const QString f[2] = {QStandardPaths::locate(QStandardPaths::GenericDataLocation, KWIN_DATADIR + QStringLiteral("/tm_outer.png")),
                          QStandardPaths::locate(QStandardPaths::GenericDataLocation, KWIN_DATADIR + QStringLiteral("/tm_inner.png"))};
    if (f[0].isEmpty() || f[1].isEmpty()) {
        return;
    }

    const QImage outerImage(f[0]);
    m_outerArcItem = scene()->renderer()->createImageItem(this);
    m_outerArcItem->setImage(outerImage);
    m_outerArcItem->setSize(outerImage.size());
    m_outerArcItem->setPosition(QPointF(-outerImage.width() / 2, -outerImage.height() / 2));

    const QImage innerImage(f[1]);
    m_innerArcItem = scene()->renderer()->createImageItem(this);
    m_innerArcItem->setImage(innerImage);
    m_innerArcItem->setSize(innerImage.size());
    m_innerArcItem->setPosition(QPointF(-innerImage.width() / 2, -innerImage.height() / 2));
}

void RotatingArcsItem::rotate(qreal angle)
{
    m_outerArcItem->setTransform(
        QTransform()
            .translate(m_outerArcItem->size().width() / 2, m_outerArcItem->size().height() / 2)
            .rotate(angle)
            .translate(-m_outerArcItem->size().width() / 2, -m_outerArcItem->size().height() / 2));

    m_innerArcItem->setTransform(
        QTransform()
            .translate(m_innerArcItem->size().width() / 2, m_innerArcItem->size().height() / 2)
            .rotate(-2 * angle)
            .translate(-m_innerArcItem->size().width() / 2, -m_innerArcItem->size().height() / 2));
}

TrackMouseEffect::TrackMouseEffect()
{
    TrackMouseConfig::instance(effects->config());

    QAction *action = new QAction(this);
    action->setObjectName(QStringLiteral("TrackMouse"));
    action->setText(i18n("Track mouse"));
    KGlobalAccel::self()->setDefaultShortcut(action, QList<QKeySequence>());
    KGlobalAccel::self()->setShortcut(action, QList<QKeySequence>());
    connect(action, &QAction::triggered, this, &TrackMouseEffect::toggle);

    reconfigure(ReconfigureAll);
}

TrackMouseEffect::~TrackMouseEffect()
{
}

void TrackMouseEffect::reconfigure(ReconfigureFlags)
{
    const bool needMouseEventsOld = needMouseEvents();
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
    const bool needMouseEventsNew = needMouseEvents();
    if (needMouseEventsNew && !needMouseEventsOld) {
        connect(effects, &EffectsHandler::mouseChanged, this, &TrackMouseEffect::slotMouseChanged);
    } else if (!needMouseEventsNew && needMouseEventsOld) {
        disconnect(effects, &EffectsHandler::mouseChanged, this, &TrackMouseEffect::slotMouseChanged);
    }
}

void TrackMouseEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    QTime t = QTime::currentTime();
    m_angle = ((t.second() % 4) * 90.0) + (t.msec() / 1000.0 * 90.0);
    m_rotatingArcsItem->rotate(m_angle);

    effects->prePaintScreen(data, presentTime);
}

void TrackMouseEffect::toggle()
{
    const bool needMouseEventsOld = needMouseEvents();
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

    const bool needMouseEventsNew = needMouseEvents();
    if (needMouseEventsNew && !needMouseEventsOld) {
        connect(effects, &EffectsHandler::mouseChanged, this, &TrackMouseEffect::slotMouseChanged);
    } else if (!needMouseEventsNew && needMouseEventsOld) {
        disconnect(effects, &EffectsHandler::mouseChanged, this, &TrackMouseEffect::slotMouseChanged);
    }
    if (m_state == State::Inactive) {
        m_rotatingArcsItem.reset();
    } else {
        if (!m_rotatingArcsItem) {
            m_rotatingArcsItem = std::make_unique<RotatingArcsItem>(effects->scene()->overlayItem());
        }
        m_rotatingArcsItem->setPosition(effects->cursorPos());
    }
}

void TrackMouseEffect::slotMouseChanged(const QPointF &, const QPointF &,
                                        Qt::MouseButtons, Qt::MouseButtons,
                                        Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers)
{
    switch (m_state) {
    case State::ActivatedByModifiers:
        if (modifiers != m_modifiers) {
            m_state = State::Inactive;
        }
        break;

    case State::ActivatedByShortcut:
        break;

    case State::Inactive:
        if (modifiers == m_modifiers) {
            m_state = State::ActivatedByModifiers;
        }
        break;

    default:
        Q_UNREACHABLE();
        break;
    }

    if (m_state == State::Inactive) {
        m_rotatingArcsItem.reset();
    } else {
        if (!m_rotatingArcsItem) {
            m_rotatingArcsItem = std::make_unique<RotatingArcsItem>(effects->scene()->overlayItem());
        }
        m_rotatingArcsItem->setPosition(effects->cursorPos());
    }
}

bool TrackMouseEffect::isActive() const
{
    return m_state != State::Inactive;
}

bool TrackMouseEffect::needMouseEvents() const
{
    return m_state != State::Inactive || m_modifiers;
}

} // namespace

#include "moc_trackmouse.cpp"
