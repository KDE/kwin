/*
  Gallium-IceWM themeable KWin client

  Copyright 2001
	Karol Szwed (gallium) <karlmail@usa.net>
	http://gallium.n3.net/

  This client loads most icewm 1.0.X pixmap themes, without taking into account
  specific font settings for clients, or coloured mouse cursors. Titlebar
  fonts can be changed via the kde control center. Bi-colour mouse cursors
  may be added in future if requested by users, as well as theme font support.
  Any styles using inbuilt icewm titlebar drawing without using pixmaps (e.g.
  Warp4, win95 etc.) are not fully supported, and may cause drawing errors, 
  as these themes use in-built icewm drawing mechanisms.

  When a pixmap theme is not present (or a corrupt one is present) then very
  plain title decorations are painted instead, so that users don't see
  non-painted window areas where possible ;)

  At a later date, frame shaping may be added if really requested, and an 
  update to support the latest icewm 1.1.X theme format may be made.
*/

#include <kconfig.h>
#include <kstddirs.h>
#include <kglobal.h>
#include <klocale.h>
#include <qlabel.h>
#include <qdrawutil.h>
#include <qdatetime.h>
#include <kdrawutil.h>
#include <qbitmap.h>
#include <qstring.h>
#include "../../workspace.h"
#include "../../options.h"
#include "icewm.h"

using namespace KWinInternal;


////////////////////////////////////////////////////////////////////////////////////////////
// Here's the global pixmap stuff - as memory efficient as it can be :)
////////////////////////////////////////////////////////////////////////////////////////////

// IceWM frame pixmaps
QPixmap* frameTL[] = {NULL, NULL};
QPixmap* frameT [] = {NULL, NULL};
QPixmap* frameTR[] = {NULL, NULL};
QPixmap* frameL [] = {NULL, NULL};
QPixmap* frameR [] = {NULL, NULL};
QPixmap* frameBL[] = {NULL, NULL};
QPixmap* frameB [] = {NULL, NULL};
QPixmap* frameBR[] = {NULL, NULL}; 

// Button pixmaps
QPixmap* closePix[]      = {NULL, NULL};
QPixmap* depthPix[]      = {NULL, NULL};
QPixmap* maximizePix[]   = {NULL, NULL};
QPixmap* minimizePix[]   = {NULL, NULL};
QPixmap* restorePix[]    = {NULL, NULL};
QPixmap* hidePix[]       = {NULL, NULL};
QPixmap* rollupPix[]     = {NULL, NULL};
QPixmap* rolldownPix[]   = {NULL, NULL};
QPixmap* menuButtonPix[] = {NULL, NULL};

// Titlebar pixmaps
QPixmap* titleJ[] = {NULL, NULL};
QPixmap* titleL[] = {NULL, NULL};
QPixmap* titleS[] = {NULL, NULL};
QPixmap* titleP[] = {NULL, NULL};
QPixmap* titleT[] = {NULL, NULL};
QPixmap* titleM[] = {NULL, NULL};
QPixmap* titleB[] = {NULL, NULL};
QPixmap* titleR[] = {NULL, NULL};
QPixmap* titleQ[] = {NULL, NULL};

ThemeHandler* clientHandler;

QString* titleButtonsLeft;
QString* titleButtonsRight;

QColor* colorActiveBorder;
QColor* colorInActiveBorder;
QColor* colorActiveTitleBarText;
QColor* colorInActiveTitleBarText;
QColor* colorActiveTitleBar;
QColor* colorInActiveTitleBar;
QColor* colorActiveTitleTextShadow;
QColor* colorInActiveTitleTextShadow;

int  cornerSizeX;
int  cornerSizeY;
int  titleBarHeight;
int  borderSizeX;
int  borderSizeY;

bool validframe  			= false;
bool useActiveShadow 		= false;
bool useInActiveShadow 		= false;

// KControl Settings - Read from kwinrc config file or icewm theme
bool themeTitleTextColors 	= true;		// Allow theme to set colors. kcontrol will have no effect
bool titleBarOnTop 		  	= true;		// Titlebars can be below windows too :)
bool showMenuButtonIcon   	= false;	// Draw a mini icon over the menu pixmap.
bool customButtonPositions 	= false;	// Let the theme dictate the btn pos.
bool titleBarCentered 	  	= true;



////////////////////////////////////////////////////////////////////////////////////////////
// General utility functions 
////////////////////////////////////////////////////////////////////////////////////////////

// Returns true if both active and inactive pixmaps are valid, and not null
bool validPixmaps( QPixmap* p[] )
{
	return ( p[Active]   && ( !p[Active]->isNull()   ) &&
			 p[InActive] && ( !p[InActive]->isNull() ) );
}


////////////////////////////////////////////////////////////////////////////////////////////
// ThemeHandler class
//
// This class allows us to free dynamic memory upon being reset, or unloaded
// from kwin, so we don't leak big images everywhere, and handles the theme
// initialisation / destruction in general.
////////////////////////////////////////////////////////////////////////////////////////////

ThemeHandler::ThemeHandler(): QObject( NULL )
{
	initialized = false;

	// Prevent having globals objects (use pointers to objects)
	titleButtonsLeft 	= new QString();
	titleButtonsRight 	= new QString();

	colorActiveBorder 			= new QColor();
	colorInActiveBorder 		= new QColor();
	colorActiveTitleBarText 	= new QColor();
	colorInActiveTitleBarText	= new QColor();
	colorActiveTitleBar 		= new QColor();
	colorInActiveTitleBar 		= new QColor();
	colorActiveTitleTextShadow	= new QColor();
	colorInActiveTitleTextShadow = new QColor();

	// Initialize
	readConfig();
	initTheme();
	validframe = isFrameValid();
	initialized = true;
	connect( options, SIGNAL(resetClients()), this, SLOT(slotReset()) );
}


