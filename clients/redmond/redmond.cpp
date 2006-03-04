/*
 *
 * Redmond KWin client
 *
 * Copyright 2001
 *   Karol Szwed <gallium@kde.org>
 *   http://gallium.n3.net/
 *
 * Based on the default KWin client.
 *
 * Updated to support toolwindows 3/2001 (KS)
 *
 */

#include "redmond.h"

#include <qdrawutil.h>
#include <qdatetime.h>
//Added by qt3to4:
#include <QPixmap>
#include <QPaintEvent>
#include <kpixmapeffect.h>
#include <kimageeffect.h>
#include <kdrawutil.h>
#include <klocale.h>

#include <qbitmap.h>
#include <qimage.h>
#include <qapplication.h>

namespace Redmond {

static const char *kdelogo[] = {
/* columns rows colors chars-per-pixel */
"16 16 8 1",
" 	c None",
".	c #000000",
"+	c #A0A0A4",
"@	c #FFFFFF",
"#	c #585858",
"$	c #C0C0C0",
"%	c #808080",
"&	c #DCDCDC",
"                ",
"     ..    ..   ",
"    .+@.  .@#.  ",
"   .@@@. .@@@#  ",
"   .@@@..$@@$.  ",
"   .@@@.@@@$.   ",
"   .@@@%@@$.    ",
"   .@@@&@@.     ",
"   .@@@@@@.     ",
"   .@@@$@@&.    ",
"   .@@@.@@@.    ",
"   .@@@.+@@@.   ",
"   .@@@..$@@&.  ",
"   .@@%. .@@@.  ",
"   ....   ...   ",
"                "};

static unsigned char iconify_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7e, 0x00, 0x7e, 0x00, 0x00, 0x00};

static unsigned char close_bits[] = {
  0x00, 0x00, 0x86, 0x01, 0xcc, 0x00, 0x78, 0x00, 0x30, 0x00, 0x78, 0x00,
  0xcc, 0x00, 0x86, 0x01, 0x00, 0x00, 0x00, 0x00};

static unsigned char maximize_bits[] = {
  0xff, 0x01, 0xff, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0xff, 0x01, 0x00, 0x00};

static unsigned char minmax_bits[] = {
  0xfc, 0x00, 0xfc, 0x00, 0x84, 0x00, 0xbf, 0x00, 0xbf, 0x00, 0xe1, 0x00,
  0x21, 0x00, 0x21, 0x00, 0x3f, 0x00, 0x00, 0x00};

static unsigned char question_bits[] = {
  0x00, 0x00, 0x3c, 0x00, 0x66, 0x00, 0x60, 0x00, 0x30, 0x00, 0x18, 0x00,
  0x00, 0x00, 0x18, 0x00, 0x18, 0x00, 0x00, 0x00};


// Up / Down titlebar button images
static KPixmap *btnPix1;
static KPixmap *iBtnPix1;
static KPixmap *btnDownPix1;
static KPixmap *iBtnDownPix1;

static KPixmap *miniBtnPix1;
static KPixmap *iMiniBtnPix1;
static KPixmap *miniBtnDownPix1;
static KPixmap *iMiniBtnDownPix1;

static QPixmap *defaultMenuPix;
static QColor  *btnForeground;
static bool    pixmaps_created = false;

static int toolTitleHeight;
static int normalTitleHeight;
static int borderWidth;

static inline const KDecorationOptions *options()
{
   return KDecoration::options();
}

static void drawButtonFrame( KPixmap *pix, const QColorGroup &g, bool sunken )
{
    QPainter p;
    int x2 = pix->width() - 1;
    int y2 = pix->height() - 1;
    p.begin(pix);

    // titlebar button frame
    p.setPen( sunken ? g.dark().dark(155) : g.light());
    p.drawLine(0, 0, x2-1, 0);
    p.drawLine(0, 0, 0, y2-1);

    if (sunken)
    {
       p.setPen( g.mid().dark(135) );
       p.drawLine(1, 1, x2-2, 1);
       p.drawLine(1, 1, 1, y2-2);
    }

    p.setPen( sunken ? g.light() : g.mid().dark(135));
    p.drawLine(1, y2-1, x2-1, y2-1);
    p.drawLine(x2-1, 1, x2-1, y2-1);

    p.setPen( sunken ? g.light() : g.dark().dark(155));
    p.drawLine(0, y2, x2, y2);
    p.drawLine(x2, 0, x2, y2);
}


