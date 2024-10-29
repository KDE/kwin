/*
    SPDX-FileCopyrightText: 2019 Valerio Pilo <vpilo@coldshock.net>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#pragma once

#include <KDecoration3/DecorationButton>
#include <KSharedConfig>

#include <QList>

using DecorationButtonsList = QList<KDecoration3::DecorationButtonType>;

namespace Utils
{

QString buttonsToString(const DecorationButtonsList &buttons);
DecorationButtonsList buttonsFromString(const QString &buttons);
DecorationButtonsList readDecorationButtons(const KConfigGroup &config, const QString &key, const DecorationButtonsList &defaultValue);

KDecoration3::BorderSize stringToBorderSize(const QString &name);
QString borderSizeToString(KDecoration3::BorderSize size);

const QMap<KDecoration3::BorderSize, QString> &getBorderSizeNames();
}
