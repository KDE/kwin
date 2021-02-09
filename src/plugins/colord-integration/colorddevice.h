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

class AbstractOutput;

class ColordDevice : public QObject
{
public:
    explicit ColordDevice(AbstractOutput *output, QObject *parent = nullptr);

    void initialize(const QDBusObjectPath &devicePath);

    AbstractOutput *output() const;
    QDBusObjectPath objectPath() const;

private Q_SLOTS:
    void updateProfile();

private:
    CdDeviceInterface *m_colordInterface = nullptr;
    QPointer<AbstractOutput> m_output;
};

} // namespace KWin
