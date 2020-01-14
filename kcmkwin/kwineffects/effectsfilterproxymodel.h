/*
 * Copyright (C) 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
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

#pragma once

#include <QSortFilterProxyModel>

namespace KWin
{

class EffectsFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *sourceModel READ sourceModel WRITE setSourceModel)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(bool excludeInternal READ excludeInternal WRITE setExcludeInternal NOTIFY excludeInternalChanged)
    Q_PROPERTY(bool excludeUnsupported READ excludeUnsupported WRITE setExcludeUnsupported NOTIFY excludeUnsupportedChanged)

public:
    explicit EffectsFilterProxyModel(QObject *parent = nullptr);
    ~EffectsFilterProxyModel() override;

    QString query() const;
    void setQuery(const QString &query);

    bool excludeInternal() const;
    void setExcludeInternal(bool exclude);

    bool excludeUnsupported() const;
    void setExcludeUnsupported(bool exclude);

Q_SIGNALS:
    void queryChanged();
    void excludeInternalChanged();
    void excludeUnsupportedChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_query;
    bool m_excludeInternal = true;
    bool m_excludeUnsupported = true;

    Q_DISABLE_COPY(EffectsFilterProxyModel)
};

} // namespace KWin
