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

ConfigurableGesture::ConfigurableGesture(GlobalShortcutsManager *manager, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
{
    connect(this, &ConfigurableGesture::triggered, this, &ConfigurableGesture::released);
    connect(this, &ConfigurableGesture::cancelled, this, &ConfigurableGesture::released);
}

ConfigurableGesture::~ConfigurableGesture()
{
    if (GlobalShortcutsManager *m = m_manager; m != nullptr) {
        m->unregisterGesture(this);
    }
}

size_t ConfigurableGesture::triggerActionCount() const
{
    return m_count;
}

QAction *ConfigurableGesture::makeAutoCountingTriggerAction()
{
    auto action = new QAction(this);
    ++m_count;
    connect(action, &QAction::triggered, this, &ConfigurableGesture::triggered);
    connect(action, &QObject::destroyed, this, [this] {
        --m_count;
    });
    return action;
}

void ConfigurableGesture::setAutoCreated()
{
    m_isAutoCreated = true;
}

bool ConfigurableGesture::isAutoCreated() const
{
    return m_isAutoCreated;
}

}