ThemeHandler::~ThemeHandler()
{
	if (initialized) 
		freePixmaps();

	delete colorInActiveTitleTextShadow;
	delete colorActiveTitleTextShadow;
	delete colorInActiveBorder;
	delete colorActiveTitleBarText;
	delete colorInActiveTitleBarText;
	delete colorActiveTitleBar;
	delete colorInActiveTitleBar;
	delete colorActiveBorder;

	delete titleButtonsRight;
	delete titleButtonsLeft;
}



// Converts KDE style button strings to icewm style button strings
void ThemeHandler::convertButtons( QString& s )
{
	s.replace( QRegExp("_"), "");	// Spacer	(ignored)
	s.replace( QRegExp("H"), "");	// Help		(ignored)
	s.replace( QRegExp("M"), "s");	// Sysmenu
	s.replace( QRegExp("S"), "d");	// Sticky
	s.replace( QRegExp("I"), "i");	// Minimize
	s.replace( QRegExp("A"), "m");	// Maximize
	s.replace( QRegExp("X"), "x");	// Close
}


// Reverses all characters in a QString
QString ThemeHandler::reverseString( QString s )
{
	if (s.length() <= 1)
		return s;

	QString tmpStr;
	for(int i = s.length()-1; i >= 0; i--)
	{
		tmpStr += s[(unsigned int)i];
	}

	return tmpStr;
}


// This function reads the kwinrc config file
void ThemeHandler::readConfig()
{
	KConfig* conf = KGlobal::config();
	conf->setGroup("IceWM");
	themeName = conf->readEntry("CurrentTheme", "");
	themeTitleTextColors = conf->readBoolEntry("ThemeTitleTextColors", true);
	showMenuButtonIcon = conf->readBoolEntry("ShowMenuButtonIcon", false);
	titleBarOnTop = conf->readBoolEntry("TitleBarOnTop", true);

	customButtonPositions = options->customButtonPositions();
	if (customButtonPositions)
	{
		*titleButtonsLeft  = options->titleButtonsLeft();
		*titleButtonsRight = options->titleButtonsRight();

		// Convert KDE to icewm style buttons
		convertButtons( *titleButtonsLeft );		
		convertButtons( *titleButtonsRight );
	}

	// Provide a default theme alias
	if (themeName == "default")
		themeName = "";
}


// This creates the dynamic pixmaps upon loading the style
// into the pixmap buffers above, and configures the dimensioning stuff.
void ThemeHandler::initTheme()
{
	// Add a slash if required
	if ( themeName != "" )
		themeName += "/";

	// We use kconfig to read icewm config files...
	// this is easy since icewm uses key=value pairs!
	KConfig config( locate("appdata", QString("icewm-themes/") + themeName + QString("default.theme")) );

	// Load specifics, or use IceWM defaults instead.
	borderSizeX = config.readNumEntry("BorderSizeX", 6);
	borderSizeY = config.readNumEntry("BorderSizeY", 6);
	cornerSizeX = config.readNumEntry("CornerSizeX", 24);
	cornerSizeY = config.readNumEntry("CornerSizeY", 24);
	titleBarCentered = (bool) config.readNumEntry("TitleBarCentered", 0);

	// Check if readConfig() hasn't overridden this value...
	if (!showMenuButtonIcon)
		showMenuButtonIcon = (bool) config.readNumEntry("ShowMenuButtonIcon", 0); 
	titleBarHeight = config.readNumEntry("TitleBarHeight", 20);
	
	if (!customButtonPositions)
	{
		// Read in the button configuration, stripping any quotes
		// Ignore sticky 'd' on the left buttons (some themes look bad with it on by default)
		*titleButtonsLeft = config.readEntry("TitleButtonsLeft", "s");
		*titleButtonsLeft = titleButtonsLeft->replace( QRegExp(QString("\"")), "");
		*titleButtonsRight = config.readEntry("TitleButtonsRight", "xmi");
		*titleButtonsRight = titleButtonsRight->replace( QRegExp(QString("\"")), "");

		// I have no idea why the right side buttons in icewm are reversed
		*titleButtonsRight = reverseString( *titleButtonsRight );
	}

	// Read the default border and text colours from the config file
	// And use IceWM defaults if not found
	QString s;
	s = config.readEntry("ColorActiveBorder", "#C0C0C0");
	*colorActiveBorder = decodeColor( s );
	s = config.readEntry("ColorNormalBorder", "#C0C0C0");
	*colorInActiveBorder = decodeColor( s );

	// Use these as a last resort
	s = config.readEntry("ColorActiveTitleBar", "#0000A0");
	*colorActiveTitleBar = decodeColor( s );
	s = config.readEntry("ColorNormalTitleBar", "#808080");
	*colorInActiveTitleBar = decodeColor( s );

	// Read titlebar text colours
	s = config.readEntry("ColorActiveTitleBarText", "#FFFFFF");
	*colorActiveTitleBarText = decodeColor( s );
	s = config.readEntry("ColorNormalTitleBarText", "#000000");
	*colorInActiveTitleBarText = decodeColor( s );

	// Use title text shadows only with theme title text colors
	if ( themeTitleTextColors )
	{
		s = config.readEntry("ColorActiveTitleBarShadow", "");
		if (s.length() > 0)
		{
			*colorActiveTitleTextShadow = decodeColor( s );
			useActiveShadow = true;
		} else
			useActiveShadow = false;

		s = config.readEntry("ColorNormalTitleBarShadow", "");
		if (s.length() > 0)
		{
			*colorInActiveTitleTextShadow = decodeColor( s );
			useInActiveShadow = true;
		} else
			useInActiveShadow = false;
	} else
		{
			useActiveShadow = false;
			useInActiveShadow = false;
		}

	// Stretch pixmaps for speed, where required
	setPixmap( titleJ, "title", "J.xpm" );
	setPixmap( titleL, "title", "L.xpm" );
	setPixmap( titleS, "title", "S.xpm", true );

	setPixmap( titleP, "title", "P.xpm" );
	setPixmap( titleT, "title", "T.xpm", true );
	setPixmap( titleM, "title", "M.xpm" );
	setPixmap( titleB, "title", "B.xpm", true );
	setPixmap( titleR, "title", "R.xpm" );
	setPixmap( titleQ, "title", "Q.xpm" );

	setPixmap( closePix,     "close",     ".xpm" ); 
	setPixmap( depthPix,     "depth",     ".xpm" ); 
	setPixmap( maximizePix,  "maximize",  ".xpm" ); 
	setPixmap( minimizePix,  "minimize",  ".xpm" ); 
	setPixmap( restorePix,   "restore",   ".xpm" ); 
	setPixmap( hidePix,      "hide",      ".xpm" ); 
	setPixmap( rollupPix,    "rollup",    ".xpm" ); 
	setPixmap( rolldownPix,  "rolldown",  ".xpm" ); 
	setPixmap( menuButtonPix,"menuButton",".xpm" ); 

	// Top
	setPixmap( frameTL, "frame", "TL.xpm" ); 
	setPixmap( frameT,  "frame", "T.xpm", true ); 
	setPixmap( frameTR, "frame", "TR.xpm" ); 

	// Sides
	setPixmap( frameL, "frame", "L.xpm", true, Vertical ); 
	setPixmap( frameR, "frame", "R.xpm", true, Vertical ); 

	// Bottom
	setPixmap( frameBL, "frame", "BL.xpm" ); 
	setPixmap( frameB,  "frame", "B.xpm", true ); 
	setPixmap( frameBR, "frame", "BR.xpm" ); 

    // Make sure border sizes are at least reasonable...
	if (borderSizeX < 0)
		borderSizeX = 0;
	if (borderSizeY < 0)
		borderSizeY = 0;

	// This is a work-around for some themes
	if (!titleT[Active])
		titleT[Active] = duplicateValidPixmap( Active );

	if (!titleB[Active])
		titleB[Active] = duplicateValidPixmap( Active );


	if (titleL[Active] && !titleL[InActive])
		titleL[InActive] = duplicateValidPixmap( InActive, titleL[Active]->width() );

	if (titleS[Active] && !titleS[InActive])
		titleS[InActive] = duplicateValidPixmap( InActive, titleS[Active]->width() );

	if (titleP[Active] && !titleP[InActive])
		titleP[InActive] = duplicateValidPixmap( InActive, titleP[Active]->width() );

	if (titleT[Active] && !titleT[InActive])
		titleT[InActive] = duplicateValidPixmap( InActive, titleT[Active]->width() );

	if (titleM[Active] && !titleM[InActive])
		titleM[InActive] = duplicateValidPixmap( InActive, titleM[Active]->width() );

	if (titleB[Active] && !titleB[InActive])
		titleB[InActive] = duplicateValidPixmap( InActive, titleB[Active]->width() );

	if (titleR[Active] && !titleR[InActive])
		titleR[InActive] = duplicateValidPixmap( InActive, titleR[Active]->width() );
}


