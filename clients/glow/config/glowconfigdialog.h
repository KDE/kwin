/***************************************************************************
                          glowconfigdialog.h  -  description
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

#ifndef GLOW_CONFIG_DIALOG_H
#define GLOW_CONFIG_DIALOG_H

#include <vector>
#include <map>
#include <qobject.h>

class QPushButton;
class QSignalMapper;
class QRadioButton;
class KConfig;
class KColorButton;

class GlowConfigDialog : public QObject
{
	Q_OBJECT

public:
	struct ButtonConfig
	{
		// one of "TitleBar", "TitleBlend", "Custom"
		QString glowType;
		QColor glowColor;
	};

	GlowConfigDialog( KConfig* conf, QWidget* parent );
	~GlowConfigDialog();

signals:
	void changed();

public slots:
	void load( KConfig* conf );
	void save( KConfig* conf );
	void defaults();

protected slots:
	void slotSelectionChanged();	// Internal use
	void slotTitleButtonClicked(int);
	void slotColorRadioButtonClicked(int);
	void slotColorButtonChanged(const QColor&);

private:
	KConfig *m_glowConfig;
	map<QString, ButtonConfig> m_buttonConfigMap;

	QGroupBox *m_mainGroupBox;

	QPushButton *m_stickyButton;
	QPushButton *m_helpButton;
	QPushButton *m_iconifyButton;
	QPushButton *m_maximizeButton;
	QPushButton *m_closeButton;
	vector<QPushButton*> m_titleButtonList;
	QSignalMapper *m_titleButtonMapper;

	QRadioButton *m_titleBarColorButton;
	QRadioButton *m_titleBlendColorButton;
	QRadioButton *m_customColorButton;
	vector<QRadioButton*> m_colorRadioButtonList;
	QSignalMapper *m_colorRadioButtonMapper;
	KColorButton *m_colorButton;

	void updateUI();
	void updateConfig();
};

#endif
