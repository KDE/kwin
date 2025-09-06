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
    explicit ConfigurableGesture(GlobalShortcutsManager *manager);
    ~ConfigurableGesture();

    QAction *forwardAction() const;
    QAction *reverseAction() const;

Q_SIGNALS:
    void forwardProgress(double value);
    void reverseProgress(double value);

private:
    QPointer<GlobalShortcutsManager> m_manager;
    std::unique_ptr<QAction> m_forwardAction;
    std::unique_ptr<QAction> m_backwardAction;
};

}
