/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "colorddeviceinterface.h"

#include <QDBusObjectPath>
#include <QObject>
#include <QPointer>

namespace KWin
{

class Output;

class ColordDevice : public QObject
{
public:
    explicit ColordDevice(Output *output, QObject *parent = nullptr);

    void initialize(const QDBusObjectPath &devicePath);

    Output *output() const;
    QDBusObjectPath objectPath() const;

private Q_SLOTS:
    void updateProfile();

private:
    CdDeviceInterface *m_colordInterface = nullptr;
    QPointer<Output> m_output;
};

} // namespace KWin
