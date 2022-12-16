/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kcmodule.h>

#include "ui_thumbnailaside_config.h"

class KActionCollection;

namespace KWin
{

class ThumbnailAsideEffectConfigForm : public QWidget, public Ui::ThumbnailAsideEffectConfigForm
{
    Q_OBJECT
public:
    explicit ThumbnailAsideEffectConfigForm(QWidget *parent);
};

class ThumbnailAsideEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit ThumbnailAsideEffectConfig(QWidget *parent = nullptr, const QVariantList &args = QVariantList());
    ~ThumbnailAsideEffectConfig() override;

    void save() override;

private:
    ThumbnailAsideEffectConfigForm m_ui;
    KActionCollection *m_actionCollection;
};

} // namespace
