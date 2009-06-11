//////////////////////////////////////////////////////////////////////////////
// oxygenclient.cpp
// -------------------
// Oxygen window decoration for KDE
// -------------------
// Copyright (c) 2006, 2007 Casper Boemann <cbr@boemann.dk>
// Copyright (c) 2006, 2007 Riccardo Iaconelli <riccardo@kde.org>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//////////////////////////////////////////////////////////////////////////////
// #ifndef OXYGENCLIENT_H
// #define OXYGENCLIENT_H

#include "oxygenclient.h"

#include <KConfig>
#include <KGlobal>
#include <KLocale>
#include <KDebug>
#include <KColorUtils>

#include <QBitmap>
#include <QLabel>
#include <QLayout>
#include <QPainter>
#include <QToolTip>
//Added by qt3to4:
#include <QBoxLayout>
#include <QGridLayout>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QEvent>
#include <QShowEvent>
#include <QPaintEvent>
#include <QPainterPath>
#include <QTimer>
#include <QCache>
#include <QtGui/QApplication>

#include "math.h"

#include "oxygenclient.moc"
#include "oxygenbutton.h"
#include "oxygen.h"

namespace Oxygen
{

K_GLOBAL_STATIC_WITH_ARGS(OxygenHelper, globalHelper, ("OxygenDeco"))

OxygenHelper *oxygenHelper()
{
    return globalHelper;
}

static void oxkwincleanupBefore()
{
    OxygenHelper *h = globalHelper;
    h->invalidateCaches();
}

void OxygenClient::invalidateCaches()
{
}

void renderDot(QPainter *p, const QPointF &point, qreal diameter)
{
    p->drawEllipse(QRectF(point.x()-diameter/2, point.y()-diameter/2, diameter, diameter));
}


OxygenClient::OxygenClient(KDecorationBridge *b, KDecorationFactory *f)
    : KCommonDecorationUnstable(b, f)
    , colorCacheInvalid_(true)
    , helper_(*globalHelper)
{
    qAddPostRoutine(oxkwincleanupBefore);
}

OxygenClient::~OxygenClient()
{
}
QString OxygenClient::visibleName() const
{
    return i18n("Oxygen");
}

void OxygenClient::init()
{
    KCommonDecoration::init();

    widget()->setAutoFillBackground(false);
    widget()->setAttribute(Qt::WA_OpaquePaintEvent);
}

bool OxygenClient::decorationBehaviour(DecorationBehaviour behaviour) const
{
    switch (behaviour) {
        case DB_MenuClose:
            return true;//Handler()->menuClose();

        case DB_WindowMask:
            return false;

        default:
            return KCommonDecoration::decorationBehaviour(behaviour);
    }
}

int OxygenClient::layoutMetric(LayoutMetric lm, bool respectWindowState, const KCommonDecorationButton *btn) const
{
    bool maximized = maximizeMode()==MaximizeFull && !options()->moveResizeMaximizedWindows();
    int frameWidth = OxygenFactory::borderSize();

    switch (lm) {
        case LM_BorderLeft:
        case LM_BorderRight:
        case LM_BorderBottom:
        {
            if (respectWindowState && maximized) {
                return 0;
            } else {
                // Even for thin borders (2px wide) we want to preserve
                // the rounded corners having a minimum height of 7px
                if (lm == LM_BorderBottom) {
                    return qMax(frameWidth, 7);
                } else {
                    return frameWidth;
                }
            }
        }

        case LM_TitleEdgeTop:
        {
            if (respectWindowState && maximized) {
                return 0;
            } else {
                return TFRAMESIZE;
            }
        }

        case LM_TitleEdgeBottom:
        {
            return 0;
        }

        case LM_TitleEdgeLeft:
        case LM_TitleEdgeRight:
        {
            if (respectWindowState && maximized) {
                return 0;
            } else {
                return 6;
            }
        }

        case LM_TitleBorderLeft:
        case LM_TitleBorderRight:
            return 5;

        case LM_ButtonWidth:
        case LM_ButtonHeight:
        case LM_TitleHeight:
        {
            if (respectWindowState && isToolWindow()) {
                return OXYGEN_BUTTONSIZE;
            } else {
                return OXYGEN_BUTTONSIZE;
            }
        }

        case LM_ButtonSpacing:
            return 1;

        case LM_ButtonMarginTop:
            return 0;

        // outer margin for shadow/glow
        case LM_OuterPaddingLeft:
        case LM_OuterPaddingRight:
        case LM_OuterPaddingTop:
        case LM_OuterPaddingBottom:
            return SHADOW_WIDTH;

        default:
            return KCommonDecoration::layoutMetric(lm, respectWindowState, btn);
    }
}


KCommonDecorationButton *OxygenClient::createButton(::ButtonType type)
{
    switch (type) {
        case MenuButton:
            return new OxygenButton(*this, i18n("Menu"), ButtonMenu);

        case HelpButton:
            return new OxygenButton(*this, i18n("Help"), ButtonHelp);

        case MinButton:
            return new OxygenButton(*this, i18n("Minimize"), ButtonMin);

        case MaxButton:
            return new OxygenButton(*this, i18n("Maximize"), ButtonMax);

        case CloseButton:
            return new OxygenButton(*this, i18n("Close"), ButtonClose);

        case AboveButton:
            return new OxygenButton(*this, i18n("Keep Above Others"), ButtonAbove);

        case BelowButton:
            return new OxygenButton(*this, i18n("Keep Below Others"), ButtonBelow);

        case OnAllDesktopsButton:
            return new OxygenButton(*this, i18n("On All Desktops"), ButtonSticky);

        case ShadeButton:
            return new OxygenButton(*this, i18n("Shade Button"), ButtonShade);

        default:
            return 0;
    }
}


// c0 - background
// c1 - foreground
// t - target contrast ratio
QColor reduceContrast(const QColor &c0, const QColor &c1, double t)
{
    double s = KColorUtils::contrastRatio(c0, c1);
    if (s < t)
        return c1;

    double l = 0.0, h = 1.0;
    double x = s, a;
    QColor r = c1;
    for (int maxiter = 16; maxiter; --maxiter) {
        a = 0.5 * (l + h);
        r = KColorUtils::mix(c0, c1, a);
        x = KColorUtils::contrastRatio(c0, r);
        if (fabs(x - t) < 0.01)
            break;
        if (x > t)
            h = a;
        else
            l = a;
    }
    return r;
}


QColor OxygenClient::titlebarTextColor(const QPalette &palette)
{
    if (isActive())
        return palette.color(QPalette::Active, QPalette::WindowText);
    else {
        if(colorCacheInvalid_) {
            QColor ab = palette.color(QPalette::Active, QPalette::Window);
            QColor af = palette.color(QPalette::Active, QPalette::WindowText);
            QColor nb = palette.color(QPalette::Inactive, QPalette::Window);
            QColor nf = palette.color(QPalette::Inactive, QPalette::WindowText);

            colorCacheInvalid_ = false;
            cachedTitlebarTextColor_ = reduceContrast(nb, nf, qMax(qreal(2.5), KColorUtils::contrastRatio(ab, KColorUtils::mix(ab, af, 0.4))));
        }
        return cachedTitlebarTextColor_;
    }
}

void OxygenClient::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e)
    if (!OxygenFactory::initialized()) return;

    QPalette palette = widget()->palette();
    QPainter painter(widget());

    // Set palette to the right group.
    // TODO - fix KWin to do this for us :-).
    if (isActive())
        palette.setCurrentColorGroup(QPalette::Active);
    else
        palette.setCurrentColorGroup(QPalette::Inactive);

    int x,y,w,h;
    QRect frame = widget()->rect();
    frame.adjust(SHADOW_WIDTH, SHADOW_WIDTH, -SHADOW_WIDTH, -SHADOW_WIDTH);

    QColor color = palette.window().color();
    QColor light = helper_.calcLightColor( color );
    QColor dark = helper_.calcDarkColor( color );

    const int titleHeight = layoutMetric(LM_TitleHeight);
    const int titleTop = layoutMetric(LM_TitleEdgeTop) + frame.top();
    const int titleEdgeLeft = layoutMetric(LM_TitleEdgeLeft);
    const int marginLeft = layoutMetric(LM_TitleBorderLeft);
    const int marginRight = layoutMetric(LM_TitleBorderRight);

    const int titleLeft = frame.left() + titleEdgeLeft + buttonsLeftWidth() + marginLeft;
    const int titleWidth = frame.width() -
            titleEdgeLeft - layoutMetric(LM_TitleEdgeRight) -
            buttonsLeftWidth() - buttonsRightWidth() -
            marginLeft - marginRight;

    // draw shadow

    if (compositingActive())
        shadowTiles(color,KDecoration::options()->color(ColorTitleBar),
                SHADOW_WIDTH, isActive())->render( frame.adjusted(-SHADOW_WIDTH+4,
                    -SHADOW_WIDTH+4, SHADOW_WIDTH-4, SHADOW_WIDTH-4),
                &painter, TileSet::Ring);

    // draw window background
    bool isCompositingActive = compositingActive();

    if (isCompositingActive) {
        frame.getRect(&x, &y, &w, &h);

        QRegion mask(x+5, y+0, w-10, h-0);
        mask += QRegion(x+0, y+5, w-0, h-10);
        mask += QRegion(x+1, y+3, w-2, h-6);
        mask += QRegion(x+2, y+2, w-4, h-4);
        mask += QRegion(x+3, y+1, w-6, h-2);

        painter.setClipRegion(mask);
    } 

    helper_.renderWindowBackground(&painter, frame, this->widget(), palette, SHADOW_WIDTH);

    if (isCompositingActive) {
        painter.setClipping(false);
    }

    // draw title text
    painter.setFont(options()->font(isActive(), false));
    painter.setPen(titlebarTextColor(palette));
    painter.drawText(titleLeft, titleTop-1, titleWidth, titleHeight,  // -1 is to go into top resizearea
              OxygenFactory::titleAlignment() | Qt::AlignVCenter, caption());

    painter.setRenderHint(QPainter::Antialiasing);

    // Draw dividing line
    if (compositingActive()) {
        frame.adjust(-1,-1,1,1);
    }
    frame.getRect(&x, &y, &w, &h);

    if(isActive()) {
        helper_.drawSeparator(&painter, QRect(x, titleTop+titleHeight-1.5, w, 2), color, Qt::Horizontal);
    }

    // draw stripes as indicator for active windows
    if (isActive() && OxygenFactory::showStripes()) {
        Qt::Alignment align = OxygenFactory::titleAlignment();
        if (widget()->layoutDirection() == Qt::RightToLeft)
        {
            if (align == Qt::AlignLeft)
                align = Qt::AlignRight;
            else if (align == Qt::AlignRight)
                align = Qt::AlignLeft;
        }

        if (align & Qt::AlignLeft) {
            int left = titleLeft + QFontMetrics(options()->font(isActive(), false)).width(caption()) + 4;
            int right = titleLeft + titleWidth;
            if (left < right)
                drawStripes(&painter, palette, left, right, titleTop+6);
        }
        if (align & Qt::AlignRight) {
            int left = titleLeft;
            int right = titleLeft + titleWidth - QFontMetrics(options()->font(isActive(), false)).width(caption()) - 4;
            if (left < right)
                drawStripes(&painter, palette, right, left, titleTop+6);
        }
        if (align & Qt::AlignHCenter) {
            int textWidth = QFontMetrics(options()->font(isActive(), false)).width(caption());
            int left = titleLeft;
            int centerLeft = titleLeft + titleWidth/2 - textWidth/2 - 4;
            int centerRight = titleLeft + titleWidth/2 + textWidth/2 + 4;
            int right = titleLeft + titleWidth;
            if (left < centerLeft && right > centerRight) {
                drawStripes(&painter, palette, centerLeft, left, titleTop+6);
                drawStripes(&painter, palette, centerRight, right, titleTop+6);
            }
        }
    }

    // Draw shadows of the frame
    bool maximized = maximizeMode()==MaximizeFull && !options()->moveResizeMaximizedWindows();

    if(maximized)
        return;

    helper_.drawFloatFrame(&painter, frame, color, !compositingActive(), isActive(),
                                                                KDecoration::options()->color(ColorTitleBar));

    if(!isResizable())
        return;

    // Draw the 3-dots resize handles
    qreal cenY = frame.height() / 2 + x + 0.5;
    qreal posX = frame.width() + y - 2.5;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 66));
    renderDot(&painter, QPointF(posX, cenY - 3), 1.8);
    renderDot(&painter, QPointF(posX, cenY), 1.8);
    renderDot(&painter, QPointF(posX, cenY + 3), 1.8);

    painter.translate(x + frame.width()-9, y + frame.height()-9);
    renderDot(&painter, QPointF(2.5, 6.5), 1.8);
    renderDot(&painter, QPointF(5.5, 5.5), 1.8);
    renderDot(&painter, QPointF(6.5, 2.5), 1.8);
}

