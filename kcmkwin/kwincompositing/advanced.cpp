/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "advanced.h"
#include "advanced.moc"
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

    connect(ui.compositingType, SIGNAL(currentIndexChanged(int)), this, SLOT(compositingModeChanged()));

    connect(ui.compositingType, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.glMode, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.glTextureFilter, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.glDirect, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.glVSync, SIGNAL(toggled(bool)), this, SLOT(changed()));

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

void KWinAdvancedCompositingOptions::compositingModeChanged()
{
    ui.glGroup->setEnabled(ui.compositingType->currentIndex() == 0);
}

void KWinAdvancedCompositingOptions::load()
{
    KConfigGroup config(mKWinConfig, "Compositing");
    QString backend = config.readEntry("Backend", "OpenGL");
    ui.compositingType->setCurrentIndex((backend == "XRender") ? 1 : 0);
    QString glMode = config.readEntry("GLMode", "TFP");
    ui.glMode->setCurrentIndex((glMode == "TFP") ? 0 : ((glMode == "SHM") ? 1 : 2));
    ui.glTextureFilter->setCurrentIndex(config.readEntry("GLTextureFilter", 1));
    ui.glDirect->setChecked(config.readEntry("GLDirect", true));
    ui.glVSync->setChecked(config.readEntry("GLVSync", true));
}

void KWinAdvancedCompositingOptions::save()
{
    // Is this ok?
    if (!isButtonEnabled(KDialog::Apply)) {
        return;
    }

    KConfigGroup config(mKWinConfig, "Compositing");
    config.writeEntry("Backend", (ui.compositingType->currentIndex() == 0) ? "OpenGL" : "XRender");
    QString glModes[] = { "TFP", "SHM", "Fallback" };
    config.writeEntry("GLMode", glModes[ui.glMode->currentIndex()]);
    config.writeEntry("GLTextureFilter", ui.glTextureFilter->currentIndex());
    config.writeEntry("GLDirect", ui.glDirect->isChecked());
    config.writeEntry("GLVSync", ui.glVSync->isChecked());

    enableButtonApply(false);
    emit configSaved();
}

} // namespace

