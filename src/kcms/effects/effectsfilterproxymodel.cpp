/*
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "effectsfilterproxymodel.h"

#include "effectsmodel.h"

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
        Q_EMIT queryChanged();
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
        Q_EMIT excludeUnsupportedChanged();
        invalidateFilter();
    }
}

bool EffectsFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);

    if (!m_query.isEmpty()) {
        const bool matches = idx.data(EffectsModel::NameRole).toString().contains(m_query, Qt::CaseInsensitive) || idx.data(EffectsModel::DescriptionRole).toString().contains(m_query, Qt::CaseInsensitive) || idx.data(EffectsModel::CategoryRole).toString().contains(m_query, Qt::CaseInsensitive);
        if (!matches) {
            return false;
        }
    }

    if (m_excludeUnsupported) {
        if (!idx.data(EffectsModel::SupportedRole).toBool()) {
            return false;
        }
    }

    return true;
}

} // namespace KWin

#include "moc_effectsfilterproxymodel.cpp"
