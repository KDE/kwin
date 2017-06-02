/*
 * Copyright (c) 2004 Lubos Lunak <l.lunak@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifndef __RULESWIDGET_H__
#define __RULESWIDGET_H__

#include <config-kwin.h>

#include <QDialog>
#include <kwindowsystem.h>
#include <kkeysequencewidget.h>

#include "ui_ruleswidgetbase.h"
#include "ui_editshortcut.h"

#ifdef KWIN_BUILD_ACTIVITIES
namespace KActivities {
    class Consumer;
} // namespace KActivities
#endif

namespace KWin
{

class Rules;
class DetectDialog;

class RulesWidget
    : public QWidget, public Ui::RulesWidgetBase
{
    Q_OBJECT
public:
    explicit RulesWidget(QWidget* parent = nullptr);
    void setRules(Rules* r);
    Rules* rules() const;
    bool finalCheck();
    void prepareWindowSpecific(WId window);
Q_SIGNALS:
    void changed(bool state);
protected Q_SLOTS:
    void detectClicked();
    void wmclassMatchChanged();
    void roleMatchChanged();
    void titleMatchChanged();
    void machineMatchChanged();
    void shortcutEditClicked();
private Q_SLOTS:
    // geometry tab
    void updateEnableposition();
    void updateEnablesize();
    void updateEnabledesktop();
    void updateEnablescreen();
#ifdef KWIN_BUILD_ACTIVITIES
    void updateEnableactivity();
#endif
    void updateEnablemaximizehoriz();
    void updateEnablemaximizevert();
    void updateEnableminimize();
    void updateEnableshade();
    void updateEnablefullscreen();
    void updateEnableplacement();
    // preferences tab
    void updateEnableabove();
    void updateEnablebelow();
    void updateEnablenoborder();
    void updateEnabledecocolor();
    void updateEnableskiptaskbar();
    void updateEnableskippager();
    void updateEnableskipswitcher();
    void updateEnableacceptfocus();
    void updateEnablecloseable();
    void updateEnableautogroup();
    void updateEnableautogroupfg();
    void updateEnableautogroupid();
    void updateEnableopacityactive();
    void updateEnableopacityinactive();
    // workarounds tab
    void updateEnablefsplevel();
    void updateEnablefpplevel();
    void updateEnabletype();
    void updateEnableignoregeometry();
    void updateEnableminsize();
    void updateEnablemaxsize();
    void updateEnablestrictgeometry();
    void updateEnableshortcut();
    void updateEnabledisableglobalshortcuts();
    void updateEnableblockcompositing();
    // internal
    void detected(bool);
private:
    int desktopToCombo(int d) const;
    int comboToDesktop(int val) const;
#ifdef KWIN_BUILD_ACTIVITIES
    int activityToCombo(QString d) const;
    QString comboToActivity(int val) const;
    void updateActivitiesList();
    KActivities::Consumer *m_activities;
    QString m_selectedActivityId; // we need this for async activity loading
#endif
    int comboToTiling(int val) const;
    int inc(int i) const { return i+1; }
    int dec(int i) const { return i-1; }
    void prefillUnusedValues(const KWindowInfo& info);
    DetectDialog* detect_dlg;
    bool detect_dlg_ok;
};

class RulesDialog
    : public QDialog
{
    Q_OBJECT
public:
    explicit RulesDialog(QWidget* parent = nullptr, const char* name = nullptr);
    Rules* edit(Rules* r, WId window, bool show_hints);
protected:
    void accept() Q_DECL_OVERRIDE;
private Q_SLOTS:
    void displayHints();
private:
    RulesWidget* widget;
    Rules* rules;
};

class EditShortcut
    : public QWidget, public Ui_EditShortcut
{
    Q_OBJECT
public:
    explicit EditShortcut(QWidget* parent = nullptr);
protected Q_SLOTS:
    void editShortcut();
    void clearShortcut();
};

class EditShortcutDialog
    : public QDialog
{
    Q_OBJECT
public:
    explicit EditShortcutDialog(QWidget* parent = nullptr, const char* name = nullptr);
    void setShortcut(const QString& cut);
    QString shortcut() const;
private:
    EditShortcut* widget;
};

// slightly duped from utils.cpp
class ShortcutDialog
    : public QDialog
{
    Q_OBJECT
public:
    explicit ShortcutDialog(const QKeySequence& cut, QWidget* parent = nullptr);
    void accept() Q_DECL_OVERRIDE;
    QKeySequence shortcut() const;
private:
    KKeySequenceWidget* widget;
};

} // namespace

#endif
