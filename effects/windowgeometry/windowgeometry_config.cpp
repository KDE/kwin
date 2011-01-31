/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 Thomas Lübking <thomas.luebking@web.de>

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

#include "windowgeometry_config.h"
#include <kwineffects.h>
#include <KActionCollection>
#include <kaction.h>
#include <klocale.h>
#include <kconfiggroup.h>

namespace KWin
{
KWIN_EFFECT_CONFIG_FACTORY

WindowGeometryConfigForm::WindowGeometryConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

WindowGeometryConfig::WindowGeometryConfig(QWidget* parent, const QVariantList& args)
    : KCModule(KWin::EffectFactory::componentData(), parent, args)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(myUi = new WindowGeometryConfigForm(this));

    // Shortcut config. The shortcut belongs to the component "kwin"!
    myActionCollection = new KActionCollection(this, KComponentData("kwin"));
    KAction* a = (KAction*)myActionCollection->addAction("WindowGeometry");
    a->setText(i18n("Toggle KWin composited geometry display"));
    a->setProperty("isConfigurationAction", true);
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_F11));

    myUi->shortcuts->addCollection(myActionCollection);
    connect(myUi->shortcuts,    SIGNAL(keyChange()), this, SLOT(changed()));

    connect(myUi->handleMove,   SIGNAL(toggled(bool)), this, SLOT(changed()));
    connect(myUi->handleResize, SIGNAL(toggled(bool)), this, SLOT(changed()));
}

WindowGeometryConfig::~WindowGeometryConfig()
{
    // Undo (only) unsaved changes to global key shortcuts
    myUi->shortcuts->undoChanges();
}

void WindowGeometryConfig::load()
{
    KCModule::load();
    KConfigGroup conf = EffectsHandler::effectConfig("WindowGeometry");
    myUi->handleMove->setChecked(conf.readEntry("Move", true));
    myUi->handleResize->setChecked(conf.readEntry("Resize", true));
    emit changed(false);
}

void WindowGeometryConfig::save()
{
    KConfigGroup conf = EffectsHandler::effectConfig("WindowGeometry");
    conf.writeEntry("Move",   myUi->handleMove->isChecked());
    conf.writeEntry("Resize", myUi->handleResize->isChecked());
    myUi->shortcuts->save();   // undo() will restore to this state from now on
    conf.sync();
    emit changed(false);
    EffectsHandler::sendReloadMessage("windowgeometry");
}

void WindowGeometryConfig::defaults()
{
    myUi->handleMove->setChecked(true);
    myUi->handleResize->setChecked(true);
    myUi->shortcuts->allDefault();
    emit changed(true);
}
} //namespace
#include "windowgeometry_config.moc"
