/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screens.h"

#include <config-kwin.h>

#include "core/output.h"
#include "core/platform.h"
#include "cursor.h"
#include "settings.h"
#include "utils/common.h"
#include <window.h>
#include <workspace.h>

namespace KWin
{

Screens::Screens()
    : m_maxScale(1.0)
{
    connect(workspace(), &Workspace::outputsChanged, this, &Screens::changed);
}

void Screens::init()
{
    connect(this, &Screens::changed, this, &Screens::updateSize);

    Q_EMIT changed();
}

qreal Screens::scale(int screen) const
{
    if (Output *output = findOutput(screen)) {
        return output->scale();
    }
    return 1.0;
}

qreal Screens::maxScale() const
{
    return m_maxScale;
}

void Screens::updateSize()
{
    qreal maxScale = 1.0;
    for (int i = 0; i < workspace()->outputs().count(); ++i) {
        maxScale = qMax(maxScale, scale(i));
    }
    if (!qFuzzyCompare(m_maxScale, maxScale)) {
        m_maxScale = maxScale;
        Q_EMIT maxScaleChanged();
    }
}

Output *Screens::findOutput(int screen) const
{
    return workspace()->outputs().value(screen).get();
}

} // namespace