void OxygenClient::drawStripes(QPainter *p, QPalette &palette, const int start, const int end, const int topMargin)
{
    QLinearGradient stripeGradient(QPoint(start,0), QPoint(end,0));
    stripeGradient.setColorAt(0.0, Qt::transparent);
    stripeGradient.setColorAt(0.05, KDecoration::options()->color(ColorTitleBar));
    stripeGradient.setColorAt(1.0, Qt::transparent);
    QPen pen1(stripeGradient, 0.5);

    QLinearGradient stripeGradient2(QPoint(start,0), QPoint(end,0));
    stripeGradient2.setColorAt(0.0, Qt::transparent);
    stripeGradient2.setColorAt(0.05, helper_.calcLightColor(palette.color(QPalette::Window)));
    stripeGradient2.setColorAt(1.0, Qt::transparent);
    QPen pen2(stripeGradient2, 0.5);

    bool antialiasing = p->testRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::Antialiasing, false);
    for (int i = 0; i < 3; ++i)
    {
        p->setPen(pen1);
        p->drawLine(QPointF(start, topMargin+4*i), QPointF(end, topMargin+4*i));
        p->setPen(pen2);
        p->drawLine(QPointF(start, topMargin+4*i+1), QPointF(end, topMargin+4*i+1));
    }
    p->setRenderHint(QPainter::Antialiasing, antialiasing);
}


