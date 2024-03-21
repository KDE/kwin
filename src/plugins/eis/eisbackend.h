/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "core/inputbackend.h"
#include "utils/ramfile.h"

#include <QDBusContext>
#include <QDBusUnixFileDescriptor>

#include <memory>

extern "C" {
struct eis;
struct eis_device;
struct eis_seat;
}

class QDBusServiceWatcher;

namespace KWin
{
class EisContext;

class EisBackend : public KWin::InputBackend, public QDBusContext
{
    Q_OBJECT
public:
    explicit EisBackend(QObject *parent = nullptr);
    ~EisBackend() override;
    void initialize() override;

    void updateScreens() override;

    Q_INVOKABLE QDBusUnixFileDescriptor connectToEIS(const int &capabilities, int &cookie);
    Q_INVOKABLE void disconnect(int cookie);

    eis_device *createKeyboard(eis_seat *seat);
    eis_device *createPointer(eis_seat *seat);
    eis_device *createAbsoluteDevice(eis_seat *seat);

private:
    QDBusServiceWatcher *m_serviceWatcher;
    RamFile m_keymapFile;
    std::vector<std::unique_ptr<EisContext>> m_contexts;
};

}
