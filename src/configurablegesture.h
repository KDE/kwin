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

class QAction;

namespace KWin
{

class GlobalShortcutsManager;

class KWIN_EXPORT ConfigurableGesture : public QObject
{
    Q_OBJECT

public:
    explicit ConfigurableGesture(GlobalShortcutsManager *manager, QObject *parent = nullptr);
    ~ConfigurableGesture();

    // for internal use by GlobalShortcutsManager
    size_t triggerActionCount() const;
    QAction *makeAutoCountingTriggerAction();
    void setAutoCreated();
    bool isAutoCreated() const;

Q_SIGNALS:
    void progress(double value);

    // convenience signal, emitted when either cancelled or triggered signals are emitted
    void released(bool triggered);

    // `triggered` parameter is included so another QAction::triggered() or also the released()
    // signal can be chained - it's always false for cancelled() and always true for triggered()
    void cancelled(bool triggered = false);
    void triggered(bool triggered = true);

private:
    QPointer<GlobalShortcutsManager> m_manager;
    size_t m_count = 0; //< number of associated actions for individual gesture triggers
    bool m_isAutoCreated = false;
};

}
