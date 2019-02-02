/*
 * Copyright (C) 2019 Vlad Zagorodniy <vladzzag@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "effectsfilterproxymodel.h"

#include "effectmodel.h"

namespace KWin
{

EffectsFilterProxyModel::EffectsFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

EffectsFilterProxyModel::~EffectsFilterProxyModel()
{
}

QString EffectsFilterProxyModel::query() const
{
    return m_query;
}

void EffectsFilterProxyModel::setQuery(const QString &query)
{
    if (m_query != query) {
        m_query = query;
        emit queryChanged();
        invalidateFilter();
    }
}

bool EffectsFilterProxyModel::excludeInternal() const
{
    return m_excludeInternal;
}

void EffectsFilterProxyModel::setExcludeInternal(bool exclude)
{
    if (m_excludeInternal != exclude) {
        m_excludeInternal = exclude;
        emit excludeInternalChanged();
        invalidateFilter();
    }
}

bool EffectsFilterProxyModel::excludeUnsupported() const
{
    return m_excludeUnsupported;
}

void EffectsFilterProxyModel::setExcludeUnsupported(bool exclude)
{
    if (m_excludeUnsupported != exclude) {
        m_excludeUnsupported = exclude;
        emit excludeUnsupportedChanged();
        invalidateFilter();
    }
}

bool EffectsFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);

    if (!m_query.isEmpty()) {
        const bool matches = idx.data(EffectModel::NameRole).toString().contains(m_query, Qt::CaseInsensitive) ||
            idx.data(EffectModel::DescriptionRole).toString().contains(m_query, Qt::CaseInsensitive) ||
            idx.data(EffectModel::CategoryRole).toString().contains(m_query, Qt::CaseInsensitive);
        if (!matches) {
            return false;
        }
    }

    if (m_excludeInternal) {
        if (idx.data(EffectModel::InternalRole).toBool()) {
            return false;
        }
    }

    if (m_excludeUnsupported) {
        if (!idx.data(EffectModel::SupportedRole).toBool()) {
            return false;
        }
    }

    return true;
}

} // namespace KWin
