/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kglobalaccel_plugin.h"
#include "../../input.h"

#include <QDebug>

KGlobalAccelImpl::KGlobalAccelImpl(QObject *parent)
    : KGlobalAccelInterface(parent)
{
}

KGlobalAccelImpl::~KGlobalAccelImpl() = default;

bool KGlobalAccelImpl::grabKey(int key, bool grab)
{
    Q_UNUSED(key)
    Q_UNUSED(grab)
    return true;
}

void KGlobalAccelImpl::setEnabled(bool enabled)
{
    if (m_shuttingDown) {
        return;
    }
    static KWin::InputRedirection *s_input = KWin::InputRedirection::self();
    if (!s_input) {
        qFatal("This plugin is intended to be used with KWin and this is not KWin, exiting now");
    } else {
        if (!m_inputDestroyedConnection) {
            m_inputDestroyedConnection = connect(s_input, &QObject::destroyed, this, [this] { m_shuttingDown = true; });
        }
    }
    s_input->registerGlobalAccel(enabled ? this : nullptr);
}

bool KGlobalAccelImpl::checkKeyPressed(int keyQt)
{
    return keyPressed(keyQt);
}
