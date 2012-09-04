/********************************************************************
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "plastikplugin.h"
#include "plastikbutton.h"
#include <QDeclarativeEngine>
Q_EXPORT_PLUGIN2(plastikplugin, PlastikPlugin)

void PlastikPlugin::registerTypes(const char *uri)
{
    Q_UNUSED(uri)
}

void PlastikPlugin::initializeEngine(QDeclarativeEngine *engine, const char *uri)
{
    Q_ASSERT(uri == QLatin1String("org.kde.kwin.decorations.plastik"));
    engine->addImageProvider(QLatin1String("plastik"), new KWin::PlastikButtonProvider());
    QDeclarativeExtensionPlugin::initializeEngine(engine, uri);
}

#include "plastikplugin.moc"
