/*
 * Copyright (c) 2020 Ismael Asensio <isma.af@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <rules.h>
#include "rulebookmodel.h"
#include "rulesmodel.h"

#include <KQuickAddons/ConfigModule>


namespace KWin
{

class KCMKWinRules : public KQuickAddons::ConfigModule
{
    Q_OBJECT

    Q_PROPERTY(RuleBookModel *ruleBookModel MEMBER m_ruleBookModel CONSTANT)
    Q_PROPERTY(RulesModel *rulesModel MEMBER m_rulesModel CONSTANT)
    Q_PROPERTY(int editIndex READ editIndex NOTIFY editIndexChanged)

public:
    explicit KCMKWinRules(QObject *parent, const QVariantList &arguments);

    Q_INVOKABLE void setRuleDescription(int index, const QString &description);
    Q_INVOKABLE void editRule(int index);

    Q_INVOKABLE void createRule();
    Q_INVOKABLE void removeRule(int index);
    Q_INVOKABLE void moveRule(int sourceIndex, int destIndex);

    Q_INVOKABLE void exportToFile(const QUrl &path, const QList<int> &indexes);
    Q_INVOKABLE void importFromFile(const QUrl &path);

public slots:
    void load() override;
    void save() override;

signals:
    void editIndexChanged();

private slots:
    void updateNeedsSave();

private:
    int editIndex() const;
    void saveCurrentRule();

private:
    RuleBookModel *m_ruleBookModel;
    RulesModel* m_rulesModel;

    QPersistentModelIndex m_editIndex;
};

} // namespace
