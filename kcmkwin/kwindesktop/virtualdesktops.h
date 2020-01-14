/*
 * Copyright (C) 2018 Eike Hein <hein@kde.org>
 * Copyright (C) 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
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

#ifndef VIRTUALDESKTOPS_H
#define VIRTUALDESKTOPS_H

#include <KQuickAddons/ManagedConfigModule>
#include <KSharedConfig>

class VirtualDesktopsSettings;

namespace KWin
{

class AnimationsModel;
class DesktopsModel;

class VirtualDesktops : public KQuickAddons::ManagedConfigModule
{
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel* desktopsModel READ desktopsModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel *animationsModel READ animationsModel CONSTANT)
    Q_PROPERTY(VirtualDesktopsSettings *virtualDesktopsSettings READ virtualDesktopsSettings CONSTANT)

public:
    explicit VirtualDesktops(QObject *parent = nullptr, const QVariantList &list = QVariantList());
    ~VirtualDesktops() override;

    QAbstractItemModel *desktopsModel() const;

    QAbstractItemModel *animationsModel() const;

    VirtualDesktopsSettings *virtualDesktopsSettings() const;

    bool isDefaults() const override;
    bool isSaveNeeded() const override;

public Q_SLOTS:
    void load() override;
    void save() override;
    void defaults() override;

    void configureAnimation();
    void showAboutAnimation();

private:
    VirtualDesktopsSettings *m_settings;
    DesktopsModel *m_desktopsModel;
    AnimationsModel *m_animationsModel;
};

}

#endif