QPixmap* ThemeHandler::duplicateValidPixmap( bool act, int size )
{
	QPixmap* p1 = NULL;
	// Use the stretch or title pixmaps instead
	if ( titleS[act] )
		p1 = new QPixmap( *titleS[act] );
	else if ( titleB[act] )
		p1 = new QPixmap( *titleB[act] );
	else if ( titleT[act] )
		p1 = new QPixmap( *titleT[act] );

	// Stretch if required
	if ( (size != -1) && p1 && (!p1->isNull()) )
		p1 = stretchPixmap( p1, true, size );
	
	return p1;
}


// Frees all memory used by pixmaps.
void ThemeHandler::freePixmaps()
{
	freePixmapGroup( frameTL );
	freePixmapGroup( frameT );
	freePixmapGroup( frameTR );
	freePixmapGroup( frameL );
	freePixmapGroup( frameR );
	freePixmapGroup( frameBL );
	freePixmapGroup( frameB );
	freePixmapGroup( frameBR );

	freePixmapGroup( closePix );
	freePixmapGroup( depthPix );
	freePixmapGroup( maximizePix );
	freePixmapGroup( minimizePix );
	freePixmapGroup( restorePix );
	freePixmapGroup( hidePix );
	freePixmapGroup( rollupPix );
	freePixmapGroup( rolldownPix );
	freePixmapGroup( menuButtonPix );

	freePixmapGroup( titleJ );
	freePixmapGroup( titleL );
	freePixmapGroup( titleS );
	freePixmapGroup( titleP );
	freePixmapGroup( titleT );
	freePixmapGroup( titleM );
	freePixmapGroup( titleB );
	freePixmapGroup( titleR );
	freePixmapGroup( titleQ ); 
}


// Frees a dynamic pixmap group from the heap.
void ThemeHandler::freePixmapGroup( QPixmap* p[] )
{
	if (p)
	{
		if (p[Active])   delete p[Active];
		if (p[InActive]) delete p[InActive]; 
		p[Active] = NULL;
		p[InActive] = NULL;
	} else
		qWarning("kwin-icewm: freePixmapGroup - invalid QPixmap** 'p'\n");
}


// Converts icewm colors #C0C0C0 or rgb:C0/C0/C0 to QColors
QColor ThemeHandler::decodeColor( QString& s )
{
	// Make rgb:C0/C0/C0, or #C0/C0/C0  -> C0C0C0
	s.replace( QRegExp("r"), "");
	s.replace( QRegExp("g"), "");
	s.replace( QRegExp("b"), "");
	s.replace( QRegExp("#"), "");
	s.replace( QRegExp("/"), "");
	s.replace( QRegExp(":"), "");
	s.replace( QRegExp("\\"), "");
	s.replace( QRegExp("\""), "");

	// Wierd error - return grey
	if (s.length() != 6)
		return QColor( 0xC0, 0xC0, 0xC0 );

	// Qt makes this conversion very easy
	return QColor( QString("#") + s );
}


