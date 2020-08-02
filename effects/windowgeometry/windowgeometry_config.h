/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Thomas Lübking <thomas.luebking@web.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef WINDOWGEOMETRY_CONFIG_H
#define WINDOWGEOMETRY_CONFIG_H

#include <kcmodule.h>

#include "ui_windowgeometry_config.h"


namespace KWin
{

class WindowGeometryConfigForm : public QWidget, public Ui::WindowGeometryConfigForm
{
    Q_OBJECT
public:
    explicit WindowGeometryConfigForm(QWidget* parent);
};

class WindowGeometryConfig : public KCModule
{
    Q_OBJECT
public:
    explicit WindowGeometryConfig(QWidget* parent = nullptr, const QVariantList& args = QVariantList());
    ~WindowGeometryConfig() override;

public Q_SLOTS:
    void save() override;
    void defaults() override;

private:
    WindowGeometryConfigForm* myUi;
    KActionCollection* myActionCollection;
};

} // namespace

#endif