void OxygenClient::updateWindowShape()
{
    bool maximized = maximizeMode()==MaximizeFull && !options()->moveResizeMaximizedWindows();
    int w=widget()->width() - (int)SHADOW_WIDTH;
    int h=widget()->height() - (int)SHADOW_WIDTH;

    if(maximized) {
        QRegion mask(0,0,w,h);
        setMask(mask);
        return;
    }

    if (!compositingActive()) {
        int sw = SHADOW_WIDTH;
        QRegion mask(sw+4, sw+0, -sw+w-8, -sw+h);
        mask += QRegion(sw+0, sw+4, -sw+w, -sw+h-8);
        mask += QRegion(sw+2, sw+1, -sw+w-4, -sw+h-2);
        mask += QRegion(sw+1, sw+2, -sw+w-2, -sw+h-4);

        setMask(mask);
    }
}

TileSet *OxygenClient::shadowTiles(const QColor& color, const QColor& glow, qreal size, bool active)
{
    ShadowTilesOption opt;
    opt.active      = active;
    opt.width       = size;
    opt.windowColor = color;
    opt.glowColor   = glow;

    ShadowTilesOption currentOpt = active ? shadowTilesOption_ : glowTilesOption_;

    bool optionChanged = true;
    if (currentOpt.active == opt.active
            && currentOpt.width == opt.width
            && opt.windowColor == opt.windowColor
            && opt.glowColor == opt.glowColor)
        optionChanged = false;

    if (active && glowTiles_ && !optionChanged)
        return glowTiles_;
    else if (!active && shadowTiles_ && !optionChanged)
        return shadowTiles_;
        
    TileSet *tileSet;

    if (!tileSet)
    {
        QColor light = oxygenHelper()->calcLightColor(oxygenHelper()->backgroundTopColor(color));
        QColor dark = oxygenHelper()->calcDarkColor(oxygenHelper()->backgroundBottomColor(color));

        QPixmap shadow = QPixmap( size*2, size*2 );
        shadow.fill( Qt::transparent );

        // draw the corner of the window - actually all 4 corners as one circle
        QLinearGradient lg = QLinearGradient(0.0, size-4.5, 0.0, size+4.5);
        lg.setColorAt(0.52, light);
        lg.setColorAt(1.0, dark);

        QPainter p( &shadow );
        p.setRenderHint( QPainter::Antialiasing );
        p.setPen( Qt::NoPen );

        if (active)
        {
            //---------------------------------------------------------------
            // Active shadow texture

            QRadialGradient rg( size, size, size );
            QColor c = color;
            c.setAlpha( 255 );  rg.setColorAt( 4.4/size, c );
            c.setAlpha( 220 );  rg.setColorAt( 4.5/size, c );
            c.setAlpha( 180 );  rg.setColorAt( 5/size, c );
            c.setAlpha( 25 );  rg.setColorAt( 5.5/size, c );
            c.setAlpha( 0 );  rg.setColorAt( 6.5/size, c );

            p.setBrush( rg );
            p.drawRect( shadow.rect() );

            rg = QRadialGradient( size, size, size );
            c = color;
            c.setAlpha( 255 );  rg.setColorAt( 4.4/size, c );
            c = glow;
            c.setAlpha( 0.58*255 );  rg.setColorAt( 4.5/size, c );
            c.setAlpha( 0.43*255 );  rg.setColorAt( 5.5/size, c );
            c.setAlpha( 0.30*255 );  rg.setColorAt( 6.5/size, c );
            c.setAlpha( 0.22*255 );  rg.setColorAt( 7.5/size, c );
            c.setAlpha( 0.15*255 );  rg.setColorAt( 8.5/size, c );
            c.setAlpha( 0.08*255 );  rg.setColorAt( 11.5/size, c );
            c.setAlpha( 0);  rg.setColorAt( 14.5/size, c );
            p.setRenderHint( QPainter::Antialiasing );
            p.setBrush( rg );
            p.drawRect( shadow.rect() );
            
            p.setBrush( Qt::NoBrush );
            p.setPen(QPen(lg, 0.8));
            p.drawEllipse(QRectF(size-4, size-4, 8, 8));

            p.end();

            tileSet = new TileSet(shadow, size, size, 1, 1);
            glowTilesOption_ = opt;
            glowTiles_       = tileSet;
        } else {
            //---------------------------------------------------------------
            // Inactive shadow texture

            QRadialGradient rg = QRadialGradient( size, size+4, size );
            QColor c = QColor( Qt::black );
            c.setAlpha( 0.12*255 );  rg.setColorAt( 4.5/size, c );
            c.setAlpha( 0.11*255 );  rg.setColorAt( 6.6/size, c );
            c.setAlpha( 0.075*255 );  rg.setColorAt( 8.5/size, c );
            c.setAlpha( 0.06*255 );  rg.setColorAt( 11.5/size, c );
            c.setAlpha( 0.035*255 );  rg.setColorAt( 14.5/size, c );
            c.setAlpha( 0.025*255 );  rg.setColorAt( 17.5/size, c );
            c.setAlpha( 0.01*255 );  rg.setColorAt( 21.5/size, c );
            c.setAlpha( 0.0*255 );  rg.setColorAt( 25.5/size, c );
            p.setRenderHint( QPainter::Antialiasing );
            p.setPen( Qt::NoPen );
            p.setBrush( rg );
            p.drawRect( shadow.rect() );

            rg = QRadialGradient( size, size+2, size );
            c = QColor( Qt::black );
            c.setAlpha( 0.25*255 );  rg.setColorAt( 4.5/size, c );
            c.setAlpha( 0.20*255 );  rg.setColorAt( 5.5/size, c );
            c.setAlpha( 0.13*255 );  rg.setColorAt( 7.5/size, c );
            c.setAlpha( 0.06*255 );  rg.setColorAt( 8.5/size, c );
            c.setAlpha( 0.015*255 );  rg.setColorAt( 11.5/size, c );
            c.setAlpha( 0.0*255 );  rg.setColorAt( 14.5/size, c );
            p.setRenderHint( QPainter::Antialiasing );
            p.setPen( Qt::NoPen );
            p.setBrush( rg );
            p.drawRect( shadow.rect() );

            rg = QRadialGradient( size, size+0.2, size );
            c = color;
            c = QColor( Qt::black );
            c.setAlpha( 0.35*255 );  rg.setColorAt( 0/size, c );
            c.setAlpha( 0.32*255 );  rg.setColorAt( 4.5/size, c );
            c.setAlpha( 0.22*255 );  rg.setColorAt( 5.0/size, c );
            c.setAlpha( 0.03*255 );  rg.setColorAt( 5.5/size, c );
            c.setAlpha( 0.0*255 );  rg.setColorAt( 6.5/size, c );
            p.setRenderHint( QPainter::Antialiasing );
            p.setPen( Qt::NoPen );
            p.setBrush( rg );
            p.drawRect( shadow.rect() );

            rg = QRadialGradient( size, size, size );
            c = color;
            c.setAlpha( 255 );  rg.setColorAt( 4.0/size, c );
            c.setAlpha( 0 );  rg.setColorAt( 4.01/size, c );
            p.setRenderHint( QPainter::Antialiasing );
            p.setPen( Qt::NoPen );
            p.setBrush( rg );
            p.drawRect( shadow.rect() );

            // draw the corner of the window - actually all 4 corners as one circle
            p.setBrush( Qt::NoBrush );
            p.setPen(QPen(lg, 0.8));
            p.drawEllipse(QRectF(size-4, size-4, 8, 8));

            p.end();

            tileSet = new TileSet(shadow, size, size, 1, 1);
            shadowTilesOption_ = opt;
            shadowTiles_       = tileSet;
        }
    }
    return tileSet;
}

} //namespace Oxygen

//#include "oxygenclient.moc"

// #endif
