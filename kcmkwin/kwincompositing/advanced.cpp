/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "advanced.h"

#include <klocale.h>

namespace KWin
{

KWinAdvancedCompositingOptions::KWinAdvancedCompositingOptions(QWidget* parent, KSharedConfigPtr config) :
        KDialog(parent)
{
    mKWinConfig = config;

    setCaption(i18n("Advanced compositing options"));
    setButtons(KDialog::Ok | KDialog::Cancel | KDialog::Apply);
    setModal(true);

    QWidget* mainWidget = new QWidget(this);
    ui.setupUi(mainWidget);
    setMainWidget(mainWidget);

    ui.compositingType->insertItem(0, i18n("OpenGL"));
    ui.compositingType->insertItem(1, i18n("XRender"));

    connect(ui.compositingType, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));

    connect(this, SIGNAL(okClicked()), this, SLOT(save()));
    connect(this, SIGNAL(applyClicked()), this, SLOT(save()));

    load();

    enableButtonApply(false);
}

KWinAdvancedCompositingOptions::~KWinAdvancedCompositingOptions()
{
}

void KWinAdvancedCompositingOptions::changed()
{
    enableButtonApply(true);
}

void KWinAdvancedCompositingOptions::load()
{
    KConfigGroup config(mKWinConfig, "Compositing");
    QString backend = config.readEntry("Backend", "OpenGL");
    ui.compositingType->setCurrentIndex((backend == "XRender") ? 1 : 0);
}

void KWinAdvancedCompositingOptions::save()
{
    // Is this ok?
    if (!isButtonEnabled(KDialog::Apply)) {
        return;
    }

    KConfigGroup config(mKWinConfig, "Compositing");
    config.writeEntry("Backend", (ui.compositingType->currentIndex() == 0) ? "OpenGL" : "XRender");

    enableButtonApply(false);
    emit configSaved();
}

} // namespace

