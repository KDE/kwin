/*
 * Copyright (c) 2019 Valerio Pilo <vpilo@coldshock.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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