static void create_pixmaps ()
{
    if (pixmaps_created)
        return;

    pixmaps_created = true;

    bool highcolor = QPixmap::defaultDepth() > 8;

    btnPix1 = new KPixmap;
    btnDownPix1 = new KPixmap;
    iBtnPix1 = new KPixmap;
    iBtnDownPix1 = new KPixmap;
    miniBtnPix1 = new KPixmap;
    miniBtnDownPix1 = new KPixmap;
    iMiniBtnPix1 = new KPixmap;
    iMiniBtnDownPix1 = new KPixmap;
    defaultMenuPix = new QPixmap(kdelogo);

    // buttons (active/inactive, sunken/unsunken)
	QColorGroup g = options()->palette(KDecoration::ColorButtonBg, true).active();
    QColor c = g.background();
    btnPix1->resize(normalTitleHeight, normalTitleHeight-2);
    btnDownPix1->resize(normalTitleHeight, normalTitleHeight-2);
    iBtnPix1->resize(normalTitleHeight, normalTitleHeight-2);
    iBtnDownPix1->resize(normalTitleHeight, normalTitleHeight-2);

    miniBtnPix1->resize(toolTitleHeight, toolTitleHeight);
    miniBtnDownPix1->resize(toolTitleHeight, toolTitleHeight);
    iMiniBtnPix1->resize(toolTitleHeight, toolTitleHeight);
    iMiniBtnDownPix1->resize(toolTitleHeight, toolTitleHeight);

    if (highcolor && false) {
        KPixmapEffect::gradient(*btnPix1, c.light(130), c.dark(130),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(*btnDownPix1, c.dark(130), c.light(130),
                                KPixmapEffect::VerticalGradient);

        KPixmapEffect::gradient(*miniBtnPix1, c.light(130), c.dark(130),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(*miniBtnDownPix1, c.dark(130), c.light(130),
                                KPixmapEffect::VerticalGradient);

		g = options()->palette(KDecoration::ColorButtonBg, false).active();
        c = g.background();
        KPixmapEffect::gradient(*iBtnPix1, c.light(130), c.dark(130),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(*iBtnDownPix1, c.dark(130), c.light(130),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(*iMiniBtnPix1, c.light(130), c.dark(130),
                                KPixmapEffect::VerticalGradient);
        KPixmapEffect::gradient(*iMiniBtnDownPix1, c.dark(130), c.light(130),
                                KPixmapEffect::VerticalGradient);
    } else {
        btnPix1->fill(c.rgb());
        btnDownPix1->fill(c.rgb());
        miniBtnPix1->fill(c.rgb());
        miniBtnDownPix1->fill(c.rgb());

		g = options()->palette(KDecoration::ColorButtonBg, false).active();
        c = g.background();
        iBtnPix1->fill(c.rgb());
        iBtnDownPix1->fill(c.rgb());
        iMiniBtnPix1->fill(c.rgb());
        iMiniBtnDownPix1->fill(c.rgb());
    }

	g = options()->palette(KDecoration::ColorButtonBg, true).active();
    drawButtonFrame(btnPix1, g, false);
    drawButtonFrame(btnDownPix1, g, true);
    drawButtonFrame(miniBtnPix1, g, false);
    drawButtonFrame(miniBtnDownPix1, g, true);

	g = options()->palette(KDecoration::ColorButtonBg, false).active();
    drawButtonFrame(iBtnPix1, g, false);
    drawButtonFrame(iBtnDownPix1, g, true);
    drawButtonFrame(iMiniBtnPix1, g, false);
    drawButtonFrame(iMiniBtnDownPix1, g, true);

    // Make sure button pixmaps contrast with the current colour scheme.
    if (qGray(options()->color(KDecoration::ColorButtonBg, true).rgb()) > 127)
        btnForeground = new QColor(Qt::black);
    else
        btnForeground = new QColor(Qt::white);
}

void delete_pixmaps()
{
    delete btnPix1;
    delete btnDownPix1;
    delete iBtnPix1;
    delete iBtnDownPix1;
    delete miniBtnPix1;
    delete miniBtnDownPix1;
    delete iMiniBtnPix1;
    delete iMiniBtnDownPix1;
    delete defaultMenuPix;
    delete btnForeground;
    pixmaps_created = false;
}

RedmondButton::RedmondButton(ButtonType type, RedmondDeco *parent, const char *name)
    : KCommonDecorationButton(type, parent)
{
	// Eliminate background flicker
	setBackgroundMode( Qt::NoBackground );

	miniBtn = decoration()->isToolWindow();
}

void RedmondButton::reset(unsigned long changed)
{
	if (changed&DecorationReset || changed&ManualReset || changed&SizeChange || changed&StateChange) {
		switch (type() ) {
			case CloseButton:
				setBitmap(close_bits);
				break;
			case HelpButton:
				setBitmap(question_bits);
				break;
			case MinButton:
				setBitmap(iconify_bits);
				break;
			case MaxButton:
				setBitmap( isChecked() ? minmax_bits : maximize_bits );
				break;
			case MenuButton:
			{
				QPixmap miniIcon = decoration()->icon().pixmap(QIcon::Small, QIcon::Normal);
				if (!miniIcon.isNull()) {
					setPixmap(miniIcon);
				} else {
					setPixmap(*defaultMenuPix);
				}
				break;
			}
			default:
				setBitmap(0);
				break;
		}

		this->update();
	}
}


void RedmondButton::setBitmap(const unsigned char *bitmap)
{
	pix.resize(0, 0);

	if (bitmap)
		deco = QBitmap(10, 10, bitmap, true);
	else {
		deco = QBitmap(10,10);
		deco.fill(Qt::color0);
	}
	deco.setMask(deco);
}


void RedmondButton::setPixmap( const QPixmap &p )
{
	deco.resize(0, 0);
	pix = p;

	repaint(false);
}

void RedmondButton::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	drawButton(&p);
}

void RedmondButton::drawButton(QPainter *p)
{
	if ( pix.isNull() ) {
		if ( decoration()->isActive() ) {
			if ( isDown() )
				p->drawPixmap(0, 0, miniBtn ? *miniBtnDownPix1 : *btnDownPix1);
			else
				p->drawPixmap(0, 0, miniBtn ? *miniBtnPix1 : *btnPix1);
		} else {
			if ( isDown() )
				p->drawPixmap(0, 0, miniBtn ? *iMiniBtnDownPix1 : *iBtnDownPix1);
			else
				p->drawPixmap(0, 0, miniBtn ? *iMiniBtnPix1 : *iBtnPix1);
		}

		p->setPen( *btnForeground );
		int xOff = (width()-10)/2;
		int yOff = (height()-10)/2;
		p->drawPixmap(isDown() ? xOff+1: xOff, isDown() ? yOff+1 : yOff, deco);
	} else {
		if (isLeft() ) {
			p->fillRect(0, 0, width(), height(),
						options()->color(KDecoration::ColorTitleBar, decoration()->isActive()));
		} else {
			p->fillRect(0, 0, width(), height(),
						options()->color(KDecoration::ColorTitleBlend, decoration()->isActive()));
		}

		if ( type()==MenuButton && height() < 16) {
			QPixmap tmpPix;

		 // Smooth scale the menu button pixmap
			tmpPix.convertFromImage(
					pix.convertToImage().smoothScale(height(), height() ));

			p->drawPixmap( 0, 0, tmpPix );
		} else {
			int xOff = (width() -pix.width() )/2;
			int yOff = (height()-pix.height())/2;
			p->drawPixmap(xOff, yOff, pix );
		}
	}
}


RedmondDeco::RedmondDeco(KDecorationBridge *b, KDecorationFactory *f)
    : KCommonDecoration(b, f)
{
}

QString RedmondDeco::visibleName() const
{
    return i18n("Redmond");
}

QString RedmondDeco::defaultButtonsLeft() const
{
    return "M";
}

QString RedmondDeco::defaultButtonsRight() const
{
    return "HIA_X";
}

bool RedmondDeco::decorationBehaviour(DecorationBehaviour behaviour) const
{
    switch (behaviour) {
        case DB_MenuClose:
            return true;

        case DB_WindowMask:
            return false;

        case DB_ButtonHide:
            return true;

        default:
            return KCommonDecoration::decorationBehaviour(behaviour);
    }
}

int RedmondDeco::layoutMetric(LayoutMetric lm, bool respectWindowState, const KCommonDecorationButton *btn) const
{
	bool border = !(maximizeMode()==MaximizeFull && !options()->moveResizeMaximizedWindows());

	switch (lm) {
		case LM_BorderLeft:
		case LM_BorderRight:
		case LM_BorderBottom:
			return border ? borderWidth : 0;

		case LM_TitleEdgeLeft:
		case LM_TitleEdgeRight:
			return border ? borderWidth+2 : 2;

		case LM_TitleEdgeTop:
			return border ? borderWidth+2 : 2;

		case LM_TitleEdgeBottom:
			return border ? 1 : 0;

		case LM_TitleBorderLeft:
		case LM_TitleBorderRight:
			return border ? 1 : 0;

		case LM_TitleHeight:
			return titleHeight-2;

		case LM_ButtonWidth:
			return titleHeight-2;
		case LM_ButtonHeight:
			if (isToolWindow() || (btn && btn->type()==MenuButton) ) {
				return titleHeight-2;
			} else {
				return titleHeight-2-2;
			}

		case LM_ButtonSpacing:
			return 0;

		case LM_ExplicitButtonSpacer:
			return 2;

		default:
			return KCommonDecoration::layoutMetric(lm, respectWindowState, btn);
	}
}

KCommonDecorationButton *RedmondDeco::createButton(ButtonType type)
{
    switch (type) {
        case MenuButton:
            return new RedmondButton(MenuButton, this, "menu");
        case HelpButton:
            return new RedmondButton(HelpButton, this, "help");
        case MinButton:
            return new RedmondButton(MinButton, this, "minimize");
        case MaxButton:
            return new RedmondButton(MaxButton, this, "maximize");
        case CloseButton:
            return new RedmondButton(CloseButton, this, "close");

        default:
            return 0;
    }
}

void RedmondDeco::init()
{
//	Finally, toolwindows look small
	if ( isToolWindow() ) {
		titleHeight = toolTitleHeight+2;
	} else {
		titleHeight = normalTitleHeight+2;
	}

    KCommonDecoration::init();
}

void RedmondDeco::reset( unsigned long changed )
{
	KCommonDecoration::reset(changed);
}

void RedmondDeco::paintEvent( QPaintEvent* )
{
    bool hicolor = QPixmap::defaultDepth() > 8;
    int fontoffset = 1;

    // Modify borderWith used by titlebar to 0, when maximized and not move or resize able
    bool border = !(maximizeMode()==MaximizeFull && !options()->moveResizeMaximizedWindows());
    int modBorderWidth = border ? borderWidth : 0;

    QPainter p(widget());

    // Obtain widget bounds.
    QRect r(widget()->rect());
    int x = r.x();
    int y = r.y();
    int x2 = r.width()-1;
    int y2 = r.height()-1;
    int w  = r.width();
    int h  = r.height();

    // Draw part of the frame that is the frame color
    // ==============================================
	QColorGroup g = options()->palette(KDecoration::ColorFrame, isActive()).active();
    p.setPen( g.background() );
    p.drawLine( x, y, x2-1, y );
    p.drawLine( x, y, x, y2-1 );

    // Draw line under title bar
    p.drawLine( x+borderWidth, y+titleHeight+borderWidth, x2-borderWidth, y+titleHeight+borderWidth );
    // Draw a hidden line that appears during shading
    p.drawLine( x+borderWidth, y2-borderWidth, x2-borderWidth, y2-borderWidth );

    // Fill out the border edges
    for (int i = 1; i < borderWidth; i++)
        p.drawRect( x+i, y+i, w-2*i, h-2*i );

    // Draw highlights and lowlights
    p.setPen(g.light());
    for (int i = 1; i <= borderWidth/3; i++) {
        p.drawLine( x+i, y+i, x2-i-1, y+i);
        p.drawLine( x+i, y+i, x+i, y2-i-1);
    }

    p.setPen(g.mid().dark(135));
    for (int i = 1; i <= borderWidth/3; i++) {
        p.drawLine( x2-i, y+i+1, x2-i, y2-i);
        p.drawLine( x+i+1, y2-i, x2-i, y2-i);
    }

    // Draw black edges
    p.setPen( g.dark().dark(155) );
    p.drawLine(x2, y, x2, y2);
    p.drawLine(x, y2, x2, y2);

    // Draw the title bar.
    // ===================
    r = titleRect();
//     QFontMetrics fm(options()->font(true));

    // Obtain blend colours.
    QColor c1 = options()->color(KDecoration::ColorTitleBar, isActive() );
    QColor c2 = options()->color(KDecoration::ColorTitleBlend, isActive() );

	QFont fnt = options()->font(true, isToolWindow() );
	if (isToolWindow() ) {
		fnt.setWeight( QFont::Normal );
		fontoffset = 0;
	}

    // Paint without a buffer if the colours are the same to
    // improve performance, and only draw gradients on hicolor displays.
    if ((c1 != c2) && hicolor) {
        // KS - Add gradient caching if needed at a later stage.

        // Create a disposable pixmap buffer for the title blend
        KPixmap* titleBuffer = new KPixmap;
        titleBuffer->resize(w-2*modBorderWidth, titleHeight);

        if (titleBuffer->depth() > 16) {
            KPixmapEffect::gradient(*titleBuffer, c1, c2,
                                    KPixmapEffect::HorizontalGradient);
        } else {
            // This enables dithering on 15 and 16bit displays, preventing
            // some pretty horrible banding effects
            QImage image = KImageEffect::gradient(titleBuffer->size(), c1, c2,
                                    KImageEffect::HorizontalGradient);

            titleBuffer->convertFromImage(image, Qt::OrderedDither);
        }

        QPainter p2( titleBuffer );

        // Since drawing the gradient is (relatively) slow, it is best
        // to draw the title text on the pixmap.

        p2.setFont( fnt );
        p2.setPen( options()->color(KDecoration::ColorFont, isActive() ));
		p2.drawText( r.x(), fontoffset, r.width()-3, r.height()-1,
                     Qt::AlignLeft | Qt::AlignVCenter, caption() );
        p2.end();

        p.drawPixmap( modBorderWidth, modBorderWidth, *titleBuffer );

        delete titleBuffer;

    } else {
       // Assume lower ended hardware, so don't use buffers.
       // Don't draw a gradient either.
       p.fillRect( modBorderWidth, modBorderWidth, w-2*modBorderWidth, titleHeight, c1 );

       // Draw the title text.
       p.setFont( fnt );
       p.setPen(options()->color(KDecoration::ColorFont, isActive() ));
	   p.drawText(r.x()+4, r.y()+fontoffset-2, r.width()-3, r.height()-1,
                  Qt::AlignLeft | Qt::AlignVCenter, caption() );
   }

}

void RedmondDecoFactory::readConfig() {
	normalTitleHeight = QFontMetrics(options()->font(true)).height();
	QFont toolFont = options()->font(true, true);
	toolFont.setWeight(QFont::Normal);
	toolTitleHeight = QFontMetrics(toolFont).height();
	switch(options()->preferredBorderSize(this)) {
	case BorderLarge:
		borderWidth = 8;
		if (normalTitleHeight < 20) normalTitleHeight = 20;
		if (toolTitleHeight < 20) toolTitleHeight = 20;
		break;
	case BorderVeryLarge:
		borderWidth = 12;
		if (normalTitleHeight < 24) normalTitleHeight = 24;
		if (toolTitleHeight < 24) toolTitleHeight = 24;
		break;
	case BorderHuge:
		borderWidth = 18;
		if (normalTitleHeight < 28) normalTitleHeight = 28;
		if (toolTitleHeight < 28) toolTitleHeight = 28;
		break;
	case BorderVeryHuge:
		borderWidth = 27;
		if (normalTitleHeight < 33) normalTitleHeight = 33;
		if (toolTitleHeight < 33) toolTitleHeight = 33;
		break;
	case BorderOversized:
		borderWidth = 40;
		if (normalTitleHeight < 40) normalTitleHeight = 40;
		if (toolTitleHeight < 40) toolTitleHeight = 40;
		break;
	case BorderTiny:
	case BorderNormal:
	default:
		borderWidth = 4;
		if (normalTitleHeight < 16) normalTitleHeight = 16;
		if (toolTitleHeight < 16) toolTitleHeight = 16;
	}
}

RedmondDecoFactory::RedmondDecoFactory()
{
	readConfig();
	create_pixmaps();
}

RedmondDecoFactory::~RedmondDecoFactory()
{
	Redmond::delete_pixmaps();
}

KDecoration *RedmondDecoFactory::createDecoration( KDecorationBridge *b )
{
	return new RedmondDeco(b, this);
}

bool RedmondDecoFactory::reset( unsigned long changed )
{
	// SettingButtons is handled by KCommonDecoration
	if ( changed & ( SettingFont | SettingBorder | SettingColors | SettingButtons ) ) {
		delete_pixmaps();
		readConfig();
		create_pixmaps();
		resetDecorations(changed);
		return true;
	} else {
		resetDecorations(changed);
		return false;
	}
}

bool RedmondDecoFactory::supports( Ability ability )
{
	switch( ability )
	{
		case AbilityAnnounceButtons:
		case AbilityButtonMenu:
		case AbilityButtonHelp:
		case AbilityButtonMinimize:
		case AbilityButtonMaximize:
		case AbilityButtonClose:
		case AbilityButtonSpacer:
			return true;
		default:
			return false;
	}
}

QList< RedmondDecoFactory::BorderSize > RedmondDecoFactory::borderSizes() const
{ // the list must be sorted
  return QList< BorderSize >() << BorderNormal << BorderLarge <<
      BorderVeryLarge <<  BorderHuge << BorderVeryHuge << BorderOversized;
}

}

extern "C" KDE_EXPORT KDecorationFactory *create_factory()
{
	return new Redmond::RedmondDecoFactory();
}


#include "redmond.moc"
// vim: ts=4
// kate: space-indent off; tab-width 4;
