/*
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef DECORATION_PLUGIN_H
#define DECORATION_PLUGIN_H
#include <QQmlExtensionPlugin>

class DecorationPlugin : public QQmlExtensionPlugin
{
    Q_PLUGIN_METADATA(IID "org.kde.kwin.decoration")
    Q_OBJECT
public:
    void registerTypes(const char *uri) override;
};

#endif
