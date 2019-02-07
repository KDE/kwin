/*
 * Copyright (c) 2019 Valerio Pilo <vpilo@coldshock.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "utils.h"

#include <KQuickAddons/ConfigModule>


class QAbstractItemModel;
class QSortFilterProxyModel;
class QQuickItem;

namespace KNS3
{
class DownloadDialog;
}

namespace KDecoration2
{
enum class BorderSize;

namespace Preview
{
class ButtonsModel;
}
namespace Configuration
{
class DecorationsModel;
}
}

class KCMKWinDecoration : public KQuickAddons::ConfigModule
{
    Q_OBJECT
    Q_PROPERTY(QSortFilterProxyModel *themesModel READ themesModel CONSTANT)
    Q_PROPERTY(QStringList borderSizesModel READ borderSizesModel CONSTANT)
    Q_PROPERTY(int borderSize READ borderSize WRITE setBorderSize NOTIFY borderSizeChanged)
    Q_PROPERTY(int theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QAbstractListModel *leftButtonsModel READ leftButtonsModel NOTIFY buttonsChanged)
    Q_PROPERTY(QAbstractListModel *rightButtonsModel READ rightButtonsModel NOTIFY buttonsChanged)
    Q_PROPERTY(QAbstractListModel *availableButtonsModel READ availableButtonsModel CONSTANT)
    Q_PROPERTY(bool closeOnDoubleClickOnMenu READ closeOnDoubleClickOnMenu WRITE setCloseOnDoubleClickOnMenu NOTIFY closeOnDoubleClickOnMenuChanged)

public:
    KCMKWinDecoration(QObject *parent, const QVariantList &arguments);

    QSortFilterProxyModel *themesModel() const;
    QAbstractListModel *leftButtonsModel();
    QAbstractListModel *rightButtonsModel();
    QAbstractListModel *availableButtonsModel() const;
    QStringList borderSizesModel() const;
    int borderSize() const;
    int theme() const;
    bool closeOnDoubleClickOnMenu() const;

    void setBorderSize(int index);
    void setBorderSize(KDecoration2::BorderSize size);
    void setTheme(int index);
    void setCloseOnDoubleClickOnMenu(bool enable);

    Q_INVOKABLE void getNewStuff(QQuickItem *context);

Q_SIGNALS:
    void themeChanged();
    void buttonsChanged();
    void borderSizeChanged();
    void closeOnDoubleClickOnMenuChanged();

public Q_SLOTS:
    void load() override;
    void save() override;
    void defaults() override;

private Q_SLOTS:
    void updateNeedsSave();
    void reloadKWinSettings();

private:
    KDecoration2::Configuration::DecorationsModel *m_themesModel;
    QSortFilterProxyModel *m_proxyThemesModel;

    KDecoration2::Preview::ButtonsModel *m_leftButtonsModel;
    KDecoration2::Preview::ButtonsModel *m_rightButtonsModel;
    KDecoration2::Preview::ButtonsModel *m_availableButtonsModel;

    QPointer<KNS3::DownloadDialog> m_newStuffDialog;

    struct Settings
    {
        KDecoration2::BorderSize borderSize;
        int themeIndex;
        bool closeOnDoubleClickOnMenu;
        DecorationButtonsList buttonsOnLeft;
        DecorationButtonsList buttonsOnRight;
    };

    Settings m_savedSettings;
    Settings m_currentSettings;
};
