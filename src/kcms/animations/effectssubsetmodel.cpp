/*
    SPDX-FileCopyrightText: 2025 Oliver Beard

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "effectssubsetmodel.h"

namespace KWin
{

// TODO: The way *sourceModel and sourceModel() is used seems a little dangerous
EffectsSubsetModel::EffectsSubsetModel(EffectsModel *sourceModel, QString exclusiveGroup, QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_exclusiveGroup(std::move(exclusiveGroup))
{
    setSourceModel(sourceModel);
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

    if (idx.data(EffectsModel::ExclusiveRole).toString() != m_exclusiveGroup) {
        return false;
    }

    return true;
}

} // namespace KWin

#include "moc_effectssubsetmodel.cpp"
