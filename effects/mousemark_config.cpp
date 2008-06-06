/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

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
    connect(m_ui->comboColors, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));

    // Shortcut config
    m_actionCollection = new KActionCollection( this, componentData() );

    KAction* a = static_cast< KAction* >( m_actionCollection->addAction( "ClearMouseMarks" ));
    a->setText( i18n( "Clear Mouse Marks" ));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut( KShortcut( Qt::SHIFT + Qt::META + Qt::Key_F11 ));

    a = static_cast< KAction* >( m_actionCollection->addAction( "ClearLastMouseMark" ));
    a->setText( i18n( "Clear Last Mouse Mark" ));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut( KShortcut( Qt::SHIFT + Qt::META + Qt::Key_F12 ));

    m_ui->editor->addCollection(m_actionCollection);

    load();
    }

MouseMarkEffectConfig::~MouseMarkEffectConfig()
    {
    // Undo (only) unsaved changes to global key shortcuts
    m_ui->editor->undoChanges();
    }

void MouseMarkEffectConfig::load()
    {
    kDebug() ;
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("MouseMark");

    int width = conf.readEntry("LineWidth", 3);
    QColor color = conf.readEntry("Color", QColor(Qt::red));
    m_ui->spinWidth->setValue(width);
    m_ui->comboColors->setColor(color);

    emit changed(false);
    }

void MouseMarkEffectConfig::save()
    {
    kDebug() << "Saving config of MouseMark" ;
    //KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("MouseMark");

    conf.writeEntry("LineWidth", m_ui->spinWidth->value());
    conf.writeEntry("Color", m_ui->comboColors->color());

    m_actionCollection->writeSettings();
    m_ui->editor->save();   // undo() will restore to this state from now on

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "mousemark" );
    }

void MouseMarkEffectConfig::defaults()
    {
    kDebug() ;
    m_ui->spinWidth->setValue(3);
    m_ui->comboColors->setColor(Qt::red);
    emit changed(true);
    }


} // namespace

#include "mousemark_config.moc"
