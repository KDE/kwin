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

#include "rulebookmodel.h"


namespace KWin
{

RuleBookModel::RuleBookModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_ruleBook(new RuleBookSettings(this))
{
}

RuleBookModel::~RuleBookModel()
{
    qDeleteAll(m_rules);
}

int RuleBookModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_rules.count();
}

QVariant RuleBookModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return QVariant();
    }

    switch (role) {
    case Qt::DisplayRole:
        return descriptionAt(index.row());
    default:
        return QVariant();
    }
}

bool RuleBookModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return false;
    }

    switch (role) {
    case Qt::DisplayRole:
        setDescriptionAt(index.row(), value.toString());
        return true;
    default:
        return false;
    }
}

bool RuleBookModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if (row < 0 || row > rowCount() || parent.isValid()) {
        return false;
    }
    beginInsertRows(parent, row, row + count - 1);

    for (int i = 0; i < count; i++) {
        Rules *newRule = new Rules();
        // HACK: Improve integration with RuleSettings and use directly its defaults
        newRule->wmclassmatch = Rules::ExactMatch;
        m_rules.insert(row + i, newRule);
    }

    m_ruleBook->setCount(m_rules.count());

    endInsertRows();
    return true;
}

bool RuleBookModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (row < 0 || row > rowCount() || parent.isValid()) {
        return false;
    }
    beginRemoveRows(parent, row, row + count - 1);

    for (int i = 0; i < count; i++) {
        delete m_rules.at(row + i);
    }
    m_rules.remove(row, count);
    m_ruleBook->setCount(m_rules.count());

    endRemoveRows();
    return true;
}

bool RuleBookModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                             const QModelIndex &destinationParent, int destinationChild)
{
    if (sourceParent != destinationParent || sourceParent != QModelIndex()){
        return false;
    }

    const bool isMoveDown = destinationChild > sourceRow;
    // QAbstractItemModel::beginMoveRows(): when moving rows down in the same parent,
    // the rows will be placed before the destinationChild index.
    if (!beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1,
                       destinationParent, isMoveDown ? destinationChild + 1 : destinationChild)) {
        return false;
    }

    for (int i = 0; i < count; i++) {
        m_rules.insert(destinationChild + i,
                       m_rules.takeAt(isMoveDown ? sourceRow : sourceRow + i));
    }

    endMoveRows();
    return true;
}


QString RuleBookModel::descriptionAt(int row) const
{
    Q_ASSERT (row >= 0 && row < m_rules.count());
    return m_rules.at(row)->description;
}

Rules *RuleBookModel::ruleAt(int row) const
{
    Q_ASSERT (row >= 0 && row < m_rules.count());
    return m_rules.at(row);
}

void RuleBookModel::setDescriptionAt(int row, const QString &description)
{
    Q_ASSERT (row >= 0 && row < m_rules.count());
    if (description == m_rules.at(row)->description) {
        return;
    }

    m_rules.at(row)->description = description;

    emit dataChanged(index(row), index(row), QVector<int>{Qt::DisplayRole});
}

void RuleBookModel::setRuleAt(int row, Rules *rule)
{
    Q_ASSERT (row >= 0 && row < m_rules.count());

    delete m_rules.at(row);
    m_rules[row] = rule;

    emit dataChanged(index(row), index(row), QVector<int>{Qt::DisplayRole});
}


void RuleBookModel::load()
{
    beginResetModel();

    m_ruleBook->load();
    qDeleteAll(m_rules);
    m_rules = m_ruleBook->rules();

    endResetModel();
}

void RuleBookModel::save()
{
    m_ruleBook->setRules(m_rules);
    m_ruleBook->save();
}

} // namespace
