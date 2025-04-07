/*
    SPDX-FileCopyrightText: 2025 Oliver Beard

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QSortFilterProxyModel>

#include "effectsmodel.h"

namespace KWin
{

class EffectsSubsetModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit EffectsSubsetModel(EffectsModel *sourceModel, const QString &exclusiveGroup, QObject *parent = nullptr);
    explicit EffectsSubsetModel(EffectsModel *sourceModel, const QStringList &whitelistEffects, QObject *parent = nullptr);

    enum Mode {
        ExclusiveGroupMode, // Filter all except specified exlusiveGroup (ExclusiveRole)
        WhitelistMode // Filter all except specified effect ids (ServiceNameRole)
    };

    void defaults();
    bool isDefaults() const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    Mode m_mode;
    QString m_exclusiveGroup;
    QStringList m_whitelistEffects;
    bool m_excludeUnsupported = true;

    Q_DISABLE_COPY(EffectsSubsetModel)
};

} // namespace KWin
