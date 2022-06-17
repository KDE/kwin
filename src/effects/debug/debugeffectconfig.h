/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_DEBUG_CONFIG_H
#define KWIN_DEBUG_CONFIG_H

#include <kcmodule.h>

class KShortcutsEditor;

namespace KWin
{

class DebugEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit DebugEffectConfig(QWidget *parent = nullptr, const QVariantList &args = QVariantList());
    ~DebugEffectConfig() override;

public Q_SLOTS:
    void save() override;
    void load() override;
    void defaults() override;

private:
    KShortcutsEditor *mShortcutEditor;
};

} // namespace

#endif