// Stretches tiny pixmaps vertically or horizontally, taking into account 
// repetition in patterns, so as not to make them mismatched
QPixmap* ThemeHandler::stretchPixmap( QPixmap* src, bool stretchHoriz, int stretchSize )
{
	if (!src) return NULL;
	if (src->isNull()) return NULL;

	int s_inc, size;

	// If its the right size already, just return
	if (stretchSize == -1)
	{
		if (stretchHoriz)
			s_inc = src->width();
		else
			s_inc = src->height();

		size = s_inc;
		if (size >= 100)
			return src;

		// Stretch an appropriate amount - taking care of pattern repetition
		while( size < 100 )
			size += s_inc;
	} else
		size = stretchSize;

	QPixmap* p = new QPixmap(); 
	if ( stretchHoriz )
		p->resize( size, src->height() );
	else
		p->resize( src->width(), size );

	QPainter pnt( p );
	if ( stretchHoriz )
		pnt.drawTiledPixmap( 0, 0, size, src->height(), *src);
	else
		pnt.drawTiledPixmap( 0, 0, src->width(), size, *src);
	pnt.end();

	delete src;
	return p; 
}


// Loads the specified Active/InActive files into the specific pixmaps, and
// can perform horizontal / vertical stretching if required for speed.
// Tries to implement some icewm specific pixmap handling for some dodgy themes
void ThemeHandler::setPixmap( QPixmap* p[], QString s1, QString s2, bool stretch, bool stretchHoriz )
{
	if ( p[Active] )
		qWarning("kwin-icewm: setPixmap - should be null (1)\n"); 
	if ( p[InActive] )
		qWarning("kwin-icewm: setPixmap - should be null (2)\n"); 

	p[Active]   = new QPixmap( locate("appdata", QString("icewm-themes/") + themeName + s1 + "A" + s2) );
	p[InActive] = new QPixmap( locate("appdata", QString("icewm-themes/") + themeName + s1 + "I" + s2) );

	// Stretch the pixmap if requested.
	if ( stretch )
	{
		if (p[Active]) 
			p[Active] = stretchPixmap( p[Active], stretchHoriz );
		if (p[InActive])
			p[InActive] = stretchPixmap( p[InActive], stretchHoriz );
	}

	if ( p[Active] && p[InActive] )
	{
		// Make sure active and inactive pixmaps are the same width for proper painting
		if (p[Active]->width() > p[InActive]->width())
			p[InActive] = stretchPixmap( p[InActive], true, p[Active]->width() );
	}

}


// returns true if there were enough pixmaps loaded to 
// draw the pixmap frame properly.
bool ThemeHandler::isFrameValid()
{
	return 
	  ( validPixmaps( frameTL ) &&
		validPixmaps( frameT )  &&
		validPixmaps( frameTR ) &&
		validPixmaps( frameL )  &&
		validPixmaps( frameR )  &&
		validPixmaps( frameBL ) &&
		validPixmaps( frameB )  &&
		validPixmaps( frameBR ) );
}


// Resets the theme, and re-clients all kwin's wrapped windows.
void ThemeHandler::slotReset()
{
	initialized = false;
	freePixmaps();
	readConfig();
	initTheme();
	validframe = isFrameValid();
	initialized = true;

	// Make kwin create new clients for each window...
	Workspace::self()->slotResetAllClientsDelayed();
}



////////////////////////////////////////////////////////////////////////////////////////////
// IceWM button class
////////////////////////////////////////////////////////////////////////////////////////////

IceWMButton::IceWMButton(Client *parent, const char *name, QPixmap* (*p)[2], bool isToggle )
    : QButton(parent, name, WStyle_Customize | WRepaintNoErase |
                            WResizeNoErase | WStyle_NoBorder)
{
	// Eliminate any possible background flicker
	setBackgroundMode( QWidget::NoBackground );
	client = parent;
	usePixmap( p );
	setFixedSize( sizeHint() );
	setToggleButton( isToggle );
}


QSize IceWMButton::sizeHint() const
{
	// Check for invalid data
	if ( validPixmaps( (QPixmap**) (*pix) ) )  // Cast to avoid dumb warning
	{
		QPixmap* p = (*pix)[ client->isActive() ? Active : InActive ];
		return( QSize(p->width(), titleBarHeight) ); 
	} else 
		return( QSize(0, 0) );
}


void IceWMButton::usePixmap( QPixmap* (*p)[2] )
{
	if ( validPixmaps( *p ) )
	{ 
		pix = p;
		setFixedSize( (*pix)[Active]->width(), titleBarHeight );
		repaint( false );
	} else
		{
			pix = NULL;
		}
}


void IceWMButton::drawButton(QPainter *pnt)
{
	if ( pix && validPixmaps(*pix) )
	{
		QPixmap* p = (*pix)[ client->isActive() ? Active : InActive ];

		if( p && (!p->isNull()) )
		{
			int width = p->width();

			// Only draw the lower pixmap 1/2 for down, and upper 1/2 for up state
			if( isDown() || isOn() )
				pnt->drawPixmap(0, 0, *p, 0, titleBarHeight, width, titleBarHeight);
			else
				pnt->drawPixmap(0, 0, *p, 0, 0, width, titleBarHeight);
		} 
	} else
		qWarning("kwin-icewm: Can't paint a null pixmap button");
}


void IceWMButton::turnOn( bool isOn )
{
	if ( isToggleButton() )
		setOn( isOn ); 
}


void IceWMButton::mousePressEvent( QMouseEvent* e )
{
	last_button = e->button();
	QMouseEvent me ( e->type(), e->pos(), e->globalPos(),
					 LeftButton, e->state() );
	QButton::mousePressEvent( &me );
}


