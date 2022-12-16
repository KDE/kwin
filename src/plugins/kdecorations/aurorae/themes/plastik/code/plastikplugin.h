/*
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QQmlExtensionPlugin>

class PlastikPlugin : public QQmlExtensionPlugin
{
    Q_PLUGIN_METADATA(IID "org.kde.kwin.decorations.plastik")
    Q_OBJECT
public:
    void registerTypes(const char *uri) override;
    void initializeEngine(QQmlEngine *engine, const char *uri) override;
};
