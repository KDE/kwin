/*
 * SPDX-FileCopyrightText: 2026 Sebastian Sauer <dipesh@gmx.de>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "dwellclicker.h"
#include "plugins/dwellclicker/dwellclickerconfig.h"
#include "cursor.h"
#include "effect/effecthandler.h"
#include "input.h"
#include "input_event.h"
#include "pointer_input.h"
#include "scene/workspacescene.h"

#include <linux/input-event-codes.h>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QPainter>
#include <QPen>

using namespace std::literals;

namespace KWin
{

class DwellClickerInputDevice : public InputDevice
{
public:
    QString name() const override
    {
        return QStringLiteral("Dwell clicker device");
    }

    bool isEnabled() const override
    {
        return true;
    }

    void setEnabled(bool) override
    {
    }

    bool isKeyboard() const override
    {
        return true;
    }

    bool isPointer() const override
    {
        return true;
    }

    bool isTouchpad() const override
    {
        return false;
    }

    bool isTouch() const override
    {
        return false;
    }

    bool isTabletTool() const override
    {
        return false;
    }

    bool isTabletPad() const override
    {
        return false;
    }

    bool isTabletModeSwitch() const override
    {
        return false;
    }

    bool isLidSwitch() const override
    {
        return false;
    }
};

DwellClickerItem::DwellClickerItem(Item *parent)
    : ImageItem(parent)
{
    updateGeometry();
    connect(Cursors::self()->mouse(), &Cursor::themeChanged,
            this, &DwellClickerItem::updateGeometry);

    m_delayTimer.setSingleShot(true);
    connect(&m_delayTimer, &QTimer::timeout, this, [this] {
        m_dwellAnimation.start();
    });

    m_dwellAnimation.setStartValue(0.0);
    m_dwellAnimation.setEndValue(1.0);
    connect(&m_dwellAnimation, &QVariantAnimation::valueChanged, this, [this]() {
        setImage(drawImage(m_dwellAnimation.currentValue().toDouble(), 0.0));
    });
    connect(&m_dwellAnimation, &QVariantAnimation::finished, this, [this]() {
        m_clickedAnimation.start();
    });

    m_clickedAnimation.setStartValue(0.0);
    m_clickedAnimation.setEndValue(1.0);
    connect(&m_clickedAnimation, &QVariantAnimation::valueChanged, this, [this]() {
        setImage(drawImage(1.0, m_clickedAnimation.currentValue().toDouble()));
    });
    connect(&m_clickedAnimation, &QVariantAnimation::finished, this, [this]() {
        stop();
        Q_EMIT clicked();
    });
}

void DwellClickerItem::updateGeometry()
{
    const QSizeF size = Cursors::self()->mouse()->geometry().size().grownBy(QMarginsF(2, 2, 2, 2));
    setPosition(-QPointF(size.width() / 2, size.height() / 2));
    setSize(size);
}

QImage DwellClickerItem::drawImage(qreal dwellProgress, qreal clickedProgress) const
{
    QImage image(size().toSize(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int spanAngle = static_cast<int>(-5760 * dwellProgress);
    QRect rect = image.rect().adjusted(2, 2, -2, -2);

    if (clickedProgress > 0.0) {
        const int shrinkOffset = static_cast<int>((rect.width() / 2) * clickedProgress);
        rect = rect.adjusted(shrinkOffset, shrinkOffset, -shrinkOffset, -shrinkOffset);
    }

    QPen progressPen(m_color, 4);
    painter.setPen(progressPen);
    painter.drawArc(rect, 90 * 16, spanAngle);

    painter.end();
    return image;
}

void DwellClickerItem::reconfigure()
{
    m_delayTimer.setInterval(DwellClickerConfig::delayTime());

    m_dwellAnimation.setDuration(DwellClickerConfig::dwellTime());
    m_clickedAnimation.setDuration(DwellClickerConfig::dwellTime() > 0 ? 100 : 0);

    m_color = DwellClickerConfig::dwellColor();
}

bool DwellClickerItem::isRunning() const
{
    return m_running;
}

void DwellClickerItem::start()
{
    stop();

    m_running = true;
    m_delayTimer.start();
}

void DwellClickerItem::stop()
{
    if (!m_running) {
        return;
    }

    m_running = false;

    m_delayTimer.stop();
    m_dwellAnimation.stop();
    m_clickedAnimation.stop();
    setImage(QImage());
}

DwellClickerEffect::DwellClickerEffect()
    : Effect()
    , m_item(std::make_unique<DwellClickerItem>(effects->scene()->cursorItem()))
    , m_device(std::make_unique<DwellClickerInputDevice>())
{
    DwellClickerConfig::instance(effects->config());
    reconfigure(ReconfigureAll);

    connect(m_item.get(), &DwellClickerItem::clicked, this, &DwellClickerEffect::onClicked);
    input()->addInputDevice(m_device.get());
    input()->installInputEventSpy(this);

    QDBusConnection::sessionBus().registerObject(
        QStringLiteral("/org/kde/KWin/DwellClicker"),
        this,
        QDBusConnection::ExportAllProperties);
    QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.KWin.DwellClicker"));
}

DwellClickerEffect::~DwellClickerEffect()
{
    QDBusConnection::sessionBus().unregisterService(QStringLiteral("org.kde.KWin.DwellClicker"));
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/org/kde/KWin/DwellClicker"));

    input()->uninstallInputEventSpy(this);

    cancelDrag();
    m_item.reset();

    input()->removeInputDevice(m_device.get());
    m_device.reset();
}

bool DwellClickerEffect::isActive() const
{
    return m_item && m_item->isRunning();
}

void DwellClickerEffect::reconfigure(ReconfigureFlags)
{
    DwellClickerConfig::self()->read();

    setButton(DwellClickerConfig::defaultButton());

    m_item->reconfigure();
}

int DwellClickerEffect::button() const
{
    return static_cast<int>(m_button);
}

void DwellClickerEffect::setButton(int button)
{
    if (static_cast<int>(m_button) == button && !m_dragging) {
        return;
    }

    cancelDrag();
    m_item->stop();

    m_button = static_cast<Button>(button);
    m_position.reset();

    emitButtonChangedSignal();
}

void DwellClickerEffect::pointerMotion(PointerMotionEvent *event)
{
    if (m_button == Button::None) {
        // No button means disable the dwell.
        return;
    }

    if (!m_position.has_value()) {
        // Require motion above threshold also after a click or changing buttons
        m_position = event->position;
        return;
    }

    const bool aboveMotionThreshold =
        event->position.x() >= m_position->x() + DwellClickerConfig::motionThreshold() ||
        event->position.x() <= m_position->x() - DwellClickerConfig::motionThreshold() ||
        event->position.y() >= m_position->y() + DwellClickerConfig::motionThreshold() ||
        event->position.y() <= m_position->y() - DwellClickerConfig::motionThreshold();

    if (!aboveMotionThreshold) {
        return;
    }

    m_position = event->position;
    m_item->start();
}

void DwellClickerEffect::buttonPress(quint32 button, PointerButtonState state, std::chrono::milliseconds timeOffset)
{
    const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()) + timeOffset;
    Q_EMIT m_device->pointerButtonChanged(button, state, time, m_device.get());
    Q_EMIT m_device->pointerFrame(m_device.get());
}

void DwellClickerEffect::onClicked()
{
    auto buttonClick = [this](quint32 button, std::chrono::milliseconds timeOffset = std::chrono::milliseconds(0)) {
        buttonPress(button, PointerButtonState::Pressed, timeOffset);
        buttonPress(button, PointerButtonState::Released, timeOffset);
    };

    switch (m_button) {
    case Button::LeftClick:
        buttonClick(BTN_LEFT);
        break;
    case Button::DoubleClick:
        // Add a minimal time offset between the clicks so Wayland and target apps
        // process two distinct clicks instead of dropping duplicate events.
        buttonClick(BTN_LEFT, std::chrono::milliseconds(-1));
        buttonClick(BTN_LEFT);
        break;
    case Button::DragClick:
        buttonPress(BTN_LEFT, m_dragging ? PointerButtonState::Released : PointerButtonState::Pressed);
        m_dragging = !m_dragging;
        break;
    case Button::MiddleClick:
        buttonClick(BTN_MIDDLE);
        break;
    case Button::RightClick:
        buttonClick(BTN_RIGHT);
        break;
    default:
        break;
    }

    m_position.reset();

    if (!DwellClickerConfig::buttonLocked() && !m_dragging) {
        // After clicking activate the default button again.
        setButton(DwellClickerConfig::defaultButton());
    }
}

void DwellClickerEffect::cancelDrag()
{
    if (!m_dragging) {
        return;
    }

    m_dragging = false;

    // To properly cancel a drag-and-drop operation without triggering a drop, Wayland applications
    // rely on an Escape key (KEY_ESC) press during the active drag grab. After that we send the
    // PointerButtonState::Released to finish any started drag and cleanly reset the state machine.

    const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
    Q_EMIT m_device->keyChanged(KEY_ESC, KeyboardKeyState::Pressed, time, m_device.get());
    Q_EMIT m_device->keyChanged(KEY_ESC, KeyboardKeyState::Released, time, m_device.get());

    buttonPress(BTN_LEFT, PointerButtonState::Released);
}

void DwellClickerEffect::emitButtonChangedSignal()
{
    // We cannot just add a buttonChanged Q_SIGNAL and emit it since this would not result
    // in the dbus PropertiesChanged being called. So send the signal ourself.

    const QVariantMap properties = {
        {"button", static_cast<int>(m_button)}
    };

    QDBusMessage message = QDBusMessage::createSignal(
        QLatin1String("/org/kde/KWin/DwellClicker"),
        QLatin1String("org.freedesktop.DBus.Properties"),
        QLatin1String("PropertiesChanged")
    );

    message.setArguments({
        QStringLiteral("org.kde.KWin.DwellClicker"),
        properties,
        QStringList()
    });

    QDBusConnection::sessionBus().send(message);
}

}

#include "moc_dwellclicker.cpp"
