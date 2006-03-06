/*
 *	This file contains the B2 configuration widget
 *
 *	Copyright (c) 2001
 *		Karol Szwed <gallium@kde.org>
 *		http://gallium.n3.net/
 */

#include "config.h"
#include <kglobal.h>


//Added by qt3to4:
#include <QLabel>
#include <klocale.h>
#include <kvbox.h>


extern "C"
{
	KDE_EXPORT QObject* allocate_config( KConfig* conf, QWidget* parent )
	{
		return(new B2Config(conf, parent));
	}
}


/* NOTE:
 * 'conf' 	is a pointer to the kwindecoration modules open kwin config,
 *			and is by default set to the "Style" group.
 *
 * 'parent'	is the parent of the QObject, which is a VBox inside the
 *			Configure tab in kwindecoration
 */

B2Config::B2Config( KConfig* conf, QWidget* parent )
	: QObject( parent )
{
	KGlobal::locale()->insertCatalog("kwin_b2_config");
	b2Config = new KConfig("kwinb2rc");
	gb = new KVBox(parent);

	cbColorBorder = new QCheckBox(
			i18n("Draw window frames using &titlebar colors"), gb);
	cbColorBorder->setWhatsThis(
			i18n("When selected, the window borders "
				"are drawn using the titlebar colors; otherwise, they are "
				"drawn using normal border colors."));

	// Grab Handle
    showGrabHandleCb = new QCheckBox(
	    i18n("Draw &resize handle"), gb);
    showGrabHandleCb->setWhatsThis(
	    i18n("When selected, decorations are drawn with a \"grab handle\" "
		 "in the bottom right corner of the windows; "
		 "otherwise, no grab handle is drawn."));

    // Double click menu option support
    actionsGB = new Q3GroupBox(i18n("Actions Settings"), gb);
	actionsGB->setOrientation(Qt::Horizontal);
    QLabel *menuDblClickLabel = new QLabel(actionsGB);
    menuDblClickLabel->setText(i18n("Double click on menu button:"));
    menuDblClickOp = new QComboBox(actionsGB);
    menuDblClickOp->insertItem(i18n("Do Nothing"));
    menuDblClickOp->insertItem(i18n("Minimize Window"));
    menuDblClickOp->insertItem(i18n("Shade Window"));
    menuDblClickOp->insertItem(i18n("Close Window"));

    menuDblClickOp->setWhatsThis(
	    i18n("An action can be associated to a double click "
		 "of the menu button. Leave it to none if in doubt."));

	// Load configuration options
	load(conf);

	// Ensure we track user changes properly
	connect(cbColorBorder, SIGNAL(clicked()),
			this, SLOT(slotSelectionChanged()));
    connect(showGrabHandleCb, SIGNAL(clicked()),
		    this, SLOT(slotSelectionChanged()));
    connect(menuDblClickOp, SIGNAL(activated(int)),
		    this, SLOT(slotSelectionChanged()));
	// Make the widgets visible in kwindecoration
	gb->show();
}


B2Config::~B2Config()
{
    delete b2Config;
	delete gb;
}


void B2Config::slotSelectionChanged()
{
	emit changed();
}


// Loads the configurable options from the kwinrc config file
// It is passed the open config from kwindecoration to improve efficiency
void B2Config::load(KConfig * /*conf*/)
{
	b2Config->setGroup("General");

	bool override = b2Config->readEntry("UseTitleBarBorderColors", QVariant(false)).toBool();
	cbColorBorder->setChecked(override);

    override = b2Config->readEntry( "DrawGrabHandle", QVariant(true )).toBool();
    showGrabHandleCb->setChecked(override);

    QString returnString = b2Config->readEntry(
					"MenuButtonDoubleClickOperation", "NoOp");

    int op;
    if (returnString == "Close") {
		op = 3;
	} else if (returnString == "Shade") {
		op = 2;
    } else if (returnString == "Minimize") {
		op = 1;
    } else {
		op = 0;
    }

    menuDblClickOp->setCurrentItem(op);

}

static QString opToString(int op)
{
    switch (op) {
    case 1:
	    return "Minimize";
    case 2:
	    return "Shade";
    case 3:
	    return "Close";
    case 0:
    default:
	    return "NoOp";
    }
}


// Saves the configurable options to the kwinrc config file
void B2Config::save(KConfig * /*conf*/)
{
	b2Config->setGroup("General");
	b2Config->writeEntry("UseTitleBarBorderColors", cbColorBorder->isChecked());
    b2Config->writeEntry("DrawGrabHandle", showGrabHandleCb->isChecked());
    b2Config->writeEntry("MenuButtonDoubleClickOperation",
	    opToString(menuDblClickOp->currentIndex()));
	// Ensure others trying to read this config get updated
	b2Config->sync();
}


// Sets UI widget defaults which must correspond to style defaults
void B2Config::defaults()
{
	cbColorBorder->setChecked(false);
    showGrabHandleCb->setChecked(true);
    menuDblClickOp->setCurrentItem(0);
}

#include "config.moc"
// vim: ts=4
