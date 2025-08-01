/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
module;

#include "effect/effect.h"

#include <QtCore/qmetatype.h>
#include <QtCore/qplugin.h>
#include <QtCore/qtmochelpers.h>
#include <QtCore/qxptype_traits.h>

export module colorpicker;
import :impl;

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(ColorPickerEffect,
                              "metadata.json.stripped",
                              return ColorPickerEffect::supported();)

} // namespace KWin

#include "main.moc"
