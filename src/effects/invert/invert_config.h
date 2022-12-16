/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kcmodule.h>

class KShortcutsEditor;

namespace KWin
{

class InvertEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit InvertEffectConfig(QWidget *parent = nullptr, const QVariantList &args = QVariantList());
    ~InvertEffectConfig() override;

public Q_SLOTS:
    void save() override;
    void load() override;
    void defaults() override;

private:
    KShortcutsEditor *mShortcutEditor;
};

} // namespace
