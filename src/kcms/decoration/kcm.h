/*
    SPDX-FileCopyrightText: 2019 Valerio Pilo <vpilo@coldshock.net>
    SPDX-FileCopyrightText: 2019 Cyril Rossi <cyril.rossi@enioka.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#pragma once

#include "utils.h"

#include <KQuickAddons/ManagedConfigModule>

class QAbstractItemModel;
class QSortFilterProxyModel;
class QQuickItem;

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

class KWinDecorationSettings;
class KWinDecorationData;

class KCMKWinDecoration : public KQuickAddons::ManagedConfigModule
{
    Q_OBJECT
    Q_PROPERTY(KWinDecorationSettings *settings READ settings CONSTANT)
    Q_PROPERTY(QSortFilterProxyModel *themesModel READ themesModel CONSTANT)
    Q_PROPERTY(QStringList borderSizesModel READ borderSizesModel NOTIFY themeChanged)
    Q_PROPERTY(int borderIndex READ borderIndex WRITE setBorderIndex NOTIFY borderIndexChanged)
    Q_PROPERTY(int borderSize READ borderSize NOTIFY borderSizeChanged)
    Q_PROPERTY(int recommendedBorderSize READ recommendedBorderSize CONSTANT)
    Q_PROPERTY(int theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QAbstractListModel *leftButtonsModel READ leftButtonsModel NOTIFY buttonsChanged)
    Q_PROPERTY(QAbstractListModel *rightButtonsModel READ rightButtonsModel NOTIFY buttonsChanged)
    Q_PROPERTY(QAbstractListModel *availableButtonsModel READ availableButtonsModel CONSTANT)

public:
    KCMKWinDecoration(QObject *parent, const QVariantList &arguments);

    KWinDecorationSettings *settings() const;
    QSortFilterProxyModel *themesModel() const;
    QAbstractListModel *leftButtonsModel();
    QAbstractListModel *rightButtonsModel();
    QAbstractListModel *availableButtonsModel() const;
    QStringList borderSizesModel() const;
    int borderIndex() const;
    int borderSize() const;
    int recommendedBorderSize() const;
    int theme() const;

    void setBorderIndex(int index);
    void setBorderSize(int index);
    void setBorderSize(KDecoration2::BorderSize size);
    void setTheme(int index);

Q_SIGNALS:
    void themeChanged();
    void borderIndexChanged();
    void borderSizeChanged();

public Q_SLOTS:
    void load() override;
    void save() override;
    void defaults() override;
    void reloadKWinSettings();

private Q_SLOTS:
    void onLeftButtonsChanged();
    void onRightButtonsChanged();

private:
    bool isSaveNeeded() const override;

    int borderSizeIndexFromString(const QString &size) const;
    QString borderSizeIndexToString(int index) const;

    KDecoration2::Configuration::DecorationsModel *m_themesModel;
    QSortFilterProxyModel *m_proxyThemesModel;

    KDecoration2::Preview::ButtonsModel *m_leftButtonsModel;
    KDecoration2::Preview::ButtonsModel *m_rightButtonsModel;
    KDecoration2::Preview::ButtonsModel *m_availableButtonsModel;

    int m_borderSizeIndex = -1;
    KWinDecorationData *m_data;
};
