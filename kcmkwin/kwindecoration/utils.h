/*
    SPDX-FileCopyrightText: 2019 Valerio Pilo <vpilo@coldshock.net>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#pragma once

#include <KDecoration2/DecorationButton>
#include <KSharedConfig>

#include <QVector>


using DecorationButtonsList = QVector<KDecoration2::DecorationButtonType>;

namespace Utils
{

QString buttonsToString(const DecorationButtonsList &buttons);
DecorationButtonsList buttonsFromString(const QString &buttons);
DecorationButtonsList readDecorationButtons(const KConfigGroup &config, const QString &key, const DecorationButtonsList &defaultValue);

KDecoration2::BorderSize stringToBorderSize(const QString &name);
QString borderSizeToString(KDecoration2::BorderSize size);

const QMap<KDecoration2::BorderSize, QString> &getBorderSizeNames();

}
