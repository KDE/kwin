/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "plugin.h"
#include "buttonsmodel.h"
#include "previewbridge.h"
#include "previewbutton.h"
#include "previewclient.h"
#include "previewitem.h"
#include "previewsettings.h"

#include <KDecoration2/Decoration>
#include <KDecoration2/DecorationShadow>

namespace KDecoration2
{
namespace Preview
{

void Plugin::registerTypes(const char *uri)
{
    Q_ASSERT(QLatin1String(uri) == QLatin1String("org.kde.kwin.private.kdecoration"));
    qmlRegisterType<KDecoration2::Preview::BridgeItem>(uri, 1, 0, "Bridge");
    qmlRegisterType<KDecoration2::Preview::Settings>(uri, 1, 0, "Settings");
    qmlRegisterType<KDecoration2::Preview::PreviewItem>(uri, 1, 0, "Decoration");
    qmlRegisterType<KDecoration2::Preview::PreviewButtonItem>(uri, 1, 0, "Button");
    qmlRegisterType<KDecoration2::Preview::ButtonsModel>(uri, 1, 0, "ButtonsModel");
    qmlRegisterAnonymousType<KDecoration2::Preview::PreviewClient>(uri, 1);
    qmlRegisterAnonymousType<KDecoration2::Decoration>(uri, 1);
    qmlRegisterAnonymousType<KDecoration2::DecorationShadow>(uri, 1);
    qmlRegisterAnonymousType<KDecoration2::Preview::PreviewBridge>(uri, 1);
}

}
}
