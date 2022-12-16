/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <KGlobalAccel/private/kglobalaccel_interface.h>

#include <QObject>

class KGlobalAccelImpl : public KGlobalAccelInterfaceV2
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID KGlobalAccelInterface_iid FILE "kwin.json")
    Q_INTERFACES(KGlobalAccelInterface)

public:
    KGlobalAccelImpl(QObject *parent = nullptr);
    ~KGlobalAccelImpl() override;

    bool grabKey(int key, bool grab) override;
    void setEnabled(bool) override;

public Q_SLOTS:
    bool checkKeyPressed(int keyQt);
    bool checkKeyReleased(int keyQt);

private:
    bool m_shuttingDown = false;
    QMetaObject::Connection m_inputDestroyedConnection;
};
