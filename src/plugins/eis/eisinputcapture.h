/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QDBusContext>
#include <QDBusUnixFileDescriptor>
#include <QObject>
#include <QSocketNotifier>

#include "eisinputcapturemanager.h"
#include "input_event_spy.h"

#include <libeis.h>

#include <memory>

namespace KWin
{
class BarrierSpy;

class EisInputCapture : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.EIS.InputCapture")
public:
    EisInputCapture(EisInputCaptureManager *manager, const QString &dbusService, QFlags<eis_device_capability> allowedCapabilities);
    ~EisInputCapture();

    const QString dbusService;

    QList<EisInputCaptureBarrier> barriers() const;
    QString dbusPath() const;

    eis_device *pointer() const;
    eis_device *keyboard() const;
    eis_device *absoluteDevice() const;

    void activate(const QPointF &position);

    Q_INVOKABLE QDBusUnixFileDescriptor connectToEIS();
    Q_INVOKABLE void enable(const QList<QPair<QPoint, QPoint>> &barriers);
    Q_INVOKABLE void disable();
    Q_INVOKABLE void release(const QPointF &cursorPosition, bool applyPosition);
Q_SIGNALS:
    void disabled();
    void activated(uint activationId, const QPointF &cursorPosition);
    void deactivated(uint activationId);

private:
    void handleEvents();
    void createDevice();
    void deactivate();

    EisInputCaptureManager *m_manager;
    QList<EisInputCaptureBarrier> m_barriers;
    QString m_dbusPath;
    QFlags<eis_device_capability> m_allowedCapabilities;
    eis *m_eis;
    QSocketNotifier m_socketNotifier;
    eis_client *m_client = nullptr;
    eis_seat *m_seat = nullptr;
    eis_device *m_pointer = nullptr;
    eis_device *m_keyboard = nullptr;
    eis_device *m_absoluteDevice = nullptr;
    uint m_activationId = 0;
};

}
