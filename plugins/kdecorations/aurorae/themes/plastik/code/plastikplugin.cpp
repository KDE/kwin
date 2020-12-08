/*
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "plastikplugin.h"
#include "plastikbutton.h"
#include <QQmlEngine>

void PlastikPlugin::registerTypes(const char *uri)
{
    // Need to register something to tell Qt that it loaded (QTBUG-84571)
    qmlRegisterModule(uri, 1, 0);
}

void PlastikPlugin::initializeEngine(QQmlEngine *engine, const char *uri)
{
    Q_ASSERT(QLatin1String(uri) == QLatin1String("org.kde.kwin.decorations.plastik"));
    engine->addImageProvider(QLatin1String("plastik"), new KWin::PlastikButtonProvider());
    QQmlExtensionPlugin::initializeEngine(engine, uri);
}
