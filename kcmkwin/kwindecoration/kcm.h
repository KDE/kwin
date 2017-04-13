/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef KDECORATIONS_KCM_H
#define KDECORATIONS_KCM_H

#include <kcmodule.h>
#include <ui_kcm.h>
#include <QAbstractItemModel>

class QSortFilterProxyModel;
class QQuickView;

namespace KDecoration2
{
namespace Preview
{
class PreviewBridge;
class ButtonsModel;
}
namespace Configuration
{
class DecorationsModel;

class ConfigurationForm : public QWidget, public Ui::KCMForm
{
public:
    explicit ConfigurationForm(QWidget* parent);
};

class ConfigurationModule : public KCModule
{
    Q_OBJECT
public:
    explicit ConfigurationModule(QWidget *parent = nullptr, const QVariantList &args = QVariantList());
    virtual ~ConfigurationModule();

    bool eventFilter(QObject *watched, QEvent *e) override;

public Q_SLOTS:
    void defaults() override;
    void load() override;
    void save() override;

protected:
    void showEvent(QShowEvent *ev) override;

private:
    void showKNS(const QString &config);
    void updateColors();
    DecorationsModel *m_model;
    QSortFilterProxyModel *m_proxyModel;
    ConfigurationForm *m_ui;
    QQuickView *m_quickView;
    Preview::ButtonsModel *m_leftButtons;
    Preview::ButtonsModel *m_rightButtons;
    Preview::ButtonsModel *m_availableButtons;
};

}

}

#endif
