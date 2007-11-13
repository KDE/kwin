/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "diminactive_config.h"

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

DimInactiveEffectConfigForm::DimInactiveEffectConfigForm(QWidget* parent) : QWidget(parent)
{
  setupUi(this);
}

DimInactiveEffectConfig::DimInactiveEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    kDebug() ;

    m_ui = new DimInactiveEffectConfigForm(this);

    QGridLayout* layout = new QGridLayout(this);

    layout->addWidget(m_ui, 0, 0);

    connect(m_ui->spinStrength, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->checkPanel, SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(m_ui->checkGroup, SIGNAL(toggled(bool)), this, SLOT(changed()));

    load();
    }

void DimInactiveEffectConfig::load()
    {
    kDebug() ;
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("DimInactive");

    int strength = conf.readEntry("Strength", 25);
    bool panel = conf.readEntry("DimPanels", false);
    bool group = conf.readEntry("DimByGroup", true);
    m_ui->spinStrength->setValue(strength);
    m_ui->checkPanel->setChecked(panel);
    m_ui->checkGroup->setChecked(group);

    emit changed(false);
    }

void DimInactiveEffectConfig::save()
    {
    kDebug();

    KConfigGroup conf = EffectsHandler::effectConfig("DimInactive");

    conf.writeEntry("Strength", m_ui->spinStrength->value());
    conf.writeEntry("DimPanels", m_ui->checkPanel->isChecked());
    conf.writeEntry("DimByGroup", m_ui->checkGroup->isChecked());

    conf.sync();

    KCModule::save();
    emit changed(false);
    EffectsHandler::sendReloadMessage( "diminactive" );
    }

void DimInactiveEffectConfig::defaults()
    {
    kDebug() ;
    m_ui->spinStrength->setValue(25);
    m_ui->checkPanel->setChecked(false);
    m_ui->checkGroup->setChecked(true);
    emit changed(true);
    }


} // namespace

#include "diminactive_config.moc"
