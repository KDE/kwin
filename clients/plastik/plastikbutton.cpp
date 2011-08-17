/* Plastik KWin window decoration
  Copyright (C) 2003-2005 Sandro Giessl <sandro@giessl.com>

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
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
 */

#include "plastikbutton.h"

// #include <kwin/options.h>

#include <QAbstractButton>
#include <QStyle>
#include <QBitmap>
#include <QPainter>
#include <QPixmap>
#include <QTimer>

#include "plastikbutton.moc"
#include "plastikclient.h"

#include <KGlobalSettings>
#include <KColorScheme>
#include <KColorUtils>

namespace KWinPlastik
{

static const uint TIMERINTERVAL = 50; // msec
static const uint ANIMATIONSTEPS = 4;

PlastikButton::PlastikButton(ButtonType type, PlastikClient *parent)
    : KCommonDecorationButton(type, parent),
    m_client(parent),
    m_iconType(NumButtonIcons),
    hover(false)
{
    setAttribute(Qt::WA_NoSystemBackground);

    // no need to reset here as the button will be reseted on first resize.

    animTmr = new QTimer(this);
    animTmr->setSingleShot(true);  // single-shot
    connect(animTmr, SIGNAL(timeout()), this, SLOT(animate()) );
    animProgress = 0;
}

PlastikButton::~PlastikButton()
{
    animTmr->stop();
    delete animTmr;
}

void PlastikButton::reset(unsigned long changed)
{
    if (changed&DecorationReset || changed&ManualReset || changed&SizeChange || changed&StateChange) {
        switch (type() ) {
            case CloseButton:
                m_iconType = CloseIcon;
                break;
            case HelpButton:
                m_iconType = HelpIcon;
                break;
            case MinButton:
                m_iconType = MinIcon;
                break;
            case MaxButton:
                if (isChecked()) {
                    m_iconType = MaxRestoreIcon;
                } else {
                    m_iconType = MaxIcon;
                }
                break;
            case OnAllDesktopsButton:
                if (isChecked()) {
                    m_iconType = NotOnAllDesktopsIcon;
                } else {
                    m_iconType = OnAllDesktopsIcon;
                }
                break;
            case ShadeButton:
                if (isChecked()) {
                    m_iconType = UnShadeIcon;
                } else {
                    m_iconType = ShadeIcon;
                }
                break;
            case AboveButton:
                if (isChecked()) {
                    m_iconType = NoKeepAboveIcon;
                } else {
                    m_iconType = KeepAboveIcon;
                }
                break;
            case BelowButton:
                if (isChecked()) {
                    m_iconType = NoKeepBelowIcon;
                } else {
                    m_iconType = KeepBelowIcon;
                }
                break;
            default:
                m_iconType = NumButtonIcons; // empty...
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
            animTmr->start(TIMERINTERVAL); // single-shot timer
        }
    } else {
        if(animProgress > 0) {
            if (Handler()->animateButtons() ) {
                animProgress--;
            } else {
                animProgress = 0;
            }
            animTmr->start(TIMERINTERVAL); // single-shot timer
        }
    }

    repaint();
}

void PlastikButton::enterEvent(QEvent *e)
{
    QAbstractButton::enterEvent(e);

    hover = true;
    animate();
}

void PlastikButton::leaveEvent(QEvent *e)
{
    QAbstractButton::leaveEvent(e);

    hover = false;
    animate();
}

void PlastikButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    drawButton(&p);
}

