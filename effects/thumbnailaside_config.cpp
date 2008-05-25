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

#include "thumbnailaside_config.h"

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

ThumbnailAsideEffectConfigForm::ThumbnailAsideEffectConfigForm(QWidget* parent) : QWidget(parent)
{
  setupUi(this);
}

ThumbnailAsideEffectConfig::ThumbnailAsideEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    kDebug() ;

    m_ui = new ThumbnailAsideEffectConfigForm(this);

    QGridLayout* layout = new QGridLayout(this);

    layout->addWidget(m_ui, 0, 0);

    connect(m_ui->editor, SIGNAL(keyChange()), this, SLOT(changed()));
    connect(m_ui->spinWidth, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->spinSpacing, SIGNAL(valueChanged(int)), this, SLOT(changed()));
    connect(m_ui->spinOpacity, SIGNAL(valueChanged(int)), this, SLOT(changed()));

    // Shortcut config
    m_actionCollection = new KActionCollection( this, componentData() );
    m_actionCollection->setConfigGroup("ThumbnailAside");
    m_actionCollection->setConfigGlobal(true);

    KAction* a = (KAction*)m_actionCollection->addAction( "ToggleCurrentThumbnail" );
    a->setText( i18n("Toggle Thumbnail for Current Window" ));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::CTRL + Qt::Key_T));
    a->setProperty("isConfigurationAction", true);

    load();
    }

ThumbnailAsideEffectConfig::~ThumbnailAsideEffectConfig()
    {
    // Undo (only) unsaved changes to global key shortcuts
    m_ui->editor->undoChanges();
    kDebug() ;
    }

void ThumbnailAsideEffectConfig::load()
    {
    kDebug() ;
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("ThumbnailAside");

    int width = conf.readEntry("MaxWidth", 200);
    int spacing = conf.readEntry("Spacing", 10);
    int opacity = conf.readEntry("Opacity", 50);
    m_ui->spinWidth->setValue(width);
    m_ui->spinSpacing->setValue(spacing);
    m_ui->spinOpacity->setValue(opacity);

    m_actionCollection->readSettings();
    m_ui->editor->addCollection(m_actionCollection);

    emit changed(false);
    }

void ThumbnailAsideEffectConfig::save()
    {
    kDebug() << "Saving config of ThumbnailAside" ;
    //KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("ThumbnailAside");

    conf.writeEntry("MaxWidth", m_ui->spinWidth->value());
    conf.writeEntry("Spacing", m_ui->spinSpacing->value());
    conf.writeEntry("Opacity", m_ui->spinOpacity->value());

    m_actionCollection->writeSettings();
    m_ui->editor->save();   // undo() will restore to this state from now on

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "thumbnailaside" );
    }

void ThumbnailAsideEffectConfig::defaults()
    {
    kDebug() ;
    m_ui->spinWidth->setValue(200);
    m_ui->spinSpacing->setValue(10);
    m_ui->spinOpacity->setValue(50);
    emit changed(true);
    }


} // namespace

#include "thumbnailaside_config.moc"
