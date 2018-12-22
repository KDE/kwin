/**************************************************************************
 * KWin - the KDE window manager                                          *
 * This file is part of the KDE project.                                  *
 *                                                                        *
 * Copyright (C) 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>             *
 *                                                                        *
 * This program is free software; you can redistribute it and/or modify   *
 * it under the terms of the GNU General Public License as published by   *
 * the Free Software Foundation; either version 2 of the License, or      *
 * (at your option) any later version.                                    *
 *                                                                        *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 **************************************************************************/


#ifndef MODEL_H
#define MODEL_H

#include <KColorScheme>
#include <KSharedConfig>
#include <QQuickWidget>
#include <QSortFilterProxyModel>
#include <QString>

namespace KWin {

class EffectModel;

namespace Compositing {

class EffectView : public QQuickWidget
{

    Q_OBJECT

public:
    enum ViewType {
        DesktopEffectsView,
        CompositingSettingsView
    };
    EffectView(ViewType type, QWidget *parent = 0);

    void save();
    void load();
    void defaults();

Q_SIGNALS:
    void changed();

private Q_SLOTS:
    void slotImplicitSizeChanged();
private:
    void init(ViewType type);
};


class EffectFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    /**
     * If @c true not supported effects are excluded, if @c false no restriction on supported.
     * Default value is @c true.
     **/
    Q_PROPERTY(bool filterOutUnsupported MEMBER m_filterOutUnsupported NOTIFY filterOutUnsupportedChanged)
    /**
     * If @c true internal effects are excluded, if @c false no restriction on internal.
     * Default value is @c true.
     **/
    Q_PROPERTY(bool filterOutInternal MEMBER m_filterOutInternal NOTIFY filterOutInternalChanged)
    Q_PROPERTY(QColor backgroundActiveColor READ backgroundActiveColor CONSTANT)
    Q_PROPERTY(QColor backgroundNormalColor READ backgroundNormalColor CONSTANT)
    Q_PROPERTY(QColor backgroundAlternateColor READ backgroundAlternateColor CONSTANT)
    Q_PROPERTY(QColor sectionColor READ sectionColor CONSTANT)
public:
    EffectFilterModel(QObject *parent = 0);
    const QString &filter() const;

    Q_INVOKABLE void updateEffectStatus(int rowIndex, int effectState);
    Q_INVOKABLE void syncConfig();
    Q_INVOKABLE void load();

    QColor backgroundActiveColor() { return KColorScheme(QPalette::Active, KColorScheme::Selection, KSharedConfigPtr(0)).background(KColorScheme::LinkBackground).color(); };
    QColor backgroundNormalColor() { return KColorScheme(QPalette::Active, KColorScheme::View, KSharedConfigPtr(0)).background(KColorScheme::NormalBackground).color(); };
    QColor backgroundAlternateColor() { return KColorScheme(QPalette::Active, KColorScheme::View, KSharedConfigPtr(0)).background(KColorScheme::AlternateBackground).color(); };

    QColor sectionColor() const {
        QColor color = KColorScheme(QPalette::Active).foreground().color();
        color.setAlphaF(0.6);
        return color;
    }

    void defaults();

public Q_SLOTS:
    void setFilter(const QString &filter);

protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;

Q_SIGNALS:
    void effectModelChanged();
    void filterChanged();
    void filterOutUnsupportedChanged();
    void filterOutInternalChanged();

private:
    EffectModel *m_effectModel;
    QString m_filter;
    bool m_filterOutUnsupported;
    bool m_filterOutInternal;
};
}
}
#endif
