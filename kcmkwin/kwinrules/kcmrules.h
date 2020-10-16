/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
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
