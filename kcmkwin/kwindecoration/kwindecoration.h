/*
    This is the new kwindecoration kcontrol module

    Copyright (c) 2001
        Karol Szwed <gallium@kde.org>
        http://gallium.n3.net/
    Copyright 2009, 2010 Martin Gräßlin <kde@martin-graesslin.com>

    Supports new kwin configuration plugins, and titlebar button position
    modification via dnd interface.

    Based on original "kwintheme" (Window Borders)
    Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/

#ifndef KWINDECORATION_H
#define KWINDECORATION_H

#include <kcmodule.h>
#include <kconfig.h>

#include <kdecoration.h>

#include "ui_decoration.h"

class QSortFilterProxyModel;
namespace KWin
{

class DecorationModel;

class KWinDecorationForm : public QWidget, public Ui::KWinDecorationForm
{
    Q_OBJECT

public:
    explicit KWinDecorationForm(QWidget* parent);
};

class DecorationButtons : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool customButtonPositions READ customPositions WRITE setCustomPositions NOTIFY customPositionsChanged)
    Q_PROPERTY(QString leftButtons READ leftButtons WRITE setLeftButtons NOTIFY leftButtonsChanged)
    Q_PROPERTY(QString rightButtons READ rightButtons WRITE setRightButtons NOTIFY rightButtonsChanged)
public:
    explicit DecorationButtons(QObject *parent = 0);
    virtual ~DecorationButtons();

    bool customPositions() const;
    const QString &leftButtons() const;
    const QString &rightButtons() const;

    void setCustomPositions(bool set);
    void setLeftButtons(const QString &leftButtons);
    void setRightButtons(const QString &rightButtons);

public Q_SLOTS:
    void resetToDefaults();

Q_SIGNALS:
    void customPositionsChanged();
    void leftButtonsChanged();
    void rightButtonsChanged();

private:
    bool m_customPositions;
    QString m_leftButtons;
    QString m_rightButtons;
};

class KWinDecorationModule : public KCModule, public KDecorationDefines
{
    Q_OBJECT

public:
    KWinDecorationModule(QWidget* parent, const QVariantList &);
    ~KWinDecorationModule();

    virtual void load();
    virtual void save();
    virtual void defaults();

    QString quickHelp() const;

    int itemWidth() const;

signals:
    void pluginLoad(const KConfigGroup& conf);
    void pluginSave(KConfigGroup &conf);
    void pluginDefaults();

protected slots:
    // Allows us to turn "save" on
    void slotSelectionChanged();
    void slotConfigureButtons();
    void slotGHNSClicked();
    void slotConfigureDecoration();

private:
    void readConfig(const KConfigGroup& conf);
    void writeConfig(KConfigGroup &conf);

    KSharedConfigPtr kwinConfig;

    KWinDecorationForm* m_ui;
    bool m_showTooltips;

    DecorationModel* m_model;
    QSortFilterProxyModel* m_proxyModel;
    bool m_configLoaded;
    DecorationButtons *m_decorationButtons;
};

} //namespace

#endif