void PlastikButton::drawButton(QPainter *painter)
{
    QRect r(0,0,width(),height());

    bool active = m_client->isActive();
    QPixmap tempPixmap;


    double c = KGlobalSettings::contrastF();
    QColor titleBar = KDecoration::options()->color(KDecoration::ColorTitleBar, active);
    QColor contourTop = KColorScheme::shade(titleBar, KColorScheme::DarkShade, c - 0.4);
    QColor contourBottom = KColorScheme::shade(titleBar, KColorScheme::MidShade, c);
    QColor surfaceTop = KColorScheme::shade(titleBar, KColorScheme::MidlightShade, c - 0.4);
    QColor surfaceBottom = KColorScheme::shade(titleBar, KColorScheme::LightShade, c - 0.4);

    QColor highlightColor = titleBar;
    double alpha = 0;
    if (type() == CloseButton) {
        KColorScheme kcs(active ? QPalette::Active : QPalette::Inactive, KColorScheme::Button);
        highlightColor = kcs.foreground(KColorScheme::NegativeText).color();
    }
    if (isDown()) {
        highlightColor = KColorScheme::shade(highlightColor, KColorScheme::ShadowShade);
        alpha = 0.3;
    }
    else if (animProgress > 0) {
        alpha = 0.6 * (double)animProgress / (double)ANIMATIONSTEPS;
        highlightColor = KColorScheme::shade(highlightColor, KColorScheme::LightShade, qMin(1.0, c + 0.4));
    }

    if (alpha > 0.0) {
        contourTop    = KColorUtils::mix(contourTop,    highlightColor, alpha*0.4);
        contourBottom = KColorUtils::mix(contourBottom, highlightColor, alpha*0.4);
        surfaceTop    = KColorUtils::mix(surfaceTop,    highlightColor, alpha);
        surfaceBottom = KColorUtils::mix(surfaceBottom, highlightColor, alpha);
    }

    QPixmap buffer(width(), height());
    QPainter bP(&buffer);

    // fake the titlebar background
    bP.drawTiledPixmap(0, 0, width(), width(), m_client->getTitleBarTile(active) );

    if (type() != MenuButton || hover || animProgress != 0) {
        qreal rxo = 600/width();
        qreal ryo = 600/height();
        qreal rxi = 500/width();
        qreal ryi = 500/height();
        bP.setPen(Qt::NoPen);
        bP.setRenderHints(QPainter::Antialiasing);
        // contour
        QLinearGradient outlineGradient(0, 0, 0, r.height());
        outlineGradient.setColorAt(0.0, contourTop);
        outlineGradient.setColorAt(1.0, contourBottom);
        bP.setBrush(outlineGradient);
        bP.drawRoundRect(r, rxo, ryo);
        // surface
        QLinearGradient surfaceGradient(0, 0, 0, r.height());
        surfaceGradient.setColorAt(0.0, surfaceTop);
        surfaceGradient.setColorAt(1.0, surfaceBottom);
        bP.setBrush(surfaceGradient);
        bP.drawRoundRect(r.adjusted(1,1,-1,-1), rxi, ryi);
    }

    if (type() == MenuButton)
    {
        QPixmap menuIcon(m_client->icon().pixmap( style()->pixelMetric( QStyle::PM_SmallIconSize ) ));
        if (width() < menuIcon.width() || height() < menuIcon.height() ) {
            menuIcon = menuIcon.scaled(width(), height());
        }
        bP.drawPixmap((width()-menuIcon.width())/2, (height()-menuIcon.height())/2, menuIcon);
    }
    else
    {
        int dX,dY;
        const QBitmap &icon = Handler()->buttonBitmap(m_iconType, size(), decoration()->isToolWindow() );
        dX = r.x()+(r.width()-icon.width())/2;
        dY = r.y()+(r.height()-icon.height())/2;
        if (isDown() ) {
            dY++;
        }

        QColor fontColor = Handler()->getColor(TitleFont,active);
        if(!isDown() && Handler()->titleShadow() ) {
            QColor shadowColor = KColorScheme::shade(fontColor, KColorScheme::ShadowShade);
            shadowColor.setAlphaF(shadowColor.alphaF() * 0.3);
            bP.setPen(shadowColor);
            bP.drawPixmap(dX+1, dY+1, icon);
        }

        bP.setPen(fontColor );
        bP.drawPixmap(dX, dY, icon);
    }

    bP.end();
    painter->drawPixmap(0, 0, buffer);
}

