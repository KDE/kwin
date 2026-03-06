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
{
}

ConfigurableGesture::~ConfigurableGesture()
{
    if (auto m = m_manager) {
        m->unregisterGesture(this);
    }
}

QAction *ConfigurableGesture::makeGestureAction(const TriggerId &id)
{
    auto iteratorInsertedPair = m_gestureActions.insert_or_assign(id, std::make_unique<QAction>());
    QAction *gestureAction = iteratorInsertedPair.first->second.get();
    connect(gestureAction, &QAction::triggered, this, &ConfigurableGesture::released);
    return gestureAction;
}

void ConfigurableGesture::dropGestureAction(const TriggerId &id)
{
    m_gestureActions.erase(id);
}

}
