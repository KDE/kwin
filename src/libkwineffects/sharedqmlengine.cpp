/*
    SPDX-FileCopyrightText: 2022 MBition GmbH
    SPDX-FileContributor: Kai Uwe Broulik <kai_uwe.broulik@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sharedqmlengine.h"

#include <QQmlContext>
#include <QQmlEngine>

#include <KLocalizedContext>

namespace KWin
{

std::weak_ptr<QQmlEngine> SharedQmlEngine::s_engine;

SharedQmlEngine::Ptr SharedQmlEngine::engine()
{
    auto ret = s_engine.lock();
    if (!ret) {
        ret = std::make_shared<QQmlEngine>();

        auto *i18nContext = new KLocalizedContext(ret.get());
        ret->rootContext()->setContextObject(i18nContext);

        s_engine = ret;
    }
    return ret;
}

} // namespace KWin
