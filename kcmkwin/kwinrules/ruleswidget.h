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

#include <kdialog.h>
#include <kwindowsystem.h>
#include <kkeysequencewidget.h>

#include "ui_ruleswidgetbase.h"
#include "ui_editshortcut.h"

namespace KWin
{

class Rules;
class DetectDialog;

class RulesWidget
    : public QWidget, public Ui::RulesWidgetBase
{
    Q_OBJECT
public:
    RulesWidget(QWidget* parent = NULL);
    void setRules(Rules* r);
    Rules* rules() const;
    bool finalCheck();
    void prepareWindowSpecific(WId window);
signals:
    void changed(bool state);
protected slots:
    void detectClicked();
    void wmclassMatchChanged();
    void roleMatchChanged();
    void titleMatchChanged();
    void machineMatchChanged();
    void shortcutEditClicked();
private slots:
    // geometry tab
    void updateEnableposition();
    void updateEnablesize();
    void updateEnabledesktop();
    void updateEnableactivity();
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
    void updateEnabletype();
    void updateEnableignoreposition();
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
    int activityToCombo(QString d) const;
    QString comboToActivity(int val) const;
    int comboToTiling(int val) const;
    void prefillUnusedValues(const KWindowInfo& info);
    DetectDialog* detect_dlg;
    bool detect_dlg_ok;
};

class RulesDialog
    : public KDialog
{
    Q_OBJECT
public:
    explicit RulesDialog(QWidget* parent = NULL, const char* name = NULL);
    Rules* edit(Rules* r, WId window, bool show_hints);
protected:
    virtual void accept();
private slots:
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
    EditShortcut(QWidget* parent = NULL);
protected slots:
    void editShortcut();
    void clearShortcut();
};

class EditShortcutDialog
    : public KDialog
{
    Q_OBJECT
public:
    explicit EditShortcutDialog(QWidget* parent = NULL, const char* name = NULL);
    void setShortcut(const QString& cut);
    QString shortcut() const;
private:
    EditShortcut* widget;
};

// slightly duped from utils.cpp
class ShortcutDialog
    : public KDialog
{
    Q_OBJECT
public:
    explicit ShortcutDialog(const QKeySequence& cut, QWidget* parent = NULL);
    virtual void accept();
    QKeySequence shortcut() const;
private:
    KKeySequenceWidget* widget;
};

} // namespace

#endif
