/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KGLOBALACCEL_PLUGIN_H
#define KGLOBALACCEL_PLUGIN_H

#include <KGlobalAccel/private/kglobalaccel_interface.h>

#include <QObject>

class KGlobalAccelImpl : public KGlobalAccelInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kglobalaccel5.KGlobalAccelInterface" FILE "kwin.json")
    Q_INTERFACES(KGlobalAccelInterface)

public:
    KGlobalAccelImpl(QObject *parent = nullptr);
    ~KGlobalAccelImpl() override;

    bool grabKey(int key, bool grab) override;
    void setEnabled(bool) override;

public Q_SLOTS:
    bool checkKeyPressed(int keyQt);

private:
    bool m_shuttingDown = false;
    QMetaObject::Connection m_inputDestroyedConnection;
};

#endif
