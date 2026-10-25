/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Alexander Berg <wehrwolfmann@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "effect/effect.h"

namespace KWin
{

/**
 * An effect that is loaded from a shared library, like effects that are not shipped with kwin.
 */
class ExternalEffect : public Effect
{
    Q_OBJECT
};

KWIN_EFFECT_FACTORY(ExternalEffect, "metadata.json")

} // namespace KWin

#include "externaleffect.moc"
