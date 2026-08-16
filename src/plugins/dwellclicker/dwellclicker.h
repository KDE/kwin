/*
 * SPDX-FileCopyrightText: 2026 Sebastian Sauer <dipesh@gmx.de>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include "core/inputdevice.h"
#include "input_event_spy.h"
#include "effect/effect.h"
#include "scene/imageitem.h"

#include <memory>
#include <optional>

#include <QTimer>
#include <QVariantAnimation>

namespace KWin
{

class Cursor;
class DwellClickerInputDevice;

class DwellClickerItem : public ImageItem
{
    Q_OBJECT

public:
    explicit DwellClickerItem(Item *parent);

    void reconfigure();
    bool isRunning() const;
    void start();
    void stop();

Q_SIGNALS:
    void clicked();

private:
    void updateGeometry();
    QImage drawImage(qreal dwellProgress, qreal clickedProgress) const;

    QTimer m_delayTimer;
    QVariantAnimation m_dwellAnimation;
    QVariantAnimation m_clickedAnimation;
    QColor m_color = {0, 0, 255, 160};
    bool m_running = false;
};

class DwellClickerEffect : public Effect, public InputEventSpy
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.DwellClicker")
    Q_PROPERTY(int button READ button WRITE setButton)

public:
    DwellClickerEffect();
    ~DwellClickerEffect() override;

    bool isActive() const override;
    void reconfigure(ReconfigureFlags flags) override;
    void pointerMotion(PointerMotionEvent *event) override;

    int button() const;
    void setButton(int button);

private:
    void buttonPress(quint32 button, PointerButtonState state, std::chrono::milliseconds timeOffset = std::chrono::milliseconds(0));
    void onClicked();
    void cancelDrag();
    void emitButtonChangedSignal();

    enum class Button {
        None = 0,
        LeftClick = 10,
        DoubleClick = 11,
        DragClick = 12,
        MiddleClick = 13,
        RightClick = 14
    };

    std::unique_ptr<DwellClickerItem> m_item;
    std::unique_ptr<DwellClickerInputDevice> m_device;
    std::optional<QPointF> m_position;
    Button m_button = Button::LeftClick;
    bool m_dragging = false;
};

}
