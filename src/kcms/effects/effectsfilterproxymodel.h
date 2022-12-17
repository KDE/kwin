/*
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
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
