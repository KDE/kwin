/*
    SPDX-FileCopyrightText: 2020 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
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
}

QHash<int, QByteArray> RuleBookModel::roleNames() const
{
    auto roles = QAbstractListModel::roleNames();
    roles.insert(DescriptionRole, QByteArray("display"));
    return roles;
}

int RuleBookModel::rowCount(const QModelIndex &parent) const
{
    return m_ruleBook->ruleCount();
}

QVariant RuleBookModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return QVariant();
    }

    if (index.row() < 0 || index.row() >= rowCount()) {
        return QVariant();
    }

    const RuleSettings *settings = m_ruleBook->ruleSettingsAt(index.row());

    switch (role) {
    case RuleBookModel::DescriptionRole:
        return settings->description();
    }

    return QVariant();
}

bool RuleBookModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return false;
    }

    RuleSettings *settings = m_ruleBook->ruleSettingsAt(index.row());

    switch (role) {
    case RuleBookModel::DescriptionRole:
        if (settings->description() == value.toString()) {
            return true;
        }
        settings->setDescription(value.toString());
        break;
    default:
        return false;
    }

    Q_EMIT dataChanged(index, index, {role});

    return true;
}

bool RuleBookModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if (row < 0 || row > rowCount() || parent.isValid()) {
        return false;
    }

    beginInsertRows(parent, row, row + count - 1);
    for (int i = 0; i < count; i++) {
        RuleSettings *settings = m_ruleBook->insertRuleSettingsAt(row + i);
        settings->setWmclassmatch(Rules::ExactMatch); // We want ExactMatch as default for new rules in the UI
    }
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
        m_ruleBook->removeRuleSettingsAt(row + i);
    }
    endRemoveRows();

    return true;
}

bool RuleBookModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                             const QModelIndex &destinationParent, int destinationChild)
{
    if (sourceParent != destinationParent || sourceParent != QModelIndex()) {
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
        m_ruleBook->moveRuleSettings(isMoveDown ? sourceRow : sourceRow + i, destinationChild);
    }

    endMoveRows();
    return true;
}

QString RuleBookModel::descriptionAt(int row) const
{
    Q_ASSERT(row >= 0 && row < rowCount());
    return m_ruleBook->ruleSettingsAt(row)->description();
}

RuleSettings *RuleBookModel::ruleSettingsAt(int row) const
{
    Q_ASSERT(row >= 0 && row < rowCount());
    return m_ruleBook->ruleSettingsAt(row);
}

void RuleBookModel::setDescriptionAt(int row, const QString &description)
{
    Q_ASSERT(row >= 0 && row < rowCount());
    if (description == m_ruleBook->ruleSettingsAt(row)->description()) {
        return;
    }

    m_ruleBook->ruleSettingsAt(row)->setDescription(description);

    Q_EMIT dataChanged(index(row), index(row), {});
}

void RuleBookModel::setRuleSettingsAt(int row, const RuleSettings &settings)
{
    Q_ASSERT(row >= 0 && row < rowCount());

    copySettingsTo(ruleSettingsAt(row), settings);

    Q_EMIT dataChanged(index(row), index(row), {});
}

void RuleBookModel::load()
{
    beginResetModel();

    m_ruleBook->load();

    endResetModel();
}

void RuleBookModel::save()
{
    m_ruleBook->save();
}

bool RuleBookModel::isSaveNeeded()
{
    return m_ruleBook->usrIsSaveNeeded();
}

void RuleBookModel::copySettingsTo(RuleSettings *dest, const RuleSettings &source)
{
    dest->setDefaults();
    const KConfigSkeletonItem::List itemList = source.items();
    for (const KConfigSkeletonItem *item : itemList) {
        dest->findItem(item->name())->setProperty(item->property());
    }
}

} // namespace
