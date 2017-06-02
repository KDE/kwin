/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef __MAIN_H__
#define __MAIN_H__

#include <kcmodule.h>
#include <ksharedconfig.h>

#include "ui_main.h"

class KActionCollection;
class KConfigGroup;
class KShortcutsEditor;

namespace KWin
{
// if you change this, update also the number of keyboard shortcuts in kwin/kwinbindings.cpp
static const int maxDesktops = 20;
static const int defaultDesktops = 4;

class KWinDesktopConfigForm : public QWidget, public Ui::KWinDesktopConfigForm
{
    Q_OBJECT

public:
    explicit KWinDesktopConfigForm(QWidget* parent);
};

class KWinDesktopConfig : public KCModule
{
    Q_OBJECT

public:
    explicit KWinDesktopConfig(QWidget* parent, const QVariantList& args);
    ~KWinDesktopConfig();
    QString cachedDesktopName(int desktop);

    // undo all changes
    void undo();

public Q_SLOTS:
    void save() Q_DECL_OVERRIDE;
    void load() Q_DECL_OVERRIDE;
    void defaults() Q_DECL_OVERRIDE;


private Q_SLOTS:
    void slotChangeShortcuts(int number);
    void slotShowAllShortcuts();
    void slotEffectSelectionChanged(int index);
    void slotAboutEffectClicked();
    void slotConfigureEffectClicked();

private:
    void init();
    void addAction(const QString &name, const QString &label);
    bool effectEnabled(const QString& effect, const KConfigGroup& cfg) const;
    QString extrapolatedShortcut(int desktop) const;

private:
    KWinDesktopConfigForm* m_ui;
    KSharedConfigPtr m_config;
    // cache for desktop names given by NETRootInfo
    // needed as the widget only stores the names for actual number of desktops
    QStringList m_desktopNames;
    // Collection for switching desktops like ctrl+f1
    KActionCollection* m_actionCollection;
    // Collection for next, previous, up, down desktop
    KActionCollection* m_switchDesktopCollection;
    KShortcutsEditor* m_editor;
};

} // namespace

#endif
