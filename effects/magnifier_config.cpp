/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "magnifier_config.h"

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

MagnifierEffectConfigForm::MagnifierEffectConfigForm(QWidget* parent) : QWidget(parent)
{
  setupUi(this);
}

MagnifierEffectConfig::MagnifierEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    kDebug() ;

    m_ui = new MagnifierEffectConfigForm(this);

    QGridLayout* layout = new QGridLayout(this);

    layout->addWidget(m_ui, 0, 0);

    connect(m_ui->editor, SIGNAL(keyChange()), this, SLOT(changed()));
    connect(m_ui->spinWidth, SIGNAL(valueChanged(int)), this, SLOT(changed()));

    // Shortcut config
    KGlobalAccel::self()->overrideMainComponentData(componentData());
    m_actionCollection = new KActionCollection( this, componentData() );
    m_actionCollection->setConfigGroup("Magnifier");
    m_actionCollection->setConfigGlobal(true);

    KAction* a;
    a = static_cast< KAction* >( m_actionCollection->addAction( KStandardAction::ZoomIn, this, SLOT( zoomIn())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Plus));
    a = static_cast< KAction* >( m_actionCollection->addAction( KStandardAction::ZoomOut, this, SLOT( zoomOut())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Minus));
    a = static_cast< KAction* >( m_actionCollection->addAction( KStandardAction::ActualSize, this, SLOT( toggle())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_0));

    load();
    }

void MagnifierEffectConfig::load()
    {
    kDebug() ;
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("Magnifier");

    int width = conf.readEntry("Width", 200);
    int height = conf.readEntry("Height", 200);
    m_ui->spinWidth->setValue(width);
    m_ui->spinHeight->setValue(height);

    m_actionCollection->readSettings();
    m_ui->editor->addCollection(m_actionCollection);

    emit changed(false);
    }

void MagnifierEffectConfig::save()
    {
    kDebug() << "Saving config of Magnifier" ;
    //KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("Magnifier");

    conf.writeEntry("Width", m_ui->spinWidth->value());
    conf.writeEntry("Height", m_ui->spinHeight->value());

    m_actionCollection->writeSettings();

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "magnifier" );
    }

void MagnifierEffectConfig::defaults()
    {
    kDebug() ;
    m_ui->spinWidth->setValue(200);
    m_ui->spinHeight->setValue(200);
    emit changed(true);
    }


} // namespace

#include "magnifier_config.moc"
