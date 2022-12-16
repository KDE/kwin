/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once

#include "utils.h"

#include <KDecoration2/DecorationThemeProvider>
#include <QAbstractListModel>

namespace KDecoration2
{

namespace Configuration
{

class DecorationsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum DecorationRole {
        PluginNameRole = Qt::UserRole + 1,
        ThemeNameRole,
        ConfigurationRole,
        RecommendedBorderSizeRole,
    };

public:
    explicit DecorationsModel(QObject *parent = nullptr);
    ~DecorationsModel() override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;

    QModelIndex findDecoration(const QString &pluginName, const QString &themeName = QString()) const;

    QStringList knsProviders() const
    {
        return m_knsProviders;
    }

public Q_SLOTS:
    void init();

private:
    std::vector<KDecoration2::DecorationThemeMetaData> m_plugins;
    QStringList m_knsProviders;
};

}
}
