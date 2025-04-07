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
    explicit EffectsSubsetModel(EffectsModel *sourceModel, QString exclusiveGroup, QObject *parent = nullptr);

    void defaults();
    bool isDefaults() const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_exclusiveGroup;
    bool m_excludeUnsupported = true;

    Q_DISABLE_COPY(EffectsSubsetModel)
};

} // namespace KWin
