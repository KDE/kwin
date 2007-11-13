/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#include "mousemark_config.h"

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

MouseMarkEffectConfigForm::MouseMarkEffectConfigForm(QWidget* parent) : QWidget(parent)
{
  setupUi(this);
}

MouseMarkEffectConfig::MouseMarkEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    kDebug() ;

    m_ui = new MouseMarkEffectConfigForm(this);

    QGridLayout* layout = new QGridLayout(this);

    layout->addWidget(m_ui, 0, 0);

    connect(m_ui->editor, SIGNAL(keyChange()), this, SLOT(changed()));
    connect(m_ui->spinWidth, SIGNAL(valueChanged(int)), this, SLOT(changed()));

    // Shortcut config
    KGlobalAccel::self()->overrideMainComponentData(componentData());
    m_actionCollection = new KActionCollection( this, componentData() );
    m_actionCollection->setConfigGroup("MouseMark");
    m_actionCollection->setConfigGlobal(true);

    KAction* a = static_cast< KAction* >( m_actionCollection->addAction( "ClearMouseMarks" ));
    a->setText( i18n( "Clear Mouse Marks" ));
    a->setGlobalShortcut( KShortcut( Qt::SHIFT + Qt::META + Qt::Key_F11 ));

    load();
    }

void MouseMarkEffectConfig::load()
    {
    kDebug() ;
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("MouseMark");

    int width = conf.readEntry("LineWidth", 200);
    QColor color = conf.readEntry("Color", QColor(255, 0, 0));
    m_ui->spinWidth->setValue(width);
    //m_ui->spinHeight->setValue(height);

    m_actionCollection->readSettings();
    m_ui->editor->addCollection(m_actionCollection);

    emit changed(false);
    }

void MouseMarkEffectConfig::save()
    {
    kDebug() << "Saving config of MouseMark" ;
    //KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("MouseMark");

    conf.writeEntry("LineWidth", m_ui->spinWidth->value());
    //conf.writeEntry("Color", m_ui->spinHeight->value());

    m_actionCollection->writeSettings();

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "magnifier" );
    }

void MouseMarkEffectConfig::defaults()
    {
    kDebug() ;
    m_ui->spinWidth->setValue(3);
    //m_ui->spinHeight->setValue(200);
    emit changed(true);
    }


} // namespace

#include "mousemark_config.moc"
