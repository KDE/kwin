/***************************************************************************
                          glowconfigdialog.cpp  -  description
                             -------------------
    begin                : Thu Sep 12 2001
    copyright            : (C) 2001 by Henning Burchardt
    email                : h_burchardt@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qcheckbox.h>
#include <qgroupbox.h>
#include <qwhatsthis.h>
#include <qlayout.h>
#include <qabstractlayout.h>
#include <qpushbutton.h>
#include <qbitmap.h>
#include <qsignalmapper.h>
#include <qradiobutton.h>
#include <kglobal.h>
#include <kconfig.h>
#include <klocale.h>
#include <kcolorbutton.h>

#include "glowconfigdialog.h"
#include "../bitmaps.h"

#define DEFAULT_BUTTON_COLOR Qt::white
#define DEFAULT_CLOSE_BUTTON_COLOR Qt::red

extern "C"
{
	QObject* allocate_config( KConfig* conf, QWidget* parent )
	{
		return(new GlowConfigDialog(conf, parent));
	}
}

GlowConfigDialog::GlowConfigDialog( KConfig * conf, QWidget * parent )
	: QObject(parent)
{
	m_glowConfig = new KConfig("kwinglowrc");
	KGlobal::locale()->insertCatalogue("libkwinglow_config");

	m_mainGroupBox = new QGroupBox(
		1, Qt::Vertical, i18n("Decoration Settings"), parent);

	QGroupBox *buttonGlowColorGroupBox = new QGroupBox(
		0, Qt::Vertical, i18n("Glow Colors"), m_mainGroupBox);

	QHBoxLayout *buttonGlowColorGroupBoxLayout =
		new QHBoxLayout(buttonGlowColorGroupBox->layout());

	QVBoxLayout *buttonLayout = new QVBoxLayout(3);

	// create buttons
	QSize buttonSize(DEFAULT_BITMAP_SIZE, DEFAULT_BITMAP_SIZE);
	QPixmap pm(buttonSize);
	pm.fill(Qt::black);

	m_stickyButton = new QPushButton(buttonGlowColorGroupBox);
	pm.setMask(QBitmap(buttonSize, stickyoff_bits, true));
	m_stickyButton->setPixmap(pm);
	buttonLayout->addWidget(m_stickyButton);
	m_titleButtonList.push_back(m_stickyButton);

	m_helpButton = new QPushButton(buttonGlowColorGroupBox);
	pm.setMask(QBitmap(buttonSize, help_bits, true));
	m_helpButton->setPixmap(pm);
	buttonLayout->addWidget(m_helpButton);
	m_titleButtonList.push_back(m_helpButton);

	m_iconifyButton = new QPushButton(buttonGlowColorGroupBox);
	pm.setMask(QBitmap(buttonSize, minimize_bits, true));
	m_iconifyButton->setPixmap(pm);
	buttonLayout->addWidget(m_iconifyButton);
	m_titleButtonList.push_back(m_iconifyButton);

	m_maximizeButton = new QPushButton(buttonGlowColorGroupBox);
	pm.setMask(QBitmap(buttonSize, maximizeoff_bits, true));
	m_maximizeButton->setPixmap(pm);
	buttonLayout->addWidget(m_maximizeButton);
	m_titleButtonList.push_back(m_maximizeButton);

	m_closeButton = new QPushButton(buttonGlowColorGroupBox);
	pm.setMask(QBitmap(buttonSize, close_bits, true));
	m_closeButton->setPixmap(pm);
	buttonLayout->addWidget(m_closeButton);
	m_titleButtonList.push_back(m_closeButton);

	// create signal mapper
	m_titleButtonMapper = new QSignalMapper(this);
	for( int i=0; i<m_titleButtonList.size(); i++ )
	{
		m_titleButtonMapper->setMapping(m_titleButtonList[i], i);
		QObject::connect(m_titleButtonList[i], SIGNAL(clicked()),
			m_titleButtonMapper, SLOT(map()));
	}
	QObject::connect(m_titleButtonMapper, SIGNAL(mapped(int)),
		this, SLOT(slotTitleButtonClicked(int)));

	buttonGlowColorGroupBoxLayout->addLayout(buttonLayout);
	buttonGlowColorGroupBoxLayout->addSpacing(20);

	QGroupBox *innerButtonGlowColorGroupBox = new QGroupBox(
		0, Qt::Vertical, buttonGlowColorGroupBox);

	QGridLayout *innerButtonGlowColorGroupBoxLayout = new QGridLayout(
		innerButtonGlowColorGroupBox->layout(), 3, 2);
	innerButtonGlowColorGroupBoxLayout->setAlignment(
		Qt::AlignTop | Qt::AlignLeft);

	// create color radio buttons
	m_titleBarColorButton = new QRadioButton(
		i18n("Title bar color"), innerButtonGlowColorGroupBox);
	m_colorRadioButtonList.push_back(m_titleBarColorButton);
	innerButtonGlowColorGroupBoxLayout->addWidget(
		m_titleBarColorButton, 0, 0);

	m_titleBlendColorButton = new QRadioButton(
		i18n("Title blend color"), innerButtonGlowColorGroupBox);
	m_colorRadioButtonList.push_back(m_titleBlendColorButton);
	innerButtonGlowColorGroupBoxLayout->addWidget(
		m_titleBlendColorButton, 1, 0);

	m_customColorButton = new QRadioButton(
		i18n("Custom color"), innerButtonGlowColorGroupBox);
	m_colorRadioButtonList.push_back(m_customColorButton);
	innerButtonGlowColorGroupBoxLayout->addWidget(
		m_customColorButton, 2, 0);

	m_colorRadioButtonMapper = new QSignalMapper(this);
	for(int i=0; i<m_colorRadioButtonList.size(); i++ )
	{
		m_colorRadioButtonMapper->setMapping(m_colorRadioButtonList[i], i);
		QObject::connect(m_colorRadioButtonList[i], SIGNAL(clicked()),
			m_colorRadioButtonMapper, SLOT(map()));
	}
	QObject::connect(m_colorRadioButtonMapper, SIGNAL(mapped(int)),
		this, SLOT(slotColorRadioButtonClicked(int)));

	m_colorButton = new KColorButton(innerButtonGlowColorGroupBox);
	innerButtonGlowColorGroupBoxLayout->addWidget(
		m_colorButton, 2, 1);

	buttonGlowColorGroupBoxLayout->addWidget(innerButtonGlowColorGroupBox);
	buttonGlowColorGroupBoxLayout->addItem(new QSpacerItem(
		20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

	// load config and update user interface
	load(conf);

	m_mainGroupBox->show();
}

GlowConfigDialog::~GlowConfigDialog()
{
	delete m_mainGroupBox;
}

void GlowConfigDialog::load( KConfig* conf )
{
	m_glowConfig->setGroup("General");

	QColor defaultButtonColor = DEFAULT_BUTTON_COLOR;
	QColor defaultCloseButtonColor = DEFAULT_CLOSE_BUTTON_COLOR;
	struct ButtonConfig cnf;

	cnf.glowType = m_glowConfig->readEntry(
		"stickyButtonGlowType", "TitleBar");
	cnf.glowColor = m_glowConfig->readColorEntry(
		"stickyButtonGlowColor", &defaultButtonColor);
	m_buttonConfigMap["stickyButton"] = cnf;

	cnf.glowType = m_glowConfig->readEntry(
		"helpButtonGlowType", "TitleBar");
	cnf.glowColor = m_glowConfig->readColorEntry(
		"helpButtonGlowColor", &defaultButtonColor);
	m_buttonConfigMap["helpButton"] = cnf;

	cnf.glowType = m_glowConfig->readEntry(
		"iconifyButtonGlowType", "TitleBar");
	cnf.glowColor = m_glowConfig->readColorEntry(
		"iconifyButtonGlowColor", &defaultButtonColor);
	m_buttonConfigMap["iconifyButton"] = cnf;

	cnf.glowType = m_glowConfig->readEntry(
		"maximizeButtonGlowType", "TitleBar");
	cnf.glowColor = m_glowConfig->readColorEntry(
		"maximizeButtonGlowColor", &defaultButtonColor);
	m_buttonConfigMap["maximizeButton"] = cnf;

	cnf.glowType = m_glowConfig->readEntry(
		"closeButtonGlowType", "Custom");
	cnf.glowColor = m_glowConfig->readColorEntry(
		"closeButtonGlowColor", &defaultCloseButtonColor);
	m_buttonConfigMap["closeButton"] = cnf;

	updateUI();
}

void GlowConfigDialog::save( KConfig *conf )
{
	updateConfig();

	m_glowConfig->setGroup("General");

	struct ButtonConfig cnf;

	cnf = m_buttonConfigMap["stickyButton"];
	m_glowConfig->writeEntry("stickyButtonGlowType", cnf.glowType);
	m_glowConfig->writeEntry("stickyButtonGlowColor", cnf.glowColor);

	cnf = m_buttonConfigMap["helpButton"];
	m_glowConfig->writeEntry("helpButtonGlowType", cnf.glowType);
	m_glowConfig->writeEntry("helpButtonGlowColor", cnf.glowColor);

	cnf = m_buttonConfigMap["iconifyButton"];
	m_glowConfig->writeEntry("iconifyButtonGlowType", cnf.glowType);
	m_glowConfig->writeEntry("iconifyButtonGlowColor", cnf.glowColor);

	cnf = m_buttonConfigMap["maximizeButton"];
	m_glowConfig->writeEntry("maximizeButtonGlowType", cnf.glowType);
	m_glowConfig->writeEntry("maximizeButtonGlowColor", cnf.glowColor);

	cnf = m_buttonConfigMap["closeButton"];
	m_glowConfig->writeEntry("closeButtonGlowType", cnf.glowType);
	m_glowConfig->writeEntry("closeButtonGlowColor", cnf.glowColor);

	m_glowConfig->sync();
}

void GlowConfigDialog::defaults()
{
	QColor defaultButtonColor = DEFAULT_BUTTON_COLOR;
	QColor defaultCloseButtonColor = DEFAULT_CLOSE_BUTTON_COLOR;
	struct ButtonConfig cnf;

	cnf.glowType = "TitleBar";
	cnf.glowColor = defaultButtonColor;

	m_buttonConfigMap["stickyButton"] = cnf;
	m_buttonConfigMap["helpButton"] = cnf;
	m_buttonConfigMap["iconifyButton"] = cnf;
	m_buttonConfigMap["maximizeButton"] = cnf;

	cnf.glowType = "Custom";
	cnf.glowColor = defaultCloseButtonColor;

	m_buttonConfigMap["closeButton"] = cnf;

	updateUI();
}

void GlowConfigDialog::slotSelectionChanged()
{
	emit changed();
}

void GlowConfigDialog::slotTitleButtonClicked(int index)
{
	updateConfig();
	for( int i=0; i<m_titleButtonList.size(); i++ )
		m_titleButtonList[i]->setDown(i==index);
	updateUI();
}

void GlowConfigDialog::slotColorRadioButtonClicked(int index)
{
	for( int i=0; i<m_colorRadioButtonList.size(); i++ )
		m_colorRadioButtonList[i]->setChecked(i==index);
	m_colorButton->setEnabled(
		m_colorRadioButtonList[index]==m_customColorButton);

	updateConfig();
	slotSelectionChanged();
}

void GlowConfigDialog::slotColorButtonChanged(const QColor&)
{
	updateConfig();
	slotSelectionChanged();
}

void GlowConfigDialog::updateUI()
{
	struct ButtonConfig cnf;
	if( m_stickyButton->isDown() )
		cnf = m_buttonConfigMap["stickyButton"];
	else if( m_helpButton->isDown() )
		cnf = m_buttonConfigMap["helpButton"];
	else if( m_iconifyButton->isDown() )
		cnf = m_buttonConfigMap["iconifyButton"];
	else if( m_maximizeButton->isDown() )
		cnf = m_buttonConfigMap["maximizeButton"];
	else if( m_closeButton->isDown() )
		cnf = m_buttonConfigMap["closeButton"];
	else
	{
		m_stickyButton->setDown(true);
		cnf = m_buttonConfigMap["stickyButton"];
	}

	if( cnf.glowType == "TitleBar" )
	{
		m_titleBarColorButton->setChecked(true);
		m_titleBlendColorButton->setChecked(false);
		m_customColorButton->setChecked(false);
	}
	else if( cnf.glowType == "TitleBlend" )
	{
		m_titleBarColorButton->setChecked(false);
		m_titleBlendColorButton->setChecked(true);
		m_customColorButton->setChecked(false);
	}
	else
	{
		m_titleBarColorButton->setChecked(false);
		m_titleBlendColorButton->setChecked(false);
		m_customColorButton->setChecked(true);
	}
	m_colorButton->setColor(cnf.glowColor);
	m_colorButton->setEnabled(m_customColorButton->isChecked());
}

void GlowConfigDialog::updateConfig()
{
	QString glowType;
	if( m_titleBarColorButton->isChecked() )
		glowType = "TitleBar";
	else if( m_titleBlendColorButton->isChecked() )
		glowType = "TitleBlend";
	else
		glowType = "Custom";
	QColor glowColor = m_colorButton->color();
	if( m_stickyButton->isDown() )
	{
		m_buttonConfigMap["stickyButton"].glowType = glowType;
		m_buttonConfigMap["stickyButton"].glowColor = glowColor;
	}
	else if( m_helpButton->isDown() )
	{
		m_buttonConfigMap["helpButton"].glowType = glowType;
		m_buttonConfigMap["helpButton"].glowColor = glowColor;
	}
	else if( m_iconifyButton->isDown() )
	{
		m_buttonConfigMap["iconifyButton"].glowType = glowType;
		m_buttonConfigMap["iconifyButton"].glowColor = glowColor;
	}
	else if( m_maximizeButton->isDown() )
	{
		m_buttonConfigMap["maximizeButton"].glowType = glowType;
		m_buttonConfigMap["maximizeButton"].glowColor = glowColor;
	}
	else // if( m_closeButton->isDown() )
	{
		m_buttonConfigMap["closeButton"].glowType = glowType;
		m_buttonConfigMap["closeButton"].glowColor = glowColor;
	}
}

#include "glowconfigdialog.moc"
