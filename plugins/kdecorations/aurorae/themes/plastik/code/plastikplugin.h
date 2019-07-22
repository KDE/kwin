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
#ifndef PLASTIK_PLUGIN_H
#define PLASTIK_PLUGIN_H

#include <QQmlExtensionPlugin>

class PlastikPlugin : public QQmlExtensionPlugin
{
    Q_PLUGIN_METADATA(IID "org.kde.kwin.decorations.plastik")
    Q_OBJECT
public:
    void registerTypes(const char *uri) override;
    void initializeEngine(QQmlEngine *engine, const char *uri) override;
};

#endif // PLASTIK_PLUGIN_H