QBitmap IconEngine::icon(ButtonIcon icon, int size)
{
    if (size%2 == 0)
        --size;

    QBitmap bitmap(size,size);
    bitmap.fill(Qt::color0);
    QPainter p(&bitmap);

    p.setPen(Qt::color1);

    QRect r = bitmap.rect();

    // line widths
    int lwTitleBar = 1;
    if (r.width() > 16) {
        lwTitleBar = 4;
    } else if (r.width() > 4) {
        lwTitleBar = 2;
    }
    int lwArrow = 1;
    if (r.width() > 16) {
        lwArrow = 4;
    } else if (r.width() > 7) {
        lwArrow = 2;
    }

    switch(icon) {
        case CloseIcon:
        {
            int lineWidth = 1;
            if (r.width() > 16) {
                lineWidth = 3;
            } else if (r.width() > 4) {
                lineWidth = 2;
            }

            drawObject(p, DiagonalLine, r.x(), r.y(), r.width(), lineWidth);
            drawObject(p, CrossDiagonalLine, r.x(), r.bottom(), r.width(), lineWidth);

            break;
        }

        case MaxIcon:
        {
            int lineWidth2 = 1; // frame
            if (r.width() > 16) {
                lineWidth2 = 2;
            } else if (r.width() > 4) {
                lineWidth2 = 1;
            }

            drawObject(p, HorizontalLine, r.x(), r.top(), r.width(), lwTitleBar);
            drawObject(p, HorizontalLine, r.x(), r.bottom()-(lineWidth2-1), r.width(), lineWidth2);
            drawObject(p, VerticalLine, r.x(), r.top(), r.height(), lineWidth2);
            drawObject(p, VerticalLine, r.right()-(lineWidth2-1), r.top(), r.height(), lineWidth2);

            break;
        }

        case MaxRestoreIcon:
        {
            int lineWidth2 = 1; // frame
            if (r.width() > 16) {
                lineWidth2 = 2;
            } else if (r.width() > 4) {
                lineWidth2 = 1;
            }

            int margin1, margin2;
            margin1 = margin2 = lineWidth2*2;
            if (r.width() < 8)
                margin1 = 1;

            // background window
            drawObject(p, HorizontalLine, r.x()+margin1, r.top(), r.width()-margin1, lineWidth2);
            drawObject(p, HorizontalLine, r.right()-margin2, r.bottom()-(lineWidth2-1)-margin1, margin2, lineWidth2);
            drawObject(p, VerticalLine, r.x()+margin1, r.top(), margin2, lineWidth2);
            drawObject(p, VerticalLine, r.right()-(lineWidth2-1), r.top(), r.height()-margin1, lineWidth2);

            // foreground window
            drawObject(p, HorizontalLine, r.x(), r.top()+margin2, r.width()-margin2, lwTitleBar);
            drawObject(p, HorizontalLine, r.x(), r.bottom()-(lineWidth2-1), r.width()-margin2, lineWidth2);
            drawObject(p, VerticalLine, r.x(), r.top()+margin2, r.height(), lineWidth2);
            drawObject(p, VerticalLine, r.right()-(lineWidth2-1)-margin2, r.top()+margin2, r.height(), lineWidth2);

            break;
        }

        case MinIcon:
        {
            drawObject(p, HorizontalLine, r.x(), r.bottom()-(lwTitleBar-1), r.width(), lwTitleBar);

            break;
        }

        case HelpIcon:
        {
            int center = r.x()+r.width()/2 -1;
            int side = r.width()/4;

            // paint a question mark... code is quite messy, to be cleaned up later...! :o

            if (r.width() > 16) {
                int lineWidth = 3;

                // top bar
                drawObject(p, HorizontalLine, center-side+3, r.y(), 2*side-3-1, lineWidth);
                // top bar rounding
                drawObject(p, CrossDiagonalLine, center-side-1, r.y()+5, 6, lineWidth);
                drawObject(p, DiagonalLine, center+side-3, r.y(), 5, lineWidth);
                // right bar
                drawObject(p, VerticalLine, center+side+2-lineWidth, r.y()+3, r.height()-(2*lineWidth+side+2+1), lineWidth);
                // bottom bar
                drawObject(p, CrossDiagonalLine, center, r.bottom()-2*lineWidth, side+2, lineWidth);
                drawObject(p, HorizontalLine, center, r.bottom()-3*lineWidth+2, lineWidth, lineWidth);
                // the dot
                drawObject(p, HorizontalLine, center, r.bottom()-(lineWidth-1), lineWidth, lineWidth);
            } else if (r.width() > 8) {
                int lineWidth = 2;

                // top bar
                drawObject(p, HorizontalLine, center-(side-1), r.y(), 2*side-1, lineWidth);
                // top bar rounding
                if (r.width() > 9) {
                    drawObject(p, CrossDiagonalLine, center-side-1, r.y()+3, 3, lineWidth);
                } else {
                    drawObject(p, CrossDiagonalLine, center-side-1, r.y()+2, 3, lineWidth);
                }
                drawObject(p, DiagonalLine, center+side-1, r.y(), 3, lineWidth);
                // right bar
                drawObject(p, VerticalLine, center+side+2-lineWidth, r.y()+2, r.height()-(2*lineWidth+side+1), lineWidth);
                // bottom bar
                drawObject(p, CrossDiagonalLine, center, r.bottom()-2*lineWidth+1, side+2, lineWidth);
                // the dot
                drawObject(p, HorizontalLine, center, r.bottom()-(lineWidth-1), lineWidth, lineWidth);
            } else {
                int lineWidth = 1;

                // top bar
                drawObject(p, HorizontalLine, center-(side-1), r.y(), 2*side, lineWidth);
                // top bar rounding
                drawObject(p, CrossDiagonalLine, center-side-1, r.y()+1, 2, lineWidth);
                // right bar
                drawObject(p, VerticalLine, center+side+1, r.y(), r.height()-(side+2+1), lineWidth);
                // bottom bar
                drawObject(p, CrossDiagonalLine, center, r.bottom()-2, side+2, lineWidth);
                // the dot
                drawObject(p, HorizontalLine, center, r.bottom(), 1, 1);
            }

            break;
        }

        case NotOnAllDesktopsIcon:
        {
            int lwMark = r.width()-lwTitleBar*2-2;
            if (lwMark < 1)
                lwMark = 3;

            drawObject(p, HorizontalLine, r.x()+(r.width()-lwMark)/2, r.y()+(r.height()-lwMark)/2, lwMark, lwMark);

            // Fall through to OnAllDesktopsIcon intended!
        }
        case OnAllDesktopsIcon:
        {
            // horizontal bars
            drawObject(p, HorizontalLine, r.x()+lwTitleBar, r.y(), r.width()-2*lwTitleBar, lwTitleBar);
            drawObject(p, HorizontalLine, r.x()+lwTitleBar, r.bottom()-(lwTitleBar-1), r.width()-2*lwTitleBar, lwTitleBar);
            // vertical bars
            drawObject(p, VerticalLine, r.x(), r.y()+lwTitleBar, r.height()-2*lwTitleBar, lwTitleBar);
            drawObject(p, VerticalLine, r.right()-(lwTitleBar-1), r.y()+lwTitleBar, r.height()-2*lwTitleBar, lwTitleBar);


            break;
        }

        case NoKeepAboveIcon:
        {
            int center = r.x()+r.width()/2;

            // arrow
            drawObject(p, CrossDiagonalLine, r.x(), center+2*lwArrow, center-r.x(), lwArrow);
            drawObject(p, DiagonalLine, r.x()+center, r.y()+1+2*lwArrow, center-r.x(), lwArrow);
            if (lwArrow>1)
                drawObject(p, HorizontalLine, center-(lwArrow-2), r.y()+2*lwArrow, (lwArrow-2)*2, lwArrow);

            // Fall through to KeepAboveIcon intended!
        }
        case KeepAboveIcon:
        {
            int center = r.x()+r.width()/2;

            // arrow
            drawObject(p, CrossDiagonalLine, r.x(), center, center-r.x(), lwArrow);
            drawObject(p, DiagonalLine, r.x()+center, r.y()+1, center-r.x(), lwArrow);
            if (lwArrow>1)
                drawObject(p, HorizontalLine, center-(lwArrow-2), r.y(), (lwArrow-2)*2, lwArrow);

            break;
        }

        case NoKeepBelowIcon:
        {
            int center = r.x()+r.width()/2;

            // arrow
            drawObject(p, DiagonalLine, r.x(), center-2*lwArrow, center-r.x(), lwArrow);
            drawObject(p, CrossDiagonalLine, r.x()+center, r.bottom()-1-2*lwArrow, center-r.x(), lwArrow);
            if (lwArrow>1)
                drawObject(p, HorizontalLine, center-(lwArrow-2), r.bottom()-(lwArrow-1)-2*lwArrow, (lwArrow-2)*2, lwArrow);

            // Fall through to KeepBelowIcon intended!
        }
        case KeepBelowIcon:
        {
            int center = r.x()+r.width()/2;

            // arrow
            drawObject(p, DiagonalLine, r.x(), center, center-r.x(), lwArrow);
            drawObject(p, CrossDiagonalLine, r.x()+center, r.bottom()-1, center-r.x(), lwArrow);
            if (lwArrow>1)
                drawObject(p, HorizontalLine, center-(lwArrow-2), r.bottom()-(lwArrow-1), (lwArrow-2)*2, lwArrow);

            break;
        }

        case ShadeIcon:
        {
            drawObject(p, HorizontalLine, r.x(), r.y(), r.width(), lwTitleBar);

            break;
        }

        case UnShadeIcon:
        {
            int lw1 = 1;
            int lw2 = 1;
            if (r.width() > 16) {
                lw1 = 4;
                lw2 = 2;
            } else if (r.width() > 7) {
                lw1 = 2;
                lw2 = 1;
            }

            int h = qMax( (r.width()/2), (lw1+2*lw2) );

            // horizontal bars
            drawObject(p, HorizontalLine, r.x(), r.y(), r.width(), lw1);
            drawObject(p, HorizontalLine, r.x(), r.x()+h-(lw2-1), r.width(), lw2);
            // vertical bars
            drawObject(p, VerticalLine, r.x(), r.y(), h, lw2);
            drawObject(p, VerticalLine, r.right()-(lw2-1), r.y(), h, lw2);

            break;
        }

        default:
            break;
    }

    p.end();

    bitmap.setMask(bitmap);

    return bitmap;
}

