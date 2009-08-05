/********************************************************************
Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef AURORAE_CONFIG_H
#define AURORAE_CONFIG_H

#include <QWidget>
#include <QAbstractListModel>
#include <QAbstractItemDelegate>
#include <KConfig>

#include "ui_config.h"

namespace Plasma
{
class FrameSvg;
}

namespace Aurorae
{
class ThemeConfig;

//Theme selector code by Andre Duffeck (modified to add package description)
class ThemeInfo
{
public:
    QString package;
    Plasma::FrameSvg *svg;
    QString description;
    QString author;
    QString email;
    QString version;
    QString website;
    QString license;
    QString themeRoot;
    ThemeConfig *themeConfig;
    QHash<QString, Plasma::FrameSvg*> *buttons;
};

class ThemeModel : public QAbstractListModel
{
public:
    enum { PackageNameRole = Qt::UserRole,
           SvgRole = Qt::UserRole + 1,
           PackageDescriptionRole = Qt::UserRole + 2,
           PackageAuthorRole = Qt::UserRole + 3,
           PackageVersionRole = Qt::UserRole + 4,
           PackageLicenseRole = Qt::UserRole + 5,
           PackageEmailRole = Qt::UserRole + 6,
           PackageWebsiteRole = Qt::UserRole + 7,
           ThemeConfigRole = Qt::UserRole + 8,
           ButtonsRole = Qt::UserRole + 9
         };

    ThemeModel(QObject *parent = 0);
    virtual ~ThemeModel();

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    int indexOf(const QString &name) const;
    void reload();
    void clearThemeList();
    void initButtonFrame(const QString &button, const QString &path, QHash<QString, Plasma::FrameSvg*> *buttons);
private:
    QMap<QString, ThemeInfo> m_themes;
};

class ThemeDelegate : public QAbstractItemDelegate
{
public:
    ThemeDelegate(QObject *parent = 0);

    virtual void paint(QPainter *painter,
                       const QStyleOptionViewItem &option,
                       const QModelIndex &index) const;
    virtual QSize sizeHint(const QStyleOptionViewItem &option,
                           const QModelIndex &index) const;
private:
    void paintDeco(QPainter *painter, bool active, const QStyleOptionViewItem &option, const QModelIndex &index,
               int leftMargin, int topMargin,
               int rightMargin, int bottomMargin) const;
};

class AuroraeConfigUI : public QWidget, public Ui::ConfigUI
{
public:
    AuroraeConfigUI(QWidget *parent) : QWidget(parent)
    {
        setupUi(this);
    }
};

class AuroraeConfig: public QObject
{
    Q_OBJECT
public:
    AuroraeConfig(KConfig *conf, QWidget *parent);
    ~AuroraeConfig();

signals:
    void changed();
public slots:
    void load(const KConfigGroup &conf);
    void save(KConfigGroup &conf);
    void defaults();
private slots:
    void slotAboutClicked();
    void slotInstallNewTheme();
    void slotGHNSClicked();

private:
    QWidget *m_parent;
    ThemeModel *m_themeModel;
    KConfig *m_config;
    AuroraeConfigUI *m_ui;
};

} // namespace

#endif
