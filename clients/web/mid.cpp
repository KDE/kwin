/*
  'Mid' fullscreen kwin client

  Copyright (C) 2009 Marco Martin <notmart@gmail.com>

  Adapted from Web by
  Copyright (C) 2005 Sandro Giessl <sandro@giessl.com>
  Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#include "mid.h"

#include <QPainter>
#include <QPaintEvent>

#include <kconfiggroup.h>
#include <klocale.h>

#include <kdebug.h>

extern "C"
{
  KDE_EXPORT KDecorationFactory *create_factory()
  {
    return new Mid::MidFactory();
  }
}

namespace Mid {

MidClient::MidClient(KDecorationBridge* bridge, KDecorationFactory* factory)
  : KCommonDecoration(bridge, factory)
{
}

MidClient::~MidClient()
{
  // Empty.
}

QString MidClient::visibleName() const
{
    return i18n("Mid");
}

QString MidClient::defaultButtonsLeft() const
{
    return QString();
}

QString MidClient::defaultButtonsRight() const
{
    return QString();
}

bool MidClient::decorationBehaviour(DecorationBehaviour behaviour) const
{
    switch (behaviour) {
    case DB_MenuClose:
        return false;

    case DB_WindowMask:
        return true;

    case DB_ButtonHide:
        return true;

    default:
        return KCommonDecoration::decorationBehaviour(behaviour);
    }
}

int MidClient::layoutMetric(LayoutMetric lm, bool respectWindowState, const KCommonDecorationButton *btn) const
{
    if (maximizeMode() == MaximizeFull) {
        return 0;
    }

    switch (lm) {
    case LM_BorderLeft:
    case LM_BorderRight:
    case LM_BorderBottom:
        return borderSize_;

    case LM_TitleEdgeLeft:
    case LM_TitleEdgeRight:
    case LM_TitleEdgeTop:
    case LM_TitleEdgeBottom:
        return 0;

    case LM_TitleBorderLeft:
    case LM_TitleBorderRight:
        return 0;

    case LM_TitleHeight:
    case LM_ButtonWidth:
    case LM_ButtonHeight:
      return borderSize_;

    case LM_ButtonSpacing:
        return 0;

    default:
        return KCommonDecoration::layoutMetric(lm, respectWindowState, btn);
    }
}

KCommonDecorationButton *MidClient::createButton(ButtonType type)
{
    Q_UNUSED(type)
    return 0;
}

  void
MidClient::init()
{
    // title height
    const int textVMargin   = 2;
    QFontMetrics fm(options()->font(isActive(), isToolWindow()));

    // border size
    switch(options()->preferredBorderSize( factory())) {
    case BorderLarge:
        borderSize_ = 8;
        break;
    case BorderVeryLarge:
        borderSize_ = 12;
        break;
    case BorderHuge:
        borderSize_ = 18;
        break;
    case BorderVeryHuge:
        borderSize_ = 27;
        break;
    case BorderOversized:
        borderSize_ = 40;
        break;
    case BorderNormal:
    default:
        borderSize_ = 4;
    }

    shape_ = true;

    KCommonDecoration::init();
    KCommonDecoration::maximize(MaximizeFull);
}

void MidClient::reset( unsigned long changed )
{
    if (changed & SettingColors) {
      // repaint the whole thing
      widget()->repaint();
    } else if (changed & SettingFont) {
      widget()->repaint();
    }

    KCommonDecoration::reset(changed);
}

void MidClient::maximizeChange()
{
    shape_ = (maximizeMode() != MaximizeFull);
}

void MidClient::paintEvent(QPaintEvent * pe)
{
    //FIXME: this should be elsewhere
    KCommonDecoration::maximize(MaximizeFull);

    if (maximizeMode() == MaximizeFull) {
        return;
    }


    QPainter p(widget());
    QPalette pal = options()->palette(ColorFrame, isActive());
    pal.setCurrentColorGroup( QPalette::Active );
    p.setBrush(pal.background());
    p.setPen(pal.midlight().color());
    p.drawRect(widget()->rect().adjusted(0,0,-1,-1));

    if (shape_) {
        int r(width());
        int b(height());

        // Draw edge of top-left corner inside the area removed by the mask.

        p.drawPoint(3, 1);
        p.drawPoint(4, 1);
        p.drawPoint(2, 2);
        p.drawPoint(1, 3);
        p.drawPoint(1, 4);

        // Draw edge of top-right corner inside the area removed by the mask.

        p.drawPoint(r - 5, 1);
        p.drawPoint(r - 4, 1);
        p.drawPoint(r - 3, 2);
        p.drawPoint(r - 2, 3);
        p.drawPoint(r - 2, 4);

        // Draw edge of bottom-left corner inside the area removed by the mask.

        p.drawPoint(1, b - 5);
        p.drawPoint(1, b - 4);
        p.drawPoint(2, b - 3);
        p.drawPoint(3, b - 2);
        p.drawPoint(4, b - 2);

        // Draw edge of bottom-right corner inside the area removed by the mask.

        p.drawPoint(r - 2, b - 5);
        p.drawPoint(r - 2, b - 4);
        p.drawPoint(r - 3, b - 3);
        p.drawPoint(r - 4, b - 2);
        p.drawPoint(r - 5, b - 2);
    }
}

void MidClient::updateWindowShape()
{
    if (!shape_) {
        return;
    }

    QRegion mask(0, 0, width(), height());

    int r(width());
    int b(height());

    // Remove top-left corner.

    mask -= QRegion(0, 0, 5, 1);
    mask -= QRegion(0, 1, 3, 1);
    mask -= QRegion(0, 2, 2, 1);
    mask -= QRegion(0, 3, 1, 2);

    // Remove top-right corner.

    mask -= QRegion(r - 5, 0, 5, 1);
    mask -= QRegion(r - 3, 1, 3, 1);
    mask -= QRegion(r - 2, 2, 2, 1);
    mask -= QRegion(r - 1, 3, 1, 2);

    // Remove bottom-left corner.

    mask -= QRegion(0, b - 5, 1, 3);
    mask -= QRegion(0, b - 3, 2, 1);
    mask -= QRegion(0, b - 2, 3, 1);
    mask -= QRegion(0, b - 1, 5, 1);

    // Remove bottom-right corner.

    mask -= QRegion(r - 5, b - 1, 5, 1);
    mask -= QRegion(r - 3, b - 2, 3, 1);
    mask -= QRegion(r - 2, b - 3, 2, 1);
    mask -= QRegion(r - 1, b - 5, 1, 2);

    setMask(mask);
}

KDecoration* MidFactory::createDecoration( KDecorationBridge* b )
{
    return(new MidClient(b, this))->decoration();
}

bool MidFactory::reset(unsigned long changed)
{
    // Do we need to "hit the wooden hammer" ?
    bool needHardReset = true;
    if ((changed & ~(SettingColors | SettingFont | SettingButtons)) == 0 ) {
      needHardReset = false;
    }

    if (needHardReset) {
      return true;
    } else {
      resetDecorations(changed);
      return false;
    }
}

bool MidFactory::supports( Ability ability ) const
{
    switch (ability) {
    // announce
    case AbilityAnnounceButtons:
    case AbilityAnnounceColors:
    // colors:
    case AbilityColorTitleBack:
    case AbilityColorTitleFore:
        return true;
    // buttons
    case AbilityButtonOnAllDesktops:
    case AbilityButtonHelp:
    case AbilityButtonMinimize:
    case AbilityButtonMaximize:
    case AbilityButtonClose:
    case AbilityButtonMenu:
    case AbilityButtonAboveOthers:
    case AbilityButtonBelowOthers:
    case AbilityButtonShade:
    case AbilityButtonSpacer:
    default:
        return false;
    };
}

QList< MidFactory::BorderSize > MidFactory::borderSizes() const
{ // the list must be sorted
    return QList< BorderSize >() << BorderNormal << BorderLarge <<
        BorderVeryLarge <<  BorderHuge << BorderVeryHuge << BorderOversized;
}

}

#include "mid.moc"

