/*
    SPDX-FileCopyrightText: 2025 Oliver Beard

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "effectssubsetmodel.h"

namespace KWin
{

EffectsSubsetModel::EffectsSubsetModel(EffectsModel *sourceModel, const QString &exclusiveGroup, QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_mode(Mode::ExclusiveGroupMode)
    , m_exclusiveGroup(std::move(exclusiveGroup))
{
    setSourceModel(sourceModel);
}

EffectsSubsetModel::EffectsSubsetModel(EffectsModel *sourceModel, const QStringList &whitelistEffects, QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_mode(Mode::WhitelistMode)
    , m_whitelistEffects(std::move(whitelistEffects))
{
    setSourceModel(sourceModel);
    sort(0);
}

void EffectsSubsetModel::defaults()
{
    auto effectsModel = qobject_cast<EffectsModel *>(sourceModel());
    if (!effectsModel) {
        return;
    }

    for (int row = 0; row < rowCount(); ++row) {
        effectsModel->defaults(mapToSource(index(row, 0)));
    }
}

bool EffectsSubsetModel::isDefaults() const
{
    auto effectsModel = qobject_cast<EffectsModel *>(sourceModel());
    if (!effectsModel) {
        return true;
    }

    for (int row = 0; row < rowCount(); ++row) {
        if (!effectsModel->isDefaults(mapToSource(index(row, 0)))) {
            return false;
        }
    }
    return true;
}

bool EffectsSubsetModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);

    switch (m_mode) {
    case Mode::ExclusiveGroupMode:
        if (idx.data(EffectsModel::ExclusiveRole).toString() != m_exclusiveGroup) {
            return false;
        }
        break;
    case Mode::WhitelistMode:
        if (!m_whitelistEffects.contains(idx.data(EffectsModel::ServiceNameRole).toString())) {
            return false;
        }
        break;
    }

    return true;
}

bool EffectsSubsetModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    if (m_mode == Mode::WhitelistMode) {
        const QString leftEffect = sourceModel()->data(left, EffectsModel::ServiceNameRole).toString();
        const QString rightEffect = sourceModel()->data(right, EffectsModel::ServiceNameRole).toString();

        return m_whitelistEffects.indexOf(leftEffect) < m_whitelistEffects.indexOf(rightEffect);
    }

    return QSortFilterProxyModel::lessThan(left, right);
}

} // namespace KWin

#include "moc_effectssubsetmodel.cpp"
