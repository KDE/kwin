/* Plastik KWin window decoration
  Copyright (C) 2003 Sandro Giessl <ceebx@users.sourceforge.net>

  based on the window decoration "Web":
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
  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
 */

// #include <kwin/options.h>

#include <qbitmap.h>
#include <qcursor.h>
#include <qimage.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <kpixmap.h>
#include <kpixmapeffect.h>
#include <qtooltip.h>
#include <qtimer.h>

#include "plastikbutton.h"
#include "plastikbutton.moc"
#include "plastikclient.h"
#include "misc.h"
#include "shadow.h"

namespace KWinPlastik
{

static const uint TIMERINTERVAL = 50; // msec
static const uint ANIMATIONSTEPS = 4;

PlastikButton::PlastikButton(ButtonType type, PlastikClient *parent, const char *name)
    : KCommonDecorationButton(type, parent, name),
    m_client(parent),
    m_pixmapType(BtnClose),
    hover(false)
{
    setBackgroundMode(NoBackground);

    // no need to reset here as the button will be resetted on first resize.

    animTmr = new QTimer(this);
    connect(animTmr, SIGNAL(timeout() ), this, SLOT(animate() ) );
    animProgress = 0;
}

PlastikButton::~PlastikButton()
{
}

void PlastikButton::reset(unsigned long changed)
{
    if (changed&DecorationReset || changed&ManualReset || changed&SizeChange || changed&StateChange) {
        switch (type() ) {
            case CloseButton:
                m_pixmapType = BtnClose;
                break;
            case HelpButton:
                m_pixmapType = BtnHelp;
                break;
            case MinButton:
                m_pixmapType = BtnMin;
                break;
            case MaxButton:
                if (isOn()) {
                    m_pixmapType = BtnMaxRestore;
                } else {
                    m_pixmapType = BtnMax;
                }
                break;
            case OnAllDesktopsButton:
                if (isOn()) {
                    m_pixmapType = BtnNotOnAllDesktops;
                } else {
                    m_pixmapType = BtnOnAllDesktops;
                }
                break;
            case ShadeButton:
                if (isOn()) {
                    m_pixmapType = BtnShadeRestore;
                } else {
                    m_pixmapType = BtnShade;
                }
                break;
            case AboveButton:
                if (isOn()) {
                    m_pixmapType = BtnNotAbove;
                } else {
                    m_pixmapType = BtnAbove;
                }
                break;
            case BelowButton:
                if (isOn()) {
                    m_pixmapType = BtnNotBelow;
                } else {
                    m_pixmapType = BtnBelow;
                }
                break;
            default:
                m_pixmapType = NumButtonPixmaps; // invalid...
                break;
        }

        this->update();
    }
}

void PlastikButton::animate()
{
    animTmr->stop();

    if(hover) {
        if(animProgress < ANIMATIONSTEPS) {
            if (Handler()->animateButtons() ) {
                animProgress++;
            } else {
                animProgress = ANIMATIONSTEPS;
            }
            animTmr->start(TIMERINTERVAL, true); // single-shot
        }
    } else {
        if(animProgress > 0) {
            if (Handler()->animateButtons() ) {
                animProgress--;
            } else {
                animProgress = 0;
            }
            animTmr->start(TIMERINTERVAL, true); // single-shot
        }
    }

    repaint(false);
}

void PlastikButton::enterEvent(QEvent *e)
{
    QButton::enterEvent(e);

    hover = true;
    animate();
}

void PlastikButton::leaveEvent(QEvent *e)
{
    QButton::leaveEvent(e);

    hover = false;
    animate();
}

void PlastikButton::drawButton(QPainter *painter)
{
    QRect r(0,0,width(),height());

    bool active = m_client->isActive();
    QPixmap backgroundTile = m_client->getTitleBarTile(active);
    KPixmap tempKPixmap;

    QColor highlightColor;
    if(type() == CloseButton) {
        highlightColor = QColor(255,64,0);
    } else {
        highlightColor = Qt::white;
    }

    QColor contourTop = alphaBlendColors(Handler()->getColor(TitleGradient2, active),
            Qt::black, 220);
    QColor contourBottom = alphaBlendColors(Handler()->getColor(TitleGradient3, active),
            Qt::black, 220);
    QColor sourfaceTop = alphaBlendColors(Handler()->getColor(TitleGradient2, active),
            Qt::white, 220);
    QColor sourfaceBottom = alphaBlendColors(Handler()->getColor(TitleGradient3, active),
            Qt::white, 220);

    int highlightAlpha = static_cast<int>(255-((60/static_cast<double>(ANIMATIONSTEPS))*
                                          static_cast<double>(animProgress) ) );
    contourTop = alphaBlendColors(contourTop, highlightColor, highlightAlpha );
    contourBottom = alphaBlendColors(contourBottom, highlightColor, highlightAlpha);
    sourfaceTop = alphaBlendColors(sourfaceTop, highlightColor, highlightAlpha);
    sourfaceBottom = alphaBlendColors(sourfaceBottom, highlightColor, highlightAlpha);

    if (isDown() ) {
        contourTop = alphaBlendColors(contourTop, Qt::black, 200);
        contourBottom = alphaBlendColors(contourBottom, Qt::black, 200);
        sourfaceTop = alphaBlendColors(sourfaceTop, Qt::black, 200);
        sourfaceBottom = alphaBlendColors(sourfaceBottom, Qt::black, 200);
    }

    QPixmap buffer;
    buffer.resize(width(), height());
    QPainter bP(&buffer);

    // fill with the titlebar background
    bP.drawTiledPixmap(0, 0, width(), width(), backgroundTile);

    if (type() != MenuButton || hover || animProgress != 0) {
        // contour
        bP.setPen(contourTop);
        bP.drawLine(r.x()+2, r.y(), r.right()-2, r.y() );
        bP.drawPoint(r.x()+1, r.y()+1);
        bP.drawPoint(r.right()-1, r.y()+1);
        bP.setPen(contourBottom);
        bP.drawLine(r.x()+2, r.bottom(), r.right()-2, r.bottom() );
        bP.drawPoint(r.x()+1, r.bottom()-1);
        bP.drawPoint(r.right()-1, r.bottom()-1);
        // sides of the contour
        tempKPixmap.resize(1, r.height()-2*2);
        KPixmapEffect::gradient(tempKPixmap,
                                contourTop,
                                contourBottom,
                                KPixmapEffect::VerticalGradient);
        bP.drawPixmap(r.x(), r.y()+2, tempKPixmap);
        bP.drawPixmap(r.right(), r.y()+2, tempKPixmap);
        // sort of anti-alias for the contour
        bP.setPen(alphaBlendColors(Handler()->getColor(TitleGradient2, active),
                contourTop, 150) );
        bP.drawPoint(r.x()+1, r.y());
        bP.drawPoint(r.right()-1, r.y());
        bP.drawPoint(r.x(), r.y()+1);
        bP.drawPoint(r.right(), r.y()+1);
        bP.setPen(alphaBlendColors(Handler()->getColor(TitleGradient3, active),
                contourBottom, 150) );
        bP.drawPoint(r.x()+1, r.bottom());
        bP.drawPoint(r.right()-1, r.bottom());
        bP.drawPoint(r.x(), r.bottom()-1);
        bP.drawPoint(r.right(), r.bottom()-1);
        // sourface
        // fill top and bottom
        bP.setPen(sourfaceTop);
        bP.drawLine(r.x()+2, r.y()+1, r.right()-2, r.y()+1 );
        bP.setPen(sourfaceBottom);
        bP.drawLine(r.x()+2, r.bottom()-1, r.right()-2, r.bottom()-1 );
        // fill the rest! :)
        tempKPixmap.resize(1, r.height()-2*2);
        KPixmapEffect::gradient(tempKPixmap,
                                sourfaceTop,
                                sourfaceBottom,
                                KPixmapEffect::VerticalGradient);
        bP.drawTiledPixmap(r.x()+1, r.y()+2, r.width()-2, r.height()-4, tempKPixmap);
    }

    if (type() == MenuButton)
    {
        QPixmap menuIcon(m_client->icon().pixmap( QIconSet::Small, QIconSet::Normal));
        if (width() < menuIcon.width() || height() < menuIcon.height() ) {
            menuIcon.convertFromImage( menuIcon.convertToImage().smoothScale(width(), height()));
        }
        bP.drawPixmap((width()-menuIcon.width())/2, (height()-menuIcon.height())/2, menuIcon);
    }
    else
    {
        int dX,dY;
        const QPixmap &icon = Handler()->buttonPixmap(m_pixmapType, size(), isDown(), active, decoration()->isToolWindow() );
        dX = r.x()+(r.width()-icon.width())/2;
        dY = r.y()+(r.height()-icon.height())/2;
        if (isDown() ) {
            dY++;
        }
        bP.drawPixmap(dX, dY, icon);
    }

    bP.end();
    painter->drawPixmap(0, 0, buffer);
}

} // KWinPlastik
