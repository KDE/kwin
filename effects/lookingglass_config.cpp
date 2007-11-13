/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "lookingglass_config.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <kconfiggroup.h>
#include <KActionCollection>
#include <kaction.h>
#include <KShortcutsEditor>
#include <KGlobalAccel>

#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>

KWIN_EFFECT_CONFIG_FACTORY

namespace KWin
{

LookingGlassEffectConfigForm::LookingGlassEffectConfigForm(QWidget* parent) : QWidget(parent)
{
  setupUi(this);
}

LookingGlassEffectConfig::LookingGlassEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    kDebug() ;

    m_ui = new LookingGlassEffectConfigForm(this);

    QGridLayout* layout = new QGridLayout(this);

    layout->addWidget(m_ui, 0, 0);

    connect(m_ui->editor, SIGNAL(keyChange()), this, SLOT(changed()));
    connect(m_ui->radiusSpin, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    
    // Shortcut config
    KGlobalAccel::self()->overrideMainComponentData(componentData());
    m_actionCollection = new KActionCollection( this, componentData() );
    m_actionCollection->setConfigGroup("LookingGlass");
    m_actionCollection->setConfigGlobal(true);
    
    KAction* a;
    a = static_cast< KAction* >( m_actionCollection->addAction( KStandardAction::ZoomIn, this, SLOT( zoomIn())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Plus));
    a = static_cast< KAction* >( m_actionCollection->addAction( KStandardAction::ZoomOut, this, SLOT( zoomOut())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Minus));
    a = static_cast< KAction* >( m_actionCollection->addAction( KStandardAction::ActualSize, this, SLOT( toggle())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_0));

    //m_ui->editor->addCollection(m_actionCollection);

    load();
    }

void LookingGlassEffectConfig::load()
    {
    kDebug() ;
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("LookingGlass");

//    m_ui->editor->readSettings(&conf);

    int radius = conf.readEntry("Radius", 200);
    m_ui->radiusSpin->setValue(radius);

    m_actionCollection->readSettings();
    m_ui->editor->addCollection(m_actionCollection);

    emit changed(false);
    }

void LookingGlassEffectConfig::save()
    {
    kDebug() << "Saving config of LookingGlass" ;
    //KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("LookingGlass");

    conf.writeEntry("Radius", m_ui->radiusSpin->value());

    m_actionCollection->writeSettings();

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "lookingglass" );
    }

void LookingGlassEffectConfig::defaults()
    {
    kDebug() ;
    m_ui->radiusSpin->setValue(200);
    emit changed(true);
    }


} // namespace

#include "lookingglass_config.moc"
