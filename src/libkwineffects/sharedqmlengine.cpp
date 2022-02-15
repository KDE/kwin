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

QWeakPointer<QQmlEngine> SharedQmlEngine::s_engine;

SharedQmlEngine::Ptr SharedQmlEngine::engine()
{
    if (!s_engine) {
        Ptr ptr(new QQmlEngine());

        auto *i18nContext = new KLocalizedContext(ptr.data());
        ptr->rootContext()->setContextObject(i18nContext);

        s_engine = ptr;
        return ptr;
    }

    return s_engine.toStrongRef();
}

} // namespace KWin
