/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screen.h"
#include "core/output.h"
#include "integration.h"
#include "logging.h"
#include "platformcursor.h"

#include <qpa/qwindowsysteminterface.h>

namespace KWin
{
namespace QPA
{

static int forcedDpi()
{
    return qEnvironmentVariableIsSet("QT_WAYLAND_FORCE_DPI") ? qEnvironmentVariableIntValue("QT_WAYLAND_FORCE_DPI") : -1;
}

Screen::Screen(Output *output, Integration *integration)
    : m_output(output)
    , m_cursor(new PlatformCursor)
    , m_integration(integration)
{
    connect(output, &Output::geometryChanged, this, &Screen::handleGeometryChanged);
}

Screen::~Screen() = default;

QList<QPlatformScreen *> Screen::virtualSiblings() const
{
    const auto screens = m_integration->screens();

    QList<QPlatformScreen *> siblings;
    siblings.reserve(siblings.size());

    for (Screen *screen : screens) {
        siblings << screen;
    }

    return siblings;
}

int Screen::depth() const
{
    return 32;
}

QImage::Format Screen::format() const
{
    return QImage::Format_ARGB32_Premultiplied;
}

QRect Screen::geometry() const
{
    if (Q_UNLIKELY(!m_output)) {
        qCCritical(KWIN_QPA) << "Attempting to get the geometry of a destroyed output";
        return QRect();
    }
    return m_output->geometry();
}

QSizeF Screen::physicalSize() const
{
    if (Q_UNLIKELY(!m_output)) {
        qCCritical(KWIN_QPA) << "Attempting to get the physical size of a destroyed output";
        return QSizeF();
    }
    return m_output->physicalSize();
}

QPlatformCursor *Screen::cursor() const
{
    return m_cursor.get();
}

QDpi Screen::logicalDpi() const
{
    const int dpi = forcedDpi();
    return dpi > 0 ? QDpi(dpi, dpi) : QDpi(96, 96);
}

qreal Screen::devicePixelRatio() const
{
    if (Q_UNLIKELY(!m_output)) {
        qCCritical(KWIN_QPA) << "Attempting to get the scale factor of a destroyed output";
        return 1;
    }
    return m_output->scale();
}

QString Screen::name() const
{
    if (Q_UNLIKELY(!m_output)) {
        qCCritical(KWIN_QPA) << "Attempting to get the name of a destroyed output";
        return QString();
    }
    return m_output->name();
}

void Screen::handleGeometryChanged()
{
    QWindowSystemInterface::handleScreenGeometryChange(screen(), geometry(), geometry());
}

QDpi PlaceholderScreen::logicalDpi() const
{
    const int dpi = forcedDpi();
    return dpi > 0 ? QDpi(dpi, dpi) : QDpi(96, 96);
}

}
}
