/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "rulebooksettings.h"
#include "rulesettings.h"

#include <QAbstractListModel>

namespace KWin
{

class RuleBookModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum {
        DescriptionRole = Qt::DisplayRole,
    };

    explicit RuleBookModel(QObject *parent = nullptr);
    ~RuleBookModel();

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                  const QModelIndex &destinationParent, int destinationChild) override;

    QString descriptionAt(int row) const;
    void setDescriptionAt(int row, const QString &description);

    RuleSettings *ruleSettingsAt(int row) const;
    void setRuleSettingsAt(int row, const RuleSettings &settings);

    void load();
    void save();
    bool isSaveNeeded();

    // Helper function to copy RuleSettings properties
    static void copySettingsTo(RuleSettings *dest, const RuleSettings &source);

private:
    RuleBookSettings *m_ruleBook;
};

} // namespace
