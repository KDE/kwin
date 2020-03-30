/*
 * windows.h
 *
 * Copyright (c) 1997 Patrick Dowler dowler@morgul.fsh.uvic.ca
 * Copyright (c) 2001 Waldo Bastian bastian@kde.org
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef KKWMWINDOWS_H
#define KKWMWINDOWS_H

#include <QWidget>
#include <kcmodule.h>

#include "ui_advanced.h"
#include "ui_focus.h"
#include "ui_moving.h"

class QRadioButton;
class QCheckBox;
class QPushButton;
class KComboBox;
class QGroupBox;
class QLabel;
class QSlider;
class QGroupBox;
class QSpinBox;

class KColorButton;

class KWinOptionsSettings;

class KWinFocusConfigForm : public QWidget, public Ui::KWinFocusConfigForm
{
    Q_OBJECT

public:
    explicit KWinFocusConfigForm(QWidget* parent);
};

class KWinMovingConfigForm : public QWidget, public Ui::KWinMovingConfigForm
{
    Q_OBJECT

public:
    explicit KWinMovingConfigForm(QWidget* parent);
};

class KWinAdvancedConfigForm : public QWidget, public Ui::KWinAdvancedConfigForm
{
    Q_OBJECT

public:
    explicit KWinAdvancedConfigForm(QWidget* parent);
};

class KFocusConfig : public KCModule
{
    Q_OBJECT
public:
    KFocusConfig(bool _standAlone, KWinOptionsSettings *settings, QWidget *parent);

    void load() override;
    void save() override;
    void defaults() override;

Q_SIGNALS:
    void unmanagedWidgetDefaulted(bool defaulted);
    void unmanagedWidgetStateChanged(bool changed);

protected:
    void showEvent(QShowEvent *ev) override;

private Q_SLOTS:
    void focusPolicyChanged();
    void updateMultiScreen();

private:

    bool     standAlone;

    KWinFocusConfigForm *m_ui;
    KWinOptionsSettings *m_settings;
};

class KMovingConfig : public KCModule
{
    Q_OBJECT
public:
    KMovingConfig(bool _standAlone, KWinOptionsSettings *settings, QWidget *parent);

    void save() override;

protected:
    void showEvent(QShowEvent *ev) override;

private:
    KWinOptionsSettings *m_config;
    bool     standAlone;
    KWinMovingConfigForm *m_ui;
};

class KAdvancedConfig : public KCModule
{
    Q_OBJECT
public:
    KAdvancedConfig(bool _standAlone, KWinOptionsSettings *settings, QWidget *parent);

    void save() override;

protected:
    void showEvent(QShowEvent *ev) override;

private:

    bool     standAlone;
    KWinAdvancedConfigForm *m_ui;
    KWinOptionsSettings *m_settings;
};

#endif // KKWMWINDOWS_H
