/********************************************************************
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "colorhelper.h"

#include <KDE/KGlobalSettings>
#include <KDE/KColorScheme>

ColorHelper::ColorHelper(QObject *parent)
    : QObject(parent)
{
}

ColorHelper::~ColorHelper()
{
}

QColor ColorHelper::shade(const QColor &color, ColorHelper::ShadeRole role)
{
    return KColorScheme::shade(color, static_cast<KColorScheme::ShadeRole>(role));
}

QColor ColorHelper::shade(const QColor &color, ColorHelper::ShadeRole role, qreal contrast)
{
    return KColorScheme::shade(color, static_cast<KColorScheme::ShadeRole>(role), contrast);
}

qreal ColorHelper::contrast() const
{
    return KGlobalSettings::contrastF();
}

QColor ColorHelper::multiplyAlpha(const QColor &color, qreal alpha)
{
    QColor retCol(color);
    retCol.setAlphaF(color.alphaF() * alpha);
    return retCol;
}

QColor ColorHelper::background(bool active, ColorHelper::BackgroundRole role) const
{
    KColorScheme kcs(active ? QPalette::Active : QPalette::Inactive, KColorScheme::Button);
    return kcs.background(static_cast<KColorScheme::BackgroundRole>(role)).color();
}

QColor ColorHelper::foreground(bool active, ColorHelper::ForegroundRole role) const
{
    KColorScheme kcs(active ? QPalette::Active : QPalette::Inactive, KColorScheme::Button);
    return kcs.foreground(static_cast<KColorScheme::ForegroundRole>(role)).color();
}

#include "colorhelper.moc"
