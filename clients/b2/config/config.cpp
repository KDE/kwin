/********************************************************************
 	This file contains the B2 configuration widget
 
 	Copyright (c) 2001
 		Karol Szwed <gallium@kde.org>
 		http://gallium.n3.net/
 	Copyright (c) 2007
 		Luciano Montanaro <mikelima@cirulla.net>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "config.h" 
#include <kglobal.h>
#include <kconfiggroup.h>

//Added by qt3to4:
#include <QLabel>
#include <QGridLayout>
#include <QSpacerItem>
#include <klocale.h>
#include <kvbox.h>


extern "C" KDE_EXPORT QObject *allocate_config(KConfig *conf, QWidget *parent)
{
    return(new B2Config(conf, parent));
}

/* NOTE:
 * 'conf' 	is a pointer to the kwindecoration modules open kwin config,
 *			and is by default set to the "Style" group.
 *
 * 'parent'	is the parent of the QObject, which is a VBox inside the
 *			Configure tab in kwindecoration
 */

B2Config::B2Config(KConfig *conf, QWidget *parent)
	: QObject(parent)
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
    actionsGB = new QGroupBox(i18n("Actions Settings"), gb);
    //actionsGB->setOrientation(Qt::Horizontal);
    QLabel *menuDblClickLabel = new QLabel(actionsGB);
    menuDblClickLabel->setText(i18n("Double click on menu button:"));
    menuDblClickOp = new QComboBox(actionsGB);
    menuDblClickOp->addItem(i18n("Do Nothing"));
    menuDblClickOp->addItem(i18n("Minimize Window"));
    menuDblClickOp->addItem(i18n("Shade Window"));
    menuDblClickOp->addItem(i18n("Close Window"));

    menuDblClickOp->setWhatsThis(
	    i18n("An action can be associated to a double click "
		"of the menu button. Leave it to none if in doubt."));

    QGridLayout *actionsLayout = new QGridLayout();
    QSpacerItem *actionsSpacer = new QSpacerItem(8, 8, 
	    QSizePolicy::Expanding, QSizePolicy::Fixed);
    actionsLayout->addWidget(menuDblClickLabel, 0, 0, Qt::AlignRight);
    actionsLayout->addWidget(menuDblClickOp, 0, 1);
    actionsLayout->addItem(actionsSpacer, 0, 2);
    actionsGB->setLayout(actionsLayout);
    // Load configuration options
    KConfigGroup cg(b2Config, "General");
    load(cg);

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
void B2Config::load(const KConfigGroup & /*conf*/)
{
    KConfigGroup cg(b2Config, "General");

    bool override = cg.readEntry("UseTitleBarBorderColors", false);
    cbColorBorder->setChecked(override);

    override = cg.readEntry("DrawGrabHandle", true);
    showGrabHandleCb->setChecked(override);

    QString returnString = cg.readEntry(
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

    menuDblClickOp->setCurrentIndex(op);
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
void B2Config::save(KConfigGroup & /*conf*/)
{
    KConfigGroup cg(b2Config, "General");
    cg.writeEntry("UseTitleBarBorderColors", cbColorBorder->isChecked());
    cg.writeEntry("DrawGrabHandle", showGrabHandleCb->isChecked());
    cg.writeEntry("MenuButtonDoubleClickOperation", 
	    opToString(menuDblClickOp->currentIndex()));
    // Ensure others trying to read this config get updated
    b2Config->sync();
}

// Sets UI widget defaults which must correspond to style defaults
void B2Config::defaults()
{
    cbColorBorder->setChecked(false);
    showGrabHandleCb->setChecked(true);
    menuDblClickOp->setCurrentIndex(0);
}

#include "config.moc"
// vi: sw=4 ts=8
