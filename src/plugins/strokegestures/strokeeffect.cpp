/*
    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2025 Jakob Petsovits <jpetso@petsovits.com>

    SPDX-License-Identifier: GPL-2.0-or-later AND MIT
*/

#include "strokeeffect.h"

// generated
#include <strokeeffectconfig.h>

// KWin
#include <core/inputdevice.h>
#include <core/rendertarget.h>
#include <core/renderviewport.h>
#include <cursor.h>
#include <effect/effecthandler.h>
#include <input.h>

// Qt
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>

using namespace std::chrono_literals;

namespace KWin
{

const int TEXTURE_PADDING = 10;

StrokeEffect::StrokeEffect()
    : m_shutdownTimer(std::make_unique<QTimer>())
{
    m_shutdownTimer->setSingleShot(true);
    connect(m_shutdownTimer.get(), &QTimer::timeout, this, &StrokeEffect::realDeactivate);
    connect(effects, &EffectsHandler::screenAboutToLock, this, &StrokeEffect::realDeactivate);

    StrokeEffectConfig::instance(effects->config());
    reconfigure(ReconfigureAll);

    auto delegate = new QQmlComponent(effects->qmlEngine(), this);
    connect(delegate, &QQmlComponent::statusChanged, this, [delegate]() {
        if (delegate->isError()) {
            qWarning() << "Failed to load stroke effect:" << delegate->errorString();
        }
    });
    delegate->loadUrl(QUrl(QStringLiteral("qrc:/stroke/qml/main.qml")), QQmlComponent::Asynchronous);
    setDelegate(delegate);
}

StrokeEffect::~StrokeEffect() = default;

QVariantMap StrokeEffect::initialProperties(LogicalOutput *screen)
{
    return QVariantMap{
        {QStringLiteral("effect"), QVariant::fromValue(this)},
        {QStringLiteral("targetScreen"), QVariant::fromValue(screen)},
    };
}

void StrokeEffect::reconfigure(ReconfigureFlags)
{
    StrokeEffectConfig::self()->read();

    setStrokeFadeOutMsec(animationTime(std::chrono::milliseconds(StrokeEffectConfig::strokeFadeOutMsec())));
    setShowsTriggeredActionOsd(StrokeEffectConfig::showsTriggeredActionOsd());

    // FIXME: example strokes, until we get them through some other means (config, D-Bus, etc.)
    qDebug() << "StrokeEffect::reconfigure(): registering example strokes";
    QList<QPointF> rightwardPoints;
    QList<QPointF> leftwardPoints;
    for (int i = 0; i < 9; ++i) {
        rightwardPoints.push_back(QPointF(i / 8.0, 0.5));
        leftwardPoints.push_back(QPointF(1.0 - (i / 8.0), 0.5));
    }
    m_actions.clear();
    m_actions.push_back(std::make_unique<QAction>(QIcon::fromTheme("go-next"), "&Forward"));
    connect(m_actions.back().get(), &QAction::triggered, this, [] {
        qDebug() << "STROKE RIGHT!";
    });
    effects->registerStrokeShortcut(Qt::NoModifier, rightwardPoints, m_actions.back().get());

    m_actions.push_back(std::make_unique<QAction>(QIcon::fromTheme("go-previous"), "&Back"));
    connect(m_actions.back().get(), &QAction::triggered, this, [] {
        qDebug() << "STROKE LEFT!";
    });
    effects->registerStrokeShortcut(Qt::NoModifier, leftwardPoints, m_actions.back().get());
}

void StrokeEffect::activate()
{
    setRunning(true);
    if (!m_isStrokeActive) {
        m_isStrokeActive = true;
        Q_EMIT strokeActiveChanged();
    }
}

void StrokeEffect::deactivate(int timeout)
{
    if (m_isStrokeActive) {
        m_isStrokeActive = false;
        Q_EMIT strokeActiveChanged();
    }
    m_shutdownTimer->start(timeout);
}

void StrokeEffect::realDeactivate()
{
    setRunning(false);
}

bool StrokeEffect::isStrokeActive() const
{
    return m_isStrokeActive;
}

int StrokeEffect::strokeFadeOutMsec() const
{
    return m_strokeFadeOutMsec;
}

void StrokeEffect::setStrokeFadeOutMsec(int msec)
{
    if (m_strokeFadeOutMsec != msec) {
        m_strokeFadeOutMsec = msec;
        Q_EMIT strokeFadeOutMsecChanged();
    }
}

bool StrokeEffect::showsTriggeredActionOsd() const
{
    return m_showsTriggeredActionOsd;
}

void StrokeEffect::setShowsTriggeredActionOsd(bool osd)
{
    if (m_showsTriggeredActionOsd != osd) {
        m_showsTriggeredActionOsd = osd;
        Q_EMIT showsTriggeredActionOsdChanged();
    }
}

void StrokeEffect::strokeGestureBegin(const StrokeGestureBeginEvent *event)
{
    activate();
    Q_EMIT strokeStarted(event->origin);
    Q_EMIT strokePointAdded(event->latestPos);
}

void StrokeEffect::strokeGestureUpdate(const StrokeGestureUpdateEvent *event)
{
    if (event->startingNewSegment) {
        Q_EMIT strokePointReplaced(event->segmentOrigin);
        Q_EMIT strokePointAdded(event->latestPos);
    } else {
        Q_EMIT strokePointReplaced(event->latestPos);
    }
}

void StrokeEffect::strokeGestureEnd(const StrokeGestureEndEvent *event)
{
    deactivate(m_strokeFadeOutMsec);
    Q_EMIT strokeEnded();

    if (m_showsTriggeredActionOsd && event->triggeredAction) {
        QDBusMessage msg = QDBusMessage::createMethodCall( //
            QStringLiteral("org.kde.plasmashell"),
            QStringLiteral("/org/kde/osdService"),
            QStringLiteral("org.kde.osdService"),
            QStringLiteral("showText"));

        msg.setArguments({event->triggeredAction->icon().name(), event->triggeredAction->iconText()});

        QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
    }
}

void StrokeEffect::strokeGestureCancelled(const StrokeGestureCancelEvent *event)
{
    deactivate(m_strokeFadeOutMsec);
    Q_EMIT strokeCancelled();
}

} // namespace KWin

#include "moc_strokeeffect.cpp"
