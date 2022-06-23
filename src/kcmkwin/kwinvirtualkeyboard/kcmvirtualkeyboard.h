/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KQuickAddons/ManagedConfigModule>

#include <KService>

class KDesktopFile;
class VirtualKeyboardData;
class VirtualKeyboardSettings;

class VirtualKeyboardsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        DesktopFileNameRole = Qt::UserRole + 1,
    };
    Q_ENUM(Roles)

    VirtualKeyboardsModel(QObject *parent = nullptr);
    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent) const override;

    QHash<int, QByteArray> roleNames() const override;

    Q_SCRIPTABLE int inputMethodIndex(const QString &desktopFile) const;

private:
    KService::List m_services;
};

class KcmVirtualKeyboard : public KQuickAddons::ManagedConfigModule
{
    Q_OBJECT
    Q_PROPERTY(VirtualKeyboardSettings *settings READ settings CONSTANT)
    Q_PROPERTY(QAbstractItemModel *model READ keyboardsModel CONSTANT)

public:
    explicit KcmVirtualKeyboard(QObject *parent = nullptr, const QVariantList &list = QVariantList());
    ~KcmVirtualKeyboard() override;

    VirtualKeyboardSettings *settings() const;
    VirtualKeyboardsModel *keyboardsModel() const
    {
        return m_model;
    }

private:
    VirtualKeyboardData *m_data;
    VirtualKeyboardsModel *const m_model;
};