void IceWMButton::mouseReleaseEvent( QMouseEvent* e )
{
	last_button = e->button();
	QMouseEvent me ( e->type(), e->pos(), e->globalPos(),
					 LeftButton, e->state() );
	QButton::mouseReleaseEvent( &me );
}



////////////////////////////////////////////////////////////////////////////////////////////
// IceWMClient class
////////////////////////////////////////////////////////////////////////////////////////////

IceWMClient::IceWMClient( Workspace *ws, WId w, QWidget *parent, const char *name )
    : Client( ws, w, parent, name, WResizeNoErase | WNorthWestGravity | WRepaintNoErase )
{
	// Set button pointers to null so we can track things
	for(int i= IceWMClient::BtnSysMenu; i < IceWMClient::BtnCount; i++)
		button[i] = NULL;

	// Make sure we can track the menu pixmaps too.
	menuButtonWithIconPix[Active] = NULL;
	menuButtonWithIconPix[InActive] = NULL;

	// No flicker thanks
	setBackgroundMode( NoBackground );

	// Pack the windowWrapper() window within a grid layout
	grid = new QGridLayout(this, 0, 0, 0);
	grid->setResizeMode(QLayout::FreeResize);
	grid->addRowSpacing(0, borderSizeY);	// Top grab bar

	// Do something IceWM can't do :)
	if (titleBarOnTop)
		grid->addWidget(windowWrapper(), 2, 1);
	else
		grid->addWidget(windowWrapper(), 1, 1);

	grid->setRowStretch(1, 10);
	grid->setRowStretch(2, 10);
	grid->setColStretch(1, 10); 
	grid->addRowSpacing(3, borderSizeY);
	grid->addColSpacing(0, borderSizeX);
	grid->addColSpacing(2, borderSizeX);

	// Pack the titlebar with spacers and buttons
	hb = new QHBoxLayout();
	hb->setResizeMode( QLayout::FreeResize );

	titleSpacerJ = addPixmapSpacer( titleJ );

	addClientButtons( *titleButtonsLeft );
	titleSpacerL = addPixmapSpacer( titleL );

	// Centre titlebar if required.
	QSizePolicy::SizeType spTitleBar;
	spTitleBar = titleBarCentered ? QSizePolicy::Expanding : QSizePolicy::Maximum;
	titleSpacerS = addPixmapSpacer( titleS, spTitleBar, 1 );
	titleSpacerP = addPixmapSpacer( titleP );

	titlebar = new QSpacerItem( titleTextWidth(caption()), titleBarHeight, 
								QSizePolicy::Preferred, QSizePolicy::Fixed );
	hb->addItem(titlebar);

	titleSpacerM = addPixmapSpacer( titleM );
	titleSpacerB = addPixmapSpacer( titleB, QSizePolicy::Expanding, 1 );
	titleSpacerR = addPixmapSpacer( titleR );

	addClientButtons( *titleButtonsRight );

	titleSpacerQ = addPixmapSpacer( titleQ );

	if (titleBarOnTop)
		grid->addLayout ( hb, 1, 1 );
	else
		grid->addLayout ( hb, 2, 1 );
}


IceWMClient::~IceWMClient()
{
	// Free the menu pixmaps if previously allocated
	if ( menuButtonWithIconPix[Active] )
		delete menuButtonWithIconPix[Active];
	if ( menuButtonWithIconPix[InActive] )
		delete menuButtonWithIconPix[InActive];
}


// Adds the buttons to the hbox layout as per the buttons specified
// in the button string 's'
void IceWMClient::addClientButtons( const QString& s )
{
	if (s.length() > 0)
		for(unsigned int i = 0; i < s.length(); i++)	
		{
			switch ( s[i].latin1() ) 
			{
				case 's':
					// Create the menu icons, and render with the current mini-icon 
					// if explicitly requested by the theme.
					if ( validPixmaps(menuButtonPix) && !button[BtnSysMenu])
					{
						if (showMenuButtonIcon) {
							renderMenuIcons();
							button[BtnSysMenu] = new IceWMButton(this, "menu", &menuButtonWithIconPix); 
						}
						else
							button[BtnSysMenu] = new IceWMButton(this, "menu", &menuButtonPix); 
	
						connect( button[BtnSysMenu], SIGNAL(pressed()), this, SLOT(menuButtonPressed()));
						hb->addWidget( button[BtnSysMenu] );
					}
					break;

				case 'x':
					if ( validPixmaps(closePix) && !button[BtnClose] )
					{
						button[BtnClose] = new IceWMButton(this, "close", &closePix);
						hb->addWidget( button[BtnClose] );
						connect( button[BtnClose], SIGNAL(clicked()), this, SLOT(closeWindow()));
					}
					break;

				case 'm':
					if ( validPixmaps(maximizePix) && !button[BtnMaximize] && isMaximizable() )
					{
						button[BtnMaximize] = new IceWMButton(this, "maximize", &maximizePix);
						hb->addWidget( button[BtnMaximize] );
						connect( button[BtnMaximize], SIGNAL(clicked()), this, SLOT(slotMaximize()));
					}
					break;

				case 'i':
					if ( validPixmaps(minimizePix) && !button[BtnMinimize] && isMinimizable() )
					{
						button[BtnMinimize] = new IceWMButton(this, "minimize", &minimizePix);
						hb->addWidget( button[BtnMinimize] );
						connect( button[BtnMinimize], SIGNAL(clicked()), this, SLOT(iconify()));
					}
					break;

				/* Not yet implemented - how's hide useful anyway?
				case 'h':
					if ( button[BtnHide]  && !button[BtnHide] )
						hb->addWidget( button[BtnHide] );
					break; */

				/* Re-enable this when kwin has void shadeChange(bool s) in clients.cpp
				case 'r':
					// NOTE: kwin doesn't have toggleShade() in clients.h !
				    if ( validPixmaps(rollupPix) && !button[BtnRollup] )
				    {
						button[BtnRollup] = new IceWMButton(this, "shade", isShade() ? &rolldownPix : &rollupPix);
						connect( button[BtnRollup], SIGNAL(clicked()), this, SLOT(toggleShade()) );
						hb->addWidget( button[BtnRollup] );
					}
					break; 	*/

				case 'd':
					// Depth == sticky
					if ( validPixmaps(depthPix) && !button[BtnDepth] )
					{
						button[BtnDepth] = new IceWMButton(this, "sticky", &depthPix, true );
						button[BtnDepth]->turnOn( isSticky() );
						hb->addWidget( button[BtnDepth] );
						connect( button[BtnDepth], SIGNAL(clicked()), this, SLOT(toggleSticky()));
					}
					break;
			}
		}
}


