/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"

#include <QObject>
#include <QPointer>

#include <map>
#include <memory>

class QAction;

namespace KWin
{

class GlobalShortcutsManager;

class KWIN_EXPORT ConfigurableGesture : public QObject
{
    Q_OBJECT

public:
    explicit ConfigurableGesture(GlobalShortcutsManager *manager);
    ~ConfigurableGesture();

    using TriggerId = QPair<QString, QString>;

    // individual actions that will emit ConfigurableGesture::released,
    // these can be dropped to unregister any associated gesture shortcut
    QAction *makeGestureAction(const TriggerId &id);
    void dropGestureAction(const TriggerId &id);

Q_SIGNALS:
    void progress(double value);
    void released(bool checked = true);

private:
    QPointer<GlobalShortcutsManager> m_manager;
    std::map<TriggerId, std::unique_ptr<QAction>> m_gestureActions;
};

}
