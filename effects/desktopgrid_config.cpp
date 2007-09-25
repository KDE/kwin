/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "desktopgrid_config.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <KActionCollection>
#include <kaction.h>
#include <KShortcutsEditor>
#include <KGlobalAccel>

#include <QVBoxLayout>
#include <QCheckBox>

KWIN_EFFECT_CONFIG_FACTORY

namespace KWin
{

DesktopGridEffectConfig::DesktopGridEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    kDebug() ;

    QVBoxLayout* layout = new QVBoxLayout(this);

    mSlide = new QCheckBox(i18n("Animate desktop changes"), this);
    connect(mSlide, SIGNAL(toggled(bool)), this, SLOT(changed()));
    layout->addWidget(mSlide);

    KGlobalAccel::self()->overrideMainComponentData(componentData());
    KActionCollection* actionCollection = new KActionCollection( this, KComponentData("kwin") );
    KAction* show = static_cast<KAction*>(actionCollection->addAction( "ShowDesktopGrid" ));
    show->setText( i18n("Show Desktop Grid" ));
    show->setGlobalShortcut( KShortcut( Qt::CTRL + Qt::Key_F8 ));

    KShortcutsEditor* shortcutEditor = new KShortcutsEditor(actionCollection, this,
            KShortcutsEditor::GlobalAction, KShortcutsEditor::LetterShortcutsDisallowed);
    connect(shortcutEditor, SIGNAL(keyChange()), this, SLOT(changed()));
    layout->addWidget(shortcutEditor);

    layout->addStretch();

    load();
    }

DesktopGridEffectConfig::~DesktopGridEffectConfig()
    {
    kDebug() ;
    }

void DesktopGridEffectConfig::load()
    {
    kDebug() ;
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("DesktopGrid");
    mSlide->setChecked(conf.readEntry("Slide", true));

    emit changed(false);
    }

void DesktopGridEffectConfig::save()
    {
    kDebug() ;
    KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("DesktopGrid");
    conf.writeEntry("Slide", mSlide->isChecked());

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "desktopgrid" );
    }

void DesktopGridEffectConfig::defaults()
    {
    kDebug() ;
    mSlide->setChecked(true);
    emit changed(true);
    }


} // namespace

#include "desktopgrid_config.moc"