// Adds a pixmap to the titlebar layout via the use of a nice QSpacerItem
QSpacerItem* IceWMClient::addPixmapSpacer( QPixmap* p[], QSizePolicy::SizeType s, int hsize )
{
	QSpacerItem* sp;

	// Add a null spacer for zero image
	if ( p && p[Active] )
	{
		int w = (hsize == -1) ? p[Active]->width(): hsize;
		sp = new QSpacerItem( w, titleBarHeight, s, QSizePolicy::Fixed );  
	}
	else
		sp = new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Fixed );

	hb->addItem( sp );
	return sp;
}


void IceWMClient::renderMenuIcons()
{
	if (validPixmaps(menuButtonPix) && (!miniIcon().isNull()))
		for(int i = 0; i < 2; i++)
		{
			if ( menuButtonWithIconPix[i] )
				 delete menuButtonWithIconPix[i];
        
			// Try to be more friendly to dodgy themes - icewm assumes a square menu button
			// but some pixmap themes don't provide a square menu button.
	        menuButtonWithIconPix[i] = new QPixmap( titleBarHeight, 2*titleBarHeight );

			QPainter pnt( menuButtonWithIconPix[i] );
			pnt.drawPixmap(0, 0, *menuButtonPix[i]);

			int offset = (titleBarHeight - miniIcon().width())/2;
			// Paint the mini icon over the menu pixmap in the centre
			pnt.drawPixmap( offset, offset, miniIcon() );
			pnt.drawPixmap( offset, titleBarHeight+offset, miniIcon() );
			pnt.end();
		}
}


void IceWMClient::slotMaximize()
{
	if ( button[BtnMaximize]->last_button == MidButton )
		maximize( MaximizeVertical );
	else if ( button[BtnMaximize]->last_button == RightButton )
		maximize( MaximizeHorizontal );
	else
		maximize();
}


int IceWMClient::titleTextWidth( const QString& s )
{
	// Obtains the actual width of the text, using the titlebar font
	QSize size;
	QFontMetrics fm( options->font(true) );
	size = fm.size( 0, s );
	return size.width();
}


// Repaint nicely upon resize to minimise flicker.
void IceWMClient::resizeEvent( QResizeEvent* e )
{
	Client::resizeEvent( e );
	calcHiddenButtons();

	if (isVisibleToTLW()) 
	{
		 update(rect());
		int dx = 0;
		int dy = 0;

		if ( e->oldSize().width() != width() )
			dx = 32 + QABS( e->oldSize().width() -  width() );

		if ( e->oldSize().height() != height() )
			dy = 8 + QABS( e->oldSize().height() -  height() );

		if ( dy )
			update( 0, height() - dy + 1, width(), dy );

		if ( dx ) 
		{
			update( width() - dx + 1, 0, dx, height() );
			update( QRect( QPoint(4,4), titlebar->geometry().bottomLeft() - QPoint(1,0) ) );
			update( QRect( titlebar->geometry().topRight(), QPoint( width() - 4, titlebar->geometry().bottom() ) ) );
			QApplication::postEvent( this, new QPaintEvent( titlebar->geometry(), FALSE ) );
		}
	} 
}


