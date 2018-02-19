/*
 * Copyright 2016  Martin Graesslin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "onscreennotification.h"
#include "input.h"
#include "input_event.h"
#include "input_event_spy.h"
#include <config-kwin.h>

#include <QPropertyAnimation>
#include <QStandardPaths>
#include <QTimer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>

#include <KConfigGroup>

#include <functional>

using namespace KWin;

class KWin::OnScreenNotificationInputEventSpy : public InputEventSpy
{
public:
    explicit OnScreenNotificationInputEventSpy(OnScreenNotification *parent);

    void pointerEvent(MouseEvent *event) override;
private:
    OnScreenNotification *m_parent;
};

OnScreenNotificationInputEventSpy::OnScreenNotificationInputEventSpy(OnScreenNotification *parent)
    : m_parent(parent)
{
}

void OnScreenNotificationInputEventSpy::pointerEvent(MouseEvent *event)
{
    if (event->type() != QEvent::MouseMove) {
        return;
    }

    m_parent->setContainsPointer(m_parent->geometry().contains(event->globalPos()));
}


OnScreenNotification::OnScreenNotification(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, std::bind(&OnScreenNotification::setVisible, this, false));
    connect(this, &OnScreenNotification::visibleChanged, this,
        [this] {
            if (m_visible) {
                show();
            } else {
                m_timer->stop();
                m_spy.reset();
                m_containsPointer = false;
            }
        }
    );
}

OnScreenNotification::~OnScreenNotification()
{
    if (QQuickWindow *w = qobject_cast<QQuickWindow*>(m_mainItem.data())) {
        w->hide();
        w->destroy();
    }
}

void OnScreenNotification::setConfig(KSharedConfigPtr config)
{
    m_config = config;
}

void OnScreenNotification::setEngine(QQmlEngine *engine)
{
    m_qmlEngine = engine;
}

bool OnScreenNotification::isVisible() const
{
    return m_visible;
}

void OnScreenNotification::setVisible(bool visible)
{
    if (m_visible == visible) {
        return;
    }
    m_visible = visible;
    emit visibleChanged();
}

QString OnScreenNotification::message() const
{
    return m_message;
}

void OnScreenNotification::setMessage(const QString &message)
{
    if (m_message == message) {
        return;
    }
    m_message = message;
    emit messageChanged();
}

QString OnScreenNotification::iconName() const
{
    return m_iconName;
}

void OnScreenNotification::setIconName(const QString &iconName)
{
    if (m_iconName == iconName) {
        return;
    }
    m_iconName = iconName;
    emit iconNameChanged();
}

int OnScreenNotification::timeout() const
{
    return m_timer->interval();
}

void OnScreenNotification::setTimeout(int timeout)
{
    if (m_timer->interval() == timeout) {
        return;
    }
    m_timer->setInterval(timeout);
    emit timeoutChanged();
}

void OnScreenNotification::show()
{
    Q_ASSERT(m_visible);
    ensureQmlContext();
    ensureQmlComponent();
    createInputSpy();
    if (m_timer->interval() != 0) {
        m_timer->start();
    }
}

void OnScreenNotification::ensureQmlContext()
{
    Q_ASSERT(m_qmlEngine);
    if (!m_qmlContext.isNull()) {
        return;
    }
    m_qmlContext.reset(new QQmlContext(m_qmlEngine));
    m_qmlContext->setContextProperty(QStringLiteral("osd"), this);
}

void OnScreenNotification::ensureQmlComponent()
{
    Q_ASSERT(m_config);
    Q_ASSERT(m_qmlEngine);
    if (!m_qmlComponent.isNull()) {
        return;
    }
    m_qmlComponent.reset(new QQmlComponent(m_qmlEngine));
    const QString fileName = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                m_config->group(QStringLiteral("OnScreenNotification")).readEntry("QmlPath", QStringLiteral(KWIN_NAME "/onscreennotification/plasma/main.qml")));
    if (fileName.isEmpty()) {
        return;
    }
    m_qmlComponent->loadUrl(QUrl::fromLocalFile(fileName));
    if (!m_qmlComponent->isError()) {
        m_mainItem.reset(m_qmlComponent->beginCreate(m_qmlContext.data()));
        if (QQuickWindow *w = qobject_cast<QQuickWindow*>(m_mainItem.data())) {
            QObject::connect(w, &QQuickWindow::sceneGraphError, [&](QQuickWindow::SceneGraphError error, const QString &message) {
                                 Q_UNUSED(error)
                                 qCritical() << "Scene graph error in OnScreenNotification:" << message;
                             });
            m_qmlComponent->completeCreate();
        }
    } else {
        m_qmlComponent.reset();
    }
}

void OnScreenNotification::createInputSpy()
{
    Q_ASSERT(m_spy.isNull());
    if (auto w = qobject_cast<QQuickWindow*>(m_mainItem.data())) {
        m_spy.reset(new OnScreenNotificationInputEventSpy(this));
        input()->installInputEventSpy(m_spy.data());
        if (!m_animation) {
            m_animation = new QPropertyAnimation(w, "opacity", this);
            m_animation->setStartValue(1.0);
            m_animation->setEndValue(0.0);
            m_animation->setDuration(250);
            m_animation->setEasingCurve(QEasingCurve::InOutQuad);
        }
    }
}

QRect OnScreenNotification::geometry() const
{
    if (QQuickWindow *w = qobject_cast<QQuickWindow*>(m_mainItem.data())) {
        return w->geometry();
    }
    return QRect();
}

void OnScreenNotification::setContainsPointer(bool contains)
{
    if (m_containsPointer == contains) {
        return;
    }
    m_containsPointer = contains;
    if (!m_animation) {
        return;
    }
    m_animation->setDirection(m_containsPointer ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);
    m_animation->start();
}

void OnScreenNotification::setSkipCloseAnimation(bool skip)
{
    if (QQuickWindow *w = qobject_cast<QQuickWindow*>(m_mainItem.data())) {
        w->setProperty("KWIN_SKIP_CLOSE_ANIMATION", skip);
    }
}

#include "onscreennotification.moc"
