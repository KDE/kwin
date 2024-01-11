/*
    windows.h

    SPDX-FileCopyrightText: 1997 Patrick Dowler <dowler@morgul.fsh.uvic.ca>
    SPDX-FileCopyrightText: 2001 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KCModule>
#include <QWidget>

#include "ui_advanced.h"
#include "ui_focus.h"
#include "ui_moving.h"

class QRadioButton;
class QCheckBox;
class QPushButton;
class QGroupBox;
class QLabel;
class QSlider;
class QGroupBox;
class QSpinBox;

class KColorButton;

class KWinOptionsSettings;
class KWinOptionsKDEGlobalsSettings;

class KWinFocusConfigForm : public QWidget, public Ui::KWinFocusConfigForm
{
    Q_OBJECT

public:
    explicit KWinFocusConfigForm(QWidget *parent);
};

class KWinMovingConfigForm : public QWidget, public Ui::KWinMovingConfigForm
{
    Q_OBJECT

public:
    explicit KWinMovingConfigForm(QWidget *parent);
};

class KWinAdvancedConfigForm : public QWidget, public Ui::KWinAdvancedConfigForm
{
    Q_OBJECT

public:
    explicit KWinAdvancedConfigForm(QWidget *parent);
};

class KFocusConfig : public KCModule
{
    Q_OBJECT
public:
    KFocusConfig(bool _standAlone, KWinOptionsSettings *settings, QWidget *parent);

    void load() override;
    void save() override;
    void defaults() override;

protected:
    void initialize(KWinOptionsSettings *settings);

private Q_SLOTS:
    void focusPolicyChanged();
    void updateMultiScreen();
    void updateDefaultIndicator();

private:
    bool standAlone;

    KWinFocusConfigForm *m_ui;
    KWinOptionsSettings *m_settings;

    void updateFocusPolicyExplanatoryText();
};

class KMovingConfig : public KCModule
{
    Q_OBJECT
public:
    KMovingConfig(bool _standAlone, KWinOptionsSettings *settings, QWidget *parent);

    void save() override;

protected:
    void initialize(KWinOptionsSettings *settings);

private:
    KWinOptionsSettings *m_settings;
    bool standAlone;
    KWinMovingConfigForm *m_ui;
};

class KAdvancedConfig : public KCModule
{
    Q_OBJECT
public:
    KAdvancedConfig(bool _standAlone, KWinOptionsSettings *settings, KWinOptionsKDEGlobalsSettings *globalSettings, QWidget *parent);

    void save() override;

protected:
    void initialize(KWinOptionsSettings *settings, KWinOptionsKDEGlobalsSettings *globalSettings);

private:
    bool standAlone;
    KWinAdvancedConfigForm *m_ui;
    KWinOptionsSettings *m_settings;
};
