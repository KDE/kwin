/*
	$Id$

	This is the new kwindecoration kcontrol module

	Copyright (c) 2001
		Karol Szwed <gallium@kde.org>
		http://gallium.n3.net/

	Supports new kwin configuration plugins, and titlebar button position
	modification via dnd interface.

	Based on original "kwintheme" (Window Borders) 
	Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>
*/

#ifndef KWINDECORATION_H
#define KWINDECORATION_H

#include <kcmodule.h>
#include <dcopobject.h>
#include <buttons.h>
#include <kconfig.h>
#include <klibloader.h>

#include "kwindecorationIface.h"

class QListBox;
class QCheckBox;
class QTabWidget;
class QVBox;

// Stores themeName and its corresponding library Name
struct DecorationInfo
{
	QString name;
	QString libraryName;
};


class KWinDecorationModule : public KCModule, virtual public KWinDecorationIface
{
	Q_OBJECT

	public:
		KWinDecorationModule(QWidget* parent, const char* name, const QStringList &);
		~KWinDecorationModule();

		virtual void load();
		virtual void save();
		virtual void defaults();

		QString quickHelp() const;
		const KAboutData* aboutData() const;

		virtual void dcopUpdateClientList();

	signals:
		void changed(bool);
		void pluginLoad( KConfig* conf );
		void pluginSave( KConfig* conf );
		void pluginDefaults();

	protected slots:
		// Allows us to turn "save" on
		void slotSelectionChanged();
		void slotEnableButtonTab(bool on);
		void slotDecorationHighlighted( const QString& s );

	private:
		void readConfig( KConfig* conf );
		void writeConfig( KConfig* conf );
		void findDecorations();
		void createDecorationList();
		void updateSelection();
		QString decorationLibName( const QString& name );
		QString decorationName ( QString& libName );
		void resetPlugin( KConfig* conf, const QString* currentDecoName = 0 );
		void resetKWin();

		QTabWidget* tabWidget;

		// Page 1
		QListBox* decorationListBox;
		QValueList<DecorationInfo> decorations;
		QCheckBox* cbUseCustomButtonPositions;
	//	QCheckBox* cbUseMiniWindows;
		QCheckBox* cbShowToolTips;

		// Page 2
		ButtonDropSite* dropSite;
		ButtonSource* buttonSource;

		// Page 3
		QObject* pluginObject;
		QString  currentLibraryName;
		QString  oldLibraryName;
		QVBox*   pluginPage;
		QVBox*	 buttonPage;
		QObject* (*allocatePlugin)( KConfig* conf, QWidget* parent );
};


#endif
// vim: ts=4
