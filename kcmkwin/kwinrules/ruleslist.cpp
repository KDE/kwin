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

#include "ruleslist.h"

#include <klistwidget.h>
#include <kpushbutton.h>
#include <assert.h>
#include <kdebug.h>
#include <kconfig.h>
#include <KFileDialog>

#include "ruleswidget.h"

namespace KWin
{

KCMRulesList::KCMRulesList(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
    // connect both current/selected, so that current==selected (stupid QListBox :( )
    connect(rules_listbox, SIGNAL(itemChanged(QListWidgetItem*)),
            SLOT(activeChanged()));
    connect(rules_listbox, SIGNAL(itemSelectionChanged()),
            SLOT(activeChanged()));
    connect(new_button, SIGNAL(clicked()),
            SLOT(newClicked()));
    connect(modify_button, SIGNAL(clicked()),
            SLOT(modifyClicked()));
    connect(delete_button, SIGNAL(clicked()),
            SLOT(deleteClicked()));
    connect(moveup_button, SIGNAL(clicked()),
            SLOT(moveupClicked()));
    connect(movedown_button, SIGNAL(clicked()),
            SLOT(movedownClicked()));
    connect(export_button, SIGNAL(clicked()),
            SLOT(exportClicked()));
    connect(import_button, SIGNAL(clicked()),
            SLOT(importClicked()));
    connect(rules_listbox, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
            SLOT(modifyClicked()));
    load();
}

KCMRulesList::~KCMRulesList()
{
    for (QVector< Rules* >::Iterator it = rules.begin();
            it != rules.end();
            ++it)
        delete *it;
    rules.clear();
}

void KCMRulesList::activeChanged()
{
    QListWidgetItem *item = rules_listbox->currentItem();
    int itemRow = rules_listbox->row(item);

    if (item != NULL)   // make current==selected
        rules_listbox->setCurrentItem(item, QItemSelectionModel::ClearAndSelect);
    modify_button->setEnabled(item != NULL);
    delete_button->setEnabled(item != NULL);
    export_button->setEnabled(item != NULL);
    moveup_button->setEnabled(item != NULL && itemRow > 0);
    movedown_button->setEnabled(item != NULL && itemRow < (rules_listbox->count() - 1));
}

void KCMRulesList::newClicked()
{
    RulesDialog dlg(this);
    Rules* rule = dlg.edit(NULL, 0, false);
    if (rule == NULL)
        return;
    int pos = rules_listbox->currentRow() + 1;
    rules_listbox->insertItem(pos , rule->description);
    rules_listbox->setCurrentRow(pos, QItemSelectionModel::ClearAndSelect);
    rules.insert(rules.begin() + pos, rule);
    emit changed(true);
}

void KCMRulesList::modifyClicked()
{
    int pos = rules_listbox->currentRow();
    if (pos == -1)
        return;
    RulesDialog dlg(this);
    Rules* rule = dlg.edit(rules[ pos ], 0, false);
    if (rule == rules[ pos ])
        return;
    delete rules[ pos ];
    rules[ pos ] = rule;
    rules_listbox->item(pos)->setText(rule->description);
    emit changed(true);
}

void KCMRulesList::deleteClicked()
{
    int pos = rules_listbox->currentRow();
    assert(pos != -1);
    delete rules_listbox->takeItem(pos);
    rules.erase(rules.begin() + pos);
    emit changed(true);
}

void KCMRulesList::moveupClicked()
{
    int pos = rules_listbox->currentRow();
    assert(pos != -1);
    if (pos > 0) {
        QListWidgetItem * item = rules_listbox->takeItem(pos);
        rules_listbox->insertItem(pos - 1 , item);
        rules_listbox->setCurrentItem(item, QItemSelectionModel::ClearAndSelect);
        Rules* rule = rules[ pos ];
        rules[ pos ] = rules[ pos - 1 ];
        rules[ pos - 1 ] = rule;
    }
    emit changed(true);
}

void KCMRulesList::movedownClicked()
{
    int pos = rules_listbox->currentRow();
    assert(pos != -1);
    if (pos < int(rules_listbox->count()) - 1) {
        QListWidgetItem * item = rules_listbox->takeItem(pos);
        rules_listbox->insertItem(pos + 1 , item);
        rules_listbox->setCurrentItem(item, QItemSelectionModel::ClearAndSelect);
        Rules* rule = rules[ pos ];
        rules[ pos ] = rules[ pos + 1 ];
        rules[ pos + 1 ] = rule;
    }
    emit changed(true);
}

void KCMRulesList::exportClicked()
{
    int pos = rules_listbox->currentRow();
    assert(pos != -1);
    QString path = KFileDialog::getSaveFileName(KUrl(), "*.kwinrule", this, i18n("Export Rule"), 0);
    if (path.isEmpty())
        return;
    KConfig config(path, KConfig::SimpleConfig);
    KConfigGroup group(&config, rules[pos]->description);
    group.deleteGroup();
    rules[pos]->write(group);
}

void KCMRulesList::importClicked()
{
    QString path = KFileDialog::getOpenFileName(KUrl(), "*.kwinrule", this, i18n("Import Rules"));
    if (path.isEmpty())
        return;
    KConfig config(path, KConfig::SimpleConfig);
    QStringList groups = config.groupList();
    if (groups.isEmpty())
        return;

    int pos = qMax(0, rules_listbox->currentRow());
    foreach (const QString &group, groups) {
        KConfigGroup grp(&config, group);
        const bool remove = grp.readEntry("DeleteRule", false);
        Rules* new_rule = new Rules(grp);

        // try to replace existing rule first
        for (int i = 0; i < rules.count(); ++i) {
            if (rules[i]->description == new_rule->description) {
                delete rules[i];
                if (remove) {
                    rules.remove(i);
                    delete rules_listbox->takeItem(i);
                    delete new_rule;
                    pos = qMax(0, rules_listbox->currentRow()); // might have changed!
                }
                else
                    rules[i] = new_rule;
                new_rule = 0;
                break;
            }
        }

        // don't add "to be deleted" if not present
        if (remove) {
            delete new_rule;
            new_rule = 0;
        }

        // plain insertion
        if (new_rule) {
            rules.insert(pos, new_rule);
            rules_listbox->insertItem(pos++, new_rule->description);
        }
    }
    emit changed(true);
}

void KCMRulesList::load()
{
    rules_listbox->clear();
    for (QVector< Rules* >::Iterator it = rules.begin();
            it != rules.end();
            ++it)
        delete *it;
    rules.clear();
    KConfig _cfg("kwinrulesrc");
    KConfigGroup cfg(&_cfg, "General");
    int count = cfg.readEntry("count", 0);
    rules.reserve(count);
    for (int i = 1;
            i <= count;
            ++i) {
        cfg = KConfigGroup(&_cfg, QString::number(i));
        Rules* rule = new Rules(cfg);
        rules.append(rule);
        rules_listbox->addItem(rule->description);
    }
    if (rules.count() > 0)
        rules_listbox->setCurrentItem(rules_listbox->item(0));
    else
        rules_listbox->setCurrentItem(NULL);
    activeChanged();
}

void KCMRulesList::save()
{
    KConfig cfg(QLatin1String("kwinrulesrc"));
    QStringList groups = cfg.groupList();
    for (QStringList::ConstIterator it = groups.constBegin();
            it != groups.constEnd();
            ++it)
        cfg.deleteGroup(*it);
    cfg.group("General").writeEntry("count", rules.count());
    int i = 1;
    for (QVector< Rules* >::ConstIterator it = rules.constBegin();
            it != rules.constEnd();
            ++it) {
        KConfigGroup cg(&cfg, QString::number(i));
        (*it)->write(cg);
        ++i;
    }
}

void KCMRulesList::defaults()
{
    load();
}

} // namespace

#include "ruleslist.moc"
