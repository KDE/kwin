/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "core/inputbackend.h"
#include "utils/ramfile.h"

#include <QDBusUnixFileDescriptor>

#include <memory>

extern "C" {
struct eis;
struct eis_device;
struct eis_seat;
}

namespace KWin
{
struct EisContext;

class EisBackend : public KWin::InputBackend
{
    Q_OBJECT
public:
    explicit EisBackend(QObject *parent = nullptr);
    ~EisBackend() override;
    void initialize() override;

    Q_INVOKABLE QDBusUnixFileDescriptor connectToEIS(const int &capabilities, int &cookie);
    Q_INVOKABLE void disconnect(int cookie);

private:
    void handleEvents(EisContext *context);
    eis_device *createKeyboard(eis_seat *seat);
    eis_device *createPointer(eis_seat *seat);
    eis_device *createAbsoluteDevice(eis_seat *seat);

    RamFile m_keymapFile;
    std::vector<std::unique_ptr<EisContext>> m_contexts;
};

}