// IceWM Paint magic goes here.
void IceWMClient::paintEvent( QPaintEvent* )
{
	QColor colorTitleShadow;
	QColor colorTitle;
	QColor c1;
	int rx, rw;

	QPainter p(this);
	int act = isActive() ? Active: InActive; 

	// Determine titlebar shadow colors
	bool useShadow = isActive() ? useActiveShadow : useInActiveShadow;
	if ( useShadow )
		colorTitleShadow = isActive() ? *colorActiveTitleTextShadow : *colorInActiveTitleTextShadow;

	if ( themeTitleTextColors )
 		colorTitle = isActive()? *colorActiveTitleBarText : *colorInActiveTitleBarText;
	else
		colorTitle = options->color(Options::Font, isActive());

	// Obtain widget bounds.
	QRect r;
	r = rect();
	int fillWidth  = r.width()  - 2*borderSizeX;
	int y = r.y();
	int x = r.x();
	int w = r.width();
	int h = r.height();
 
	// Do we have pixmaps for the frame?
	if (validframe)
	{
		// Top corner
		p.drawPixmap(0, 0, *frameTL[ act ], 0, 0, cornerSizeX, borderSizeY);
		p.drawPixmap(0, 0, *frameTL[ act ], 0, 0, borderSizeX, cornerSizeY); 

		// Top right corner
		p.drawPixmap(w-cornerSizeX, 0, *frameTR[ act ], frameTR[act]->width()-cornerSizeX, 0, cornerSizeX, borderSizeY);
		p.drawPixmap(w-borderSizeX, 0, *frameTR[ act ], frameTR[act]->width()-borderSizeX, 0, borderSizeX, cornerSizeY);

		// Top bar
		p.drawTiledPixmap( cornerSizeX, 0, w-(2*cornerSizeX), borderSizeY, *frameT[ act ] );

		// Left bar
		p.drawTiledPixmap( 0, cornerSizeY, borderSizeX, h-(2*cornerSizeY), *frameL[ act ] );

		// Right bar
		p.drawTiledPixmap( w-borderSizeX, cornerSizeY, borderSizeX, h-(2*cornerSizeY), *frameR[ act ],
		 				   frameR[act]->width()-borderSizeX );
      
		// Bottom left corner
		p.drawPixmap(0, h-borderSizeY, *frameBL[ act ], 0, frameBL[act]->height()-borderSizeY, cornerSizeX, borderSizeY);
		p.drawPixmap(0, h-cornerSizeY, *frameBL[ act ], 0, frameBL[act]->height()-cornerSizeY, borderSizeX, cornerSizeY);

		// Bottom right corner
		p.drawPixmap(w-cornerSizeX, h-borderSizeY, *frameBR[ act ], frameBR[act]->width()-cornerSizeX, frameBR[act]->height()-borderSizeY, cornerSizeX, borderSizeY);
		p.drawPixmap(w-borderSizeX, h-cornerSizeY, *frameBR[ act ], frameBR[act]->width()-borderSizeX, frameBR[act]->height()-cornerSizeY, borderSizeX, cornerSizeY);

		// Bottom bar
		p.drawTiledPixmap(cornerSizeX, h-borderSizeY, w-(2*cornerSizeX), borderSizeY, *frameB[ act ], 
						 0, frameB[ act ]->height()-borderSizeY );

	} else
	{
		// Draw a stock IceWM frame instead of a pixmap frame
		c1 = isActive() ? *colorActiveBorder : *colorInActiveBorder;
		p.setPen( c1.light(135) );
		p.drawLine(0, 0, w-2, 0);
		p.drawLine(0, 0, 0, h-2);

		p.setPen(c1);
		p.drawLine(1, 1, w-3, 1);
		p.drawLine(1, 1, 1, h-3);

		p.setPen( c1.dark(140) );
		p.drawLine(1, h-2, w-2, h-2);
		p.drawLine(w-2, 1, w-2, h-2);

		p.setPen( Qt::black );
		p.drawLine(w-1, 0, w-1, h-1);
		p.drawLine(0, h-1, w-1, h-1);

		// Fill frame border if required
		if (borderSizeX > 2)
		{
			// Fill Vertical sizes
			p.fillRect( x+2, y+2, borderSizeX-2, h-4, c1);
			p.fillRect( w-borderSizeX, y+2, borderSizeX-2, h-4, c1);
		}
		if (borderSizeY > 2)
		{
			// Fill horizontal frame parts
			p.fillRect( x+borderSizeX, y+2, fillWidth, borderSizeY-2, c1);
			p.fillRect( x+borderSizeX, h-borderSizeY, fillWidth, borderSizeY-2, c1);
		}
	}

	// Draw the title elements, which are visible
	QPixmap* titleBuffer = new QPixmap( width()-(2*borderSizeX), titleBarHeight );
	QPainter p2( titleBuffer, this );
	titleBuffer->fill( act ? *colorActiveTitleBar : *colorInActiveTitleBar );

	r = titleSpacerJ->geometry();
	if (!r.isEmpty() && titleJ[ act ])
		p2.drawPixmap( r.x()-borderSizeX, 0, *titleJ[ act ]);

	r = titleSpacerL->geometry();
	if (!r.isEmpty() && titleL[ act ])
		p2.drawPixmap( r.x()-borderSizeX, 0, *titleL[ act ]);

	r = titleSpacerS->geometry();
	if (!r.isEmpty() && titleS[ act ])
		p2.drawTiledPixmap( r.x()-borderSizeX, 0, r.width(), titleBarHeight, *titleS[ act ]);

	r = titleSpacerP->geometry();
	if (!r.isEmpty() && titleP[ act ])
		p2.drawPixmap( r.x()-borderSizeX, 0, *titleP[ act ]);

	r = titlebar->geometry();
	if (!r.isEmpty() && titleT[ act ] )
		p2.drawTiledPixmap( r.x()-borderSizeX, 0, r.width(), titleBarHeight, *titleT[ act ]);

	r = titleSpacerM->geometry();
	if (!r.isEmpty() && titleM[ act ])
		p2.drawPixmap( r.x()-borderSizeX, 0, *titleM[ act ], 0, 0, r.width(), r.height());

	r = titleSpacerB->geometry();
	if (!r.isEmpty() && titleB[ act ])
		p2.drawTiledPixmap( r.x()-borderSizeX, 0, r.width(), titleBarHeight, *titleB[ act ]);

	r = titleSpacerR->geometry();
	if (!r.isEmpty() && titleR[ act ])
		p2.drawPixmap( r.x()-borderSizeX, 0, *titleR[ act ], 0, 0, r.width(), r.height());

	r = titleSpacerQ->geometry();
	if (!r.isEmpty() && titleQ[ act ])
		p2.drawPixmap( r.x()-borderSizeX, 0, *titleQ[ act ], 0, 0, r.width(), r.height());

	p2.setFont( options->font(true) );

	// Pre-compute as much as possible
	r = titlebar->geometry();
	rx = r.x() - borderSizeX;
	rw = width()-(2*borderSizeX)-r.x();

	// Paint a title text shadow if requested
	if ( useShadow )
	{
		p2.setPen( colorTitleShadow );
		p2.drawText(rx+1, 1, rw, titleBarHeight, AlignLeft|AlignVCenter, caption());
	} 
	
	// Draw the title text
	p2.setPen( colorTitle );
	p2.drawText(rx, 0, rw, titleBarHeight, AlignLeft|AlignVCenter, caption());
	p2.end();

	bitBlt( this, borderSizeX, hb->geometry().y(), titleBuffer );
	
	delete titleBuffer;
}


void IceWMClient::showEvent(QShowEvent *ev)
{
	calcHiddenButtons();

	titlebar->changeSize( titleTextWidth(caption()), titleBarHeight, 
						  QSizePolicy::Preferred, QSizePolicy::Fixed );
	grid->activate();
	show();
	Client::showEvent(ev);
}


