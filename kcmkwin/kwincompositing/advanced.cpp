/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

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

#include "advanced.h"
#include "advanced.moc"
#include <klocale.h>

#include <QtDBus/QtDBus>
#include <kmessagebox.h>

#include "compositingprefs.h"
#include "main.h"
#include "kwin_interface.h"

namespace KWin
{

KWinAdvancedCompositingOptions::KWinAdvancedCompositingOptions(QWidget* parent, KSharedConfigPtr config, CompositingPrefs* defaults) :
        KDialog(parent)
{
    mKWinConfig = config;
    mDefaultPrefs = defaults;

    setCaption(i18n("Advanced Compositing Options"));
    setButtons(KDialog::Ok | KDialog::Cancel | KDialog::Apply);
    setModal(true);

    QWidget* mainWidget = new QWidget(this);
    ui.setupUi(mainWidget);
    setMainWidget(mainWidget);

    connect(ui.compositingType, SIGNAL(currentIndexChanged(int)), this, SLOT(compositingModeChanged()));

    connect(ui.compositingType, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.windowThumbnails, SIGNAL(activated(int)), this, SLOT(changed()));
    connect(ui.disableChecks, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.glMode, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.glTextureFilter, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    connect(ui.glDirect, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(ui.glVSync, SIGNAL(toggled(bool)), this, SLOT(changed()));

    connect(ui.xrenderSmoothScale, SIGNAL(toggled(bool)), this, SLOT(changed()));

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
    ui.xrenderGroup->setEnabled(ui.compositingType->currentIndex() == 1);
}

void KWinAdvancedCompositingOptions::showConfirmDialog()
{
    bool revert = false;
    // Feel free to extend this to support several kwin instances (multihead) if you
    // think it makes sense.
    OrgKdeKWinInterface kwin("org.kde.kwin", "/KWin", QDBusConnection::sessionBus());
    if( !kwin.compositingActive().value())
    {
        KMessageBox::sorry( this, i18n(
            "Failed to activate desktop effects using the given "
            "configuration options. Settings will be reverted to their previous values.\n\n"
            "Check your X configuration. You may also consider changing advanced options, "
            "especially changing the compositing type." ));
        revert = true;
    }
    else
    {
        ConfirmDialog confirm;
        if( !confirm.exec())
            revert = true;
    }
    if( revert )
    {
        // Revert settings
        KConfigGroup config(mKWinConfig, "Compositing");
        config.deleteGroup();
        QMap<QString, QString>::const_iterator it = mPreviousConfig.constBegin();
        for(; it != mPreviousConfig.constEnd(); ++it)
        {
            if (it.value().isEmpty())
                continue;
            config.writeEntry(it.key(), it.value());
        }
        // Reinit KWin compositing and reload (old) settings
        reinitKWinCompositing();
        load();
    }
}

void KWinAdvancedCompositingOptions::load()
{
    KConfigGroup config(mKWinConfig, "Compositing");
    QString backend = config.readEntry("Backend", "OpenGL");
    ui.compositingType->setCurrentIndex((backend == "XRender") ? 1 : 0);
    // 4 - off, 5 - shown, 6 - always, other are old values
    int hps = config.readEntry("HiddenPreviews", 5);
    if( hps == 6 ) // always
        ui.windowThumbnails->setCurrentIndex( 0 );
    else if( hps == 4 ) // never
        ui.windowThumbnails->setCurrentIndex( 2 );
    else // shown, or default
        ui.windowThumbnails->setCurrentIndex( 1 );
    ui.disableChecks->setChecked( config.readEntry( "DisableChecks", false ));

    QString glMode = config.readEntry("GLMode", "TFP");
    ui.glMode->setCurrentIndex((glMode == "TFP") ? 0 : ((glMode == "SHM") ? 1 : 2));
    ui.glTextureFilter->setCurrentIndex(config.readEntry("GLTextureFilter", 1));
    ui.glDirect->setChecked(config.readEntry("GLDirect", mDefaultPrefs->enableDirectRendering()));
    ui.glVSync->setChecked(config.readEntry("GLVSync", mDefaultPrefs->enableVSync()));

    ui.xrenderSmoothScale->setChecked(config.readEntry("XRenderSmoothScale", false));

    compositingModeChanged();
}

void KWinAdvancedCompositingOptions::save()
{
    // Is this ok?
    if (!isButtonEnabled(KDialog::Apply)) {
        return;
    }

    KConfigGroup config(mKWinConfig, "Compositing");
    mPreviousConfig = config.entryMap();
    bool showConfirm = false;
    QString glModes[] = { "TFP", "SHM", "Fallback" };
    
    if( config.readEntry("Backend", "OpenGL")
            != ((ui.compositingType->currentIndex() == 0) ? "OpenGL" : "XRender")
        || config.readEntry("GLMode", "TFP") != glModes[ui.glMode->currentIndex()]
        || config.readEntry("GLDirect", mDefaultPrefs->enableDirectRendering())
            != ui.glDirect->isChecked()
        || config.readEntry("GLVSync", mDefaultPrefs->enableVSync()) != ui.glVSync->isChecked()
        || config.readEntry("DisableChecks", false ) != ui.disableChecks->isChecked())
        {
        showConfirm = true;
        }

    config.writeEntry("Backend", (ui.compositingType->currentIndex() == 0) ? "OpenGL" : "XRender");
    static const int hps[] = { 6 /*always*/, 5 /*shown*/,  4 /*never*/ };
    config.writeEntry("HiddenPreviews", hps[ ui.windowThumbnails->currentIndex() ] );
    config.writeEntry("DisableChecks", ui.disableChecks->isChecked());

    config.writeEntry("GLMode", glModes[ui.glMode->currentIndex()]);
    config.writeEntry("GLTextureFilter", ui.glTextureFilter->currentIndex());
    config.writeEntry("GLDirect", ui.glDirect->isChecked());
    config.writeEntry("GLVSync", ui.glVSync->isChecked());

    config.writeEntry("XRenderSmoothScale", ui.xrenderSmoothScale->isChecked());

    enableButtonApply(false);

    reinitKWinCompositing();
    if( showConfirm )
        showConfirmDialog();
}

void KWinAdvancedCompositingOptions::reinitKWinCompositing()
{
    // Send signal to kwin
    mKWinConfig->sync();
    // Send signal to all kwin instances
    // If this ever changes to using the 'reconfigure' DBUS call, change the compositingActive()
    // check above.
    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reinitCompositing");
    QDBusConnection::sessionBus().send(message);
}

} // namespace

