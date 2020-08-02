/*
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KQuickAddons/ConfigModule>

#include <QAbstractItemModel>
#include <QQuickItem>

namespace KWin
{

class EffectsModel;

class DesktopEffectsKCM : public KQuickAddons::ConfigModule
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *effectsModel READ effectsModel CONSTANT)

public:
    explicit DesktopEffectsKCM(QObject *parent = nullptr, const QVariantList &list = {});
    ~DesktopEffectsKCM() override;

    QAbstractItemModel *effectsModel() const;

public Q_SLOTS:
    void load() override;
    void save() override;
    void defaults() override;

    void openGHNS(QQuickItem *context);
    void configure(const QString &pluginId, QQuickItem *context);

private Q_SLOTS:
    void updateNeedsSave();

private:
    EffectsModel *m_model;

    Q_DISABLE_COPY(DesktopEffectsKCM)
};

} // namespace KWin
