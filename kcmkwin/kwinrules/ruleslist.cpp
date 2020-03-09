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

#include <QDebug>
#include <kconfig.h>
#include <QFileDialog>

#include "../../rules.h"
#include "rulesettings.h"
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
    qDeleteAll(m_rules);
    m_rules.clear();
}

void KCMRulesList::activeChanged()
{
    QListWidgetItem *item = rules_listbox->currentItem();
    int itemRow = rules_listbox->row(item);

    if (item != nullptr)   // make current==selected
        rules_listbox->setCurrentItem(item, QItemSelectionModel::ClearAndSelect);
    modify_button->setEnabled(item != nullptr);
    delete_button->setEnabled(item != nullptr);
    export_button->setEnabled(item != nullptr);
    moveup_button->setEnabled(item != nullptr && itemRow > 0);
    movedown_button->setEnabled(item != nullptr && itemRow < (rules_listbox->count() - 1));
}

void KCMRulesList::newClicked()
{
    RulesDialog dlg(this);
    Rules* rule = dlg.edit(nullptr, {}, false);
    if (rule == nullptr)
        return;
    int pos = rules_listbox->currentRow() + 1;
    rules_listbox->insertItem(pos , rule->description);
    rules_listbox->setCurrentRow(pos, QItemSelectionModel::ClearAndSelect);
    m_rules.insert(m_rules.begin() + pos, rule);
    emit changed(true);
}

void KCMRulesList::modifyClicked()
{
    int pos = rules_listbox->currentRow();
    if (pos == -1)
        return;
    RulesDialog dlg(this);
    Rules *rule = dlg.edit(m_rules[pos], {}, false);
    if (rule == m_rules[pos])
        return;
    delete m_rules[pos];
    m_rules[pos] = rule;
    rules_listbox->item(pos)->setText(rule->description);
    emit changed(true);
}

void KCMRulesList::deleteClicked()
{
    int pos = rules_listbox->currentRow();
    Q_ASSERT(pos != -1);
    delete rules_listbox->takeItem(pos);
    m_rules.erase(m_rules.begin() + pos);
    emit changed(true);
}

void KCMRulesList::moveupClicked()
{
    int pos = rules_listbox->currentRow();
    Q_ASSERT(pos != -1);
    if (pos > 0) {
        QListWidgetItem * item = rules_listbox->takeItem(pos);
        rules_listbox->insertItem(pos - 1 , item);
        rules_listbox->setCurrentItem(item, QItemSelectionModel::ClearAndSelect);
        Rules *rule = m_rules[pos];
        m_rules[pos] = m_rules[pos - 1];
        m_rules[pos - 1] = rule;
    }
    emit changed(true);
}

void KCMRulesList::movedownClicked()
{
    int pos = rules_listbox->currentRow();
    Q_ASSERT(pos != -1);
    if (pos < int(rules_listbox->count()) - 1) {
        QListWidgetItem * item = rules_listbox->takeItem(pos);
        rules_listbox->insertItem(pos + 1 , item);
        rules_listbox->setCurrentItem(item, QItemSelectionModel::ClearAndSelect);
        Rules *rule = m_rules[pos];
        m_rules[pos] = m_rules[pos + 1];
        m_rules[pos + 1] = rule;
    }
    emit changed(true);
}

void KCMRulesList::exportClicked()
{
    int pos = rules_listbox->currentRow();
    Q_ASSERT(pos != -1);
    QString path = QFileDialog::getSaveFileName(this, i18n("Export Rules"), QDir::home().absolutePath(),
                                                i18n("KWin Rules (*.kwinrule)"));
    if (path.isEmpty())
        return;
    const auto cfg = KSharedConfig::openConfig(path, KConfig::SimpleConfig);
    RuleSettings settings(cfg, m_rules[pos]->description);
    settings.setDefaults();
    m_rules[pos]->write(&settings);
    settings.save();
}

void KCMRulesList::importClicked()
{
    QString path = QFileDialog::getOpenFileName(this, i18n("Import Rules"), QDir::home().absolutePath(),
                                                i18n("KWin Rules (*.kwinrule)"));
    if (path.isEmpty())
        return;
    const auto config = KSharedConfig::openConfig(path, KConfig::SimpleConfig);
    QStringList groups = config->groupList();
    if (groups.isEmpty())
        return;

    int pos = qMax(0, rules_listbox->currentRow());
    for (const QString &group : groups) {
        RuleSettings settings(config, group);
        const bool remove = settings.deleteRule();
        Rules *new_rule = new Rules(&settings);

        // try to replace existing rule first
        for (int i = 0; i < m_rules.count(); ++i) {
            if (m_rules[i]->description == new_rule->description) {
                delete m_rules[i];
                if (remove) {
                    m_rules.remove(i);
                    delete rules_listbox->takeItem(i);
                    delete new_rule;
                    pos = qMax(0, rules_listbox->currentRow()); // might have changed!
                } else
                    m_rules[i] = new_rule;
                new_rule = nullptr;
                break;
            }
        }

        // don't add "to be deleted" if not present
        if (remove) {
            delete new_rule;
            new_rule = nullptr;
        }

        // plain insertion
        if (new_rule) {
            m_rules.insert(pos, new_rule);
            rules_listbox->insertItem(pos++, new_rule->description);
        }
    }
    emit changed(true);
}

void KCMRulesList::load()
{
    rules_listbox->clear();
    m_settings.load();
    m_rules = m_settings.rules();
    for (const auto rule : qAsConst(m_rules)) {
        rules_listbox->addItem(rule->description);
    }

    if (m_rules.count() > 0)
        rules_listbox->setCurrentItem(rules_listbox->item(0));
    else
        rules_listbox->setCurrentItem(nullptr);
    activeChanged();
}

void KCMRulesList::save()
{
    m_settings.setRules(m_rules);
    m_settings.save();
}

} // namespace

