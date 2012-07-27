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
#include "decorationoptions.h"
#include <kdecoration.h>

namespace KWin
{

DecorationOptions::DecorationOptions(QObject *parent)
    : QObject(parent)
    , m_active(true)
    , m_decoration(NULL)
{
    connect(this, SIGNAL(decorationChanged()), SLOT(slotActiveChanged()));
    connect(this, SIGNAL(decorationChanged()), SIGNAL(colorsChanged()));
    connect(this, SIGNAL(decorationChanged()), SIGNAL(fontChanged()));
}

DecorationOptions::~DecorationOptions()
{
}

QColor DecorationOptions::borderColor() const
{
    return KDecoration::options()->color(KDecorationDefines::ColorFrame, m_active);
}

QColor DecorationOptions::buttonColor() const
{
    return KDecoration::options()->color(KDecorationDefines::ColorButtonBg, m_active);
}

QColor DecorationOptions::fontColor() const
{
    return KDecoration::options()->color(KDecorationDefines::ColorFont, m_active);
}

QColor DecorationOptions::resizeHandleColor() const
{
    return KDecoration::options()->color(KDecorationDefines::ColorHandle, m_active);
}

QColor DecorationOptions::titleBarBlendColor() const
{
    return KDecoration::options()->color(KDecorationDefines::ColorTitleBlend, m_active);
}

QColor DecorationOptions::titleBarColor() const
{
    return KDecoration::options()->color(KDecorationDefines::ColorTitleBar, m_active);
}

QFont DecorationOptions::titleFont() const
{
    return KDecoration::options()->font(m_active);
}

QString DecorationOptions::titleButtonsLeft() const
{
    if (KDecoration::options()->customButtonPositions()) {
        return KDecoration::options()->titleButtonsLeft();
    } else {
        return KDecorationOptions::defaultTitleButtonsLeft();
    }
}

QString DecorationOptions::titleButtonsRight() const
{
    if (KDecoration::options()->customButtonPositions()) {
        return KDecoration::options()->titleButtonsRight();
    } else {
        return KDecorationOptions::defaultTitleButtonsRight();
    }
}

QObject *DecorationOptions::decoration() const
{
    return m_decoration;
}

void DecorationOptions::setDecoration(QObject *decoration)
{
    if (m_decoration == decoration) {
        return;
    }
    if (m_decoration) {
        // disconnect from existing decoration
        disconnect(m_decoration, SIGNAL(activeChanged()), this, SLOT(slotActiveChanged()));
        disconnect(m_decoration, SIGNAL(buttonsChanged()), this, SIGNAL(titleButtonsChanged()));
    }
    m_decoration = decoration;
    connect(m_decoration, SIGNAL(activeChanged()), SLOT(slotActiveChanged()));
    connect(m_decoration, SIGNAL(buttonsChanged()), SIGNAL(titleButtonsChanged()));
    emit decorationChanged();
}

void DecorationOptions::slotActiveChanged()
{
    if (!m_decoration) {
        return;
    }
    if (m_active == m_decoration->property("active").toBool()) {
        return;
    }
    m_active = m_decoration->property("active").toBool();
    emit colorsChanged();
    emit fontChanged();
}

} // namespace

#include "decorationoptions.moc"
