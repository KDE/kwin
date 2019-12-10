/*
 * Copyright (C) 2018 Eike Hein <hein@kde.org>
 * Copyright (C) 2018 Vlad Zahorodnii <vladzzag@gmail.com>
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

#include <KQuickAddons/ConfigModule>
#include <KSharedConfig>

class VirtualDesktopsSettings;

namespace KWin
{

class AnimationsModel;
class DesktopsModel;

class VirtualDesktops : public KQuickAddons::ConfigModule
{
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel* desktopsModel READ desktopsModel CONSTANT)
    Q_PROPERTY(bool navWraps READ navWraps WRITE setNavWraps NOTIFY navWrapsChanged)
    Q_PROPERTY(bool osdEnabled READ osdEnabled WRITE setOsdEnabled NOTIFY osdEnabledChanged)
    Q_PROPERTY(int osdDuration READ osdDuration WRITE setOsdDuration NOTIFY osdDurationChanged)
    Q_PROPERTY(bool osdTextOnly READ osdTextOnly WRITE setOsdTextOnly NOTIFY osdTextOnlyChanged)
    Q_PROPERTY(QAbstractItemModel *animationsModel READ animationsModel CONSTANT)

public:
    explicit VirtualDesktops(QObject *parent = nullptr, const QVariantList &list = QVariantList());
    ~VirtualDesktops() override;

    QAbstractItemModel *desktopsModel() const;

    bool navWraps() const;
    void setNavWraps(bool wraps);

    bool osdEnabled() const;
    void setOsdEnabled(bool enabled);

    int osdDuration() const;
    void setOsdDuration(int duration);

    int osdTextOnly() const;
    void setOsdTextOnly(bool textOnly);

    QAbstractItemModel *animationsModel() const;

Q_SIGNALS:
    void navWrapsChanged() const;
    void osdEnabledChanged() const;
    void osdDurationChanged() const;
    void osdTextOnlyChanged() const;

public Q_SLOTS:
    void load() override;
    void save() override;
    void defaults() override;

    void configureAnimation();
    void showAboutAnimation();

private Q_SLOTS:
    void updateNeedsSave();

private:
    VirtualDesktopsSettings *m_settings;
    DesktopsModel *m_desktopsModel;
    bool m_navWraps;
    bool m_osdEnabled;
    int m_osdDuration;
    bool m_osdTextOnly;
    AnimationsModel *m_animationsModel;
};

}

#endif
