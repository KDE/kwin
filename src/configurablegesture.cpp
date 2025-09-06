/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "configurablegesture.h"
#include "globalshortcuts.h"

#include <QAction>

namespace KWin
{

ConfigurableGesture::ConfigurableGesture(GlobalShortcutsManager *manager)
    : m_manager(manager)
    , m_forwardAction(std::make_unique<QAction>())
    , m_backwardAction(std::make_unique<QAction>())
{
}

ConfigurableGesture::~ConfigurableGesture()
{
    if (auto m = m_manager) {
        m->unregisterGesture(this);
    }
}

QAction *ConfigurableGesture::forwardAction() const
{
    return m_forwardAction.get();
}

QAction *ConfigurableGesture::reverseAction() const
{
    return m_backwardAction.get();
}
}
