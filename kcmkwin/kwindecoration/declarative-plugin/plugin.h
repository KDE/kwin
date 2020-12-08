/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#ifndef KDECOARTIONS_PREVIEW_PLUGIN_H
#define KDECOARTIONS_PREVIEW_PLUGIN_H

#include <QQmlExtensionPlugin>

namespace KDecoration2
{
namespace Preview
{

class Plugin : public QQmlExtensionPlugin
{
    Q_PLUGIN_METADATA(IID "org.kde.kdecoration2")
    Q_OBJECT
public:
    void registerTypes(const char *uri) override;
};

}
}

#endif