void IconEngine::drawObject(QPainter &p, Object object, int x, int y, int length, int lineWidth)
{
    switch(object) {
        case DiagonalLine:
            if (lineWidth <= 1) {
                for (int i = 0; i < length; ++i) {
                    p.drawPoint(x+i,y+i);
                }
            } else if (lineWidth <= 2) {
                for (int i = 0; i < length; ++i) {
                    p.drawPoint(x+i,y+i);
                }
                for (int i = 0; i < (length-1); ++i) {
                    p.drawPoint(x+1+i,y+i);
                    p.drawPoint(x+i,y+1+i);
                }
            } else {
                for (int i = 1; i < (length-1); ++i) {
                    p.drawPoint(x+i,y+i);
                }
                for (int i = 0; i < (length-1); ++i) {
                    p.drawPoint(x+1+i,y+i);
                    p.drawPoint(x+i,y+1+i);
                }
                for (int i = 0; i < (length-2); ++i) {
                    p.drawPoint(x+2+i,y+i);
                    p.drawPoint(x+i,y+2+i);
                }
            }
            break;
        case CrossDiagonalLine:
            if (lineWidth <= 1) {
                for (int i = 0; i < length; ++i) {
                    p.drawPoint(x+i,y-i);
                }
            } else if (lineWidth <= 2) {
                for (int i = 0; i < length; ++i) {
                    p.drawPoint(x+i,y-i);
                }
                for (int i = 0; i < (length-1); ++i) {
                    p.drawPoint(x+1+i,y-i);
                    p.drawPoint(x+i,y-1-i);
                }
            } else {
                for (int i = 1; i < (length-1); ++i) {
                    p.drawPoint(x+i,y-i);
                }
                for (int i = 0; i < (length-1); ++i) {
                    p.drawPoint(x+1+i,y-i);
                    p.drawPoint(x+i,y-1-i);
                }
                for (int i = 0; i < (length-2); ++i) {
                    p.drawPoint(x+2+i,y-i);
                    p.drawPoint(x+i,y-2-i);
                }
            }
            break;
        case HorizontalLine:
            for (int i = 0; i < lineWidth; ++i) {
                p.drawLine(x,y+i, x+length-1, y+i);
            }
            break;
        case VerticalLine:
            for (int i = 0; i < lineWidth; ++i) {
                p.drawLine(x+i,y, x+i, y+length-1);
            }
            break;
        default:
            break;
    }
}

} // KWinPlastik
