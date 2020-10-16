/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#ifndef KDECORATION_DECORATION_MODEL_H
#define KDECORATION_DECORATION_MODEL_H

#include "utils.h"

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
    QHash< int, QByteArray > roleNames() const override;

    QModelIndex findDecoration(const QString &pluginName, const QString &themeName = QString()) const;

    QStringList knsProviders() const {
        return m_knsProviders;
    }

public Q_SLOTS:
    void init();

private:
    struct Data {
        QString pluginName;
        QString themeName;
        QString visibleName;
        bool configuration = false;
        KDecoration2::BorderSize recommendedBorderSize = KDecoration2::BorderSize::Normal;
    };
    std::vector<Data> m_plugins;
    QStringList m_knsProviders;
};

}
}

#endif
