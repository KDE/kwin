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