void IceWMClient::mouseDoubleClickEvent( QMouseEvent * e )
{
	QRect r;

	if (titleBarOnTop)
		r.setRect( borderSizeX, borderSizeY, width()-(2*borderSizeX), titleBarHeight);
	else
		r.setRect( borderSizeX, height()-borderSizeY-titleBarHeight, width()-(2*borderSizeX), titleBarHeight);

	if ( r.contains( e->pos() ) )
		workspace()->performWindowOperation( this, options->operationTitlebarDblClick() );

	workspace()->requestFocus( this );
}


// Called via Client class when the miniIcon() changes
void IceWMClient::iconChange()
{
	if (validPixmaps(menuButtonPix) && showMenuButtonIcon)
	{
		if (button[BtnSysMenu])
		{
			renderMenuIcons();
			button[BtnSysMenu]->usePixmap( &menuButtonWithIconPix );
			if (button[BtnSysMenu]->isVisible())
				button[BtnSysMenu]->repaint(false);
		}
	} 
}


// Please don't modify the following unless you want layout problems
void IceWMClient::captionChange( const QString& s )
{
	QRect r( 0, borderSizeY, geometry().width(), titleBarHeight);

	titlebar->changeSize( titleTextWidth( s ), titleBarHeight, 
						  QSizePolicy::Preferred, QSizePolicy::Fixed );
	titlebar->invalidate();
    grid->activate();
    repaint( r, false );
}


void IceWMClient::maximizeChange(bool m)
{
	// Change the button pixmap to restore if required
	if (button[BtnMaximize] && validPixmaps(restorePix))
		button[BtnMaximize]->usePixmap( m ? &restorePix : &maximizePix );
}


/* Re-enable this when kwin has void shadeChange(bool s) in clients.cpp
void IceWMClient::shadeChange(bool s) 
{
	// Change the button pixmap to rolldown if required
	if (button[BtnRollup] && validPixmaps(rolldownPix))
		button[BtnRollup]->usePixmap( s ? rolldownPix : rollupPix );
} 
*/


void IceWMClient::activeChange(bool)
{
	repaint(false);

    // Reset the button pixmaps.
	for(int i= IceWMClient::BtnSysMenu; i < IceWMClient::BtnCount; i++)
		if(button[i])
			button[i]->repaint( false );
}


// This does the showing / hiding button magic
// for variable positioned buttons.
void IceWMClient::calcHiddenButtons()
{
	const int minwidth = 220; // Minimum width where all buttons are shown
	const int btn_width = 20; // Average width

	// Show/Hide buttons in this order - Sticky, Maximize, Menu, Minimize, Close.
	IceWMButton* btnArray[] = { button[BtnDepth], button[BtnMaximize], button[BtnSysMenu],
								button[BtnMinimize], button[BtnClose] };

	int current_width = width();
	int count = 0;
	int i;

	// Find out how many buttons we have to hide.
	while (current_width < minwidth)
	{
		current_width += btn_width;
		count++;
	}

	// Bound the number of buttons to hide
	if (count > 5) count = 5;

	// Hide the required buttons...
	for(i = 0; i < count; i++)
	{
		if (btnArray[i] && btnArray[i]->isVisible() )
			btnArray[i]->hide();
	}

	// Show the rest of the buttons...
	for(i = count; i < 5; i++)
	{
		if (btnArray[i] && (!btnArray[i]->isVisible()) )
			btnArray[i]->show();		
	}
}


// Mouse position code modified from that in workspace.cpp
Client::MousePosition IceWMClient::mousePosition( const QPoint& p ) const
{
    int rangeX = cornerSizeX;
    int rangeY = cornerSizeY;
    int borderX = borderSizeX;
    int borderY = borderSizeY;

    MousePosition m = Nowhere;

    if ((p.x()  > borderX && p.x() < width()  - borderX) &&
	    ( p.y() > borderY && p.y() < height() - borderY))
	return Center;

    if ( p.y() <= rangeY && p.x() <= rangeX)
		m = TopLeft;
    else if ( p.y() >= height()-rangeY && p.x() >= width()-rangeX)
		m = BottomRight;
    else if ( p.y() >= height()-rangeX && p.x() <= rangeX)
		m = BottomLeft;
    else if ( p.y() <= rangeY && p.x() >= width()-rangeX)
		m = TopRight;
    else if ( p.y() <= borderY )
		m = Top;
    else if ( p.y() >= height()-borderY )
		m = Bottom;
    else if ( p.x() <= borderX )
		m = Left;
    else if ( p.x() >= width()-borderX )
    	m = Right;
    else
		m = Center;
	return m;
}


// Make sure the menu button follows double click conventions set in kcontrol
void IceWMClient::menuButtonPressed()
{
	static QTime* t = 0;
	static IceWMClient* tc = 0;
	if ( !t )
		t = new QTime;
 
	if ( tc != this || t->elapsed() > QApplication::doubleClickInterval() )
	{
		QPoint menuPoint ( button[BtnSysMenu]->rect().bottomLeft() );

		// Move to right if menu on rhs, otherwise on left
		// and make this depend on windowWrapper(), not button.
		workspace()->clientPopup(this)->popup( button[BtnSysMenu]->mapToGlobal( menuPoint ));

		// Animate the menu button when pressed
		if (button[BtnSysMenu])
			button[BtnSysMenu]->animateClick();
	}
	else 
		closeWindow();

	t->start();
	tc = this;           
}


extern "C"
{
	Client* allocate(Workspace *ws, WId w, int)
    {
        return(new IceWMClient(ws, w));
    }

	void init()
	{
		clientHandler = new ThemeHandler;
	}
	
	void reset()
	{
		// The ThemeHandler does this automatically
	}

	void deinit()
	{
		delete clientHandler;
	}
}


#include "icewm.moc"

// vim: ts=4
