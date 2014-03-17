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
// KConfigSkeleton
#include "windowgeometryconfig.h"

#include <QAction>
#include <kwineffects.h>
#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <kconfiggroup.h>
#include <KAboutData>

namespace KWin
{
KWIN_EFFECT_CONFIG_FACTORY

WindowGeometryConfigForm::WindowGeometryConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

WindowGeometryConfig::WindowGeometryConfig(QWidget* parent, const QVariantList& args)
    : KCModule(KAboutData::pluginData(QStringLiteral("windowgeometry")), parent, args)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(myUi = new WindowGeometryConfigForm(this));

    // Shortcut config. The shortcut belongs to the component "kwin"!
    myActionCollection = new KActionCollection(this, QStringLiteral("kwin"));
    QAction* a = myActionCollection->addAction(QStringLiteral("WindowGeometry"));
    a->setText(i18n("Toggle KWin composited geometry display"));
    a->setProperty("isConfigurationAction", true);

    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::SHIFT + Qt::Key_F11);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::SHIFT + Qt::Key_F11);
    myUi->shortcuts->addCollection(myActionCollection);

    connect(myUi->shortcuts,    SIGNAL(keyChange()), this, SLOT(changed()));

    addConfig(WindowGeometryConfiguration::self(), myUi);

    load();
}

WindowGeometryConfig::~WindowGeometryConfig()
{
    // Undo (only) unsaved changes to global key shortcuts
    myUi->shortcuts->undoChanges();
}

void WindowGeometryConfig::save()
{
    KCModule::save();
    myUi->shortcuts->save();   // undo() will restore to this state from now on
    EffectsHandler::sendReloadMessage(QStringLiteral("windowgeometry"));
}

void WindowGeometryConfig::defaults()
{
    myUi->shortcuts->allDefault();
    emit changed(true);
}

} //namespace
#include "moc_windowgeometry_config.cpp"
