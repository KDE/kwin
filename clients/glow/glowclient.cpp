/***************************************************************************
                          glowclient.cpp  -  description
                             -------------------
    begin                : Thu Sep 6 2001
    copyright            : (C) 2001 by Henning Burchardt
    email                : h_burchardt@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <cmath>
#include <qpixmap.h>
#include <qbitmap.h>
#include <qpainter.h>
#include <qapplication.h>
#include <qlayout.h>
#include <qfontmetrics.h>
#include <kpixmapeffect.h>
#include <kpixmap.h>
#include <kconfig.h>
#include <kwin/options.h>
#include <kwin/workspace.h>
#include "bitmaps.h"
#include "glowclient.h"
#include "glowbutton.h"

using namespace KWinInternal;

//-----------------------------------------------------------------------------

GlowClientConfig::GlowClientConfig()
{
}

void GlowClientConfig::load()
{
	KConfig conf("kwinglowrc");
	conf.setGroup("General");

	QColor defaultButtonColor = Qt::white;
	QColor defaultCloseButtonColor = Qt::red;

	QString stickyButtonGlowType =
		conf.readEntry("stickyButtonGlowType", "TitleBar");
	if(stickyButtonGlowType=="TitleBar")
		stickyButtonGlowColor = options->color(Options::TitleBar);
	else if(stickyButtonGlowType=="TitleBlend")
		stickyButtonGlowColor = options->color(Options::TitleBlend);
	else // if( stickyButtonGlowType=="Custom" )
		stickyButtonGlowColor = conf.readColorEntry(
			"stickyButtonGlowColor", &defaultButtonColor);

	QString helpButtonGlowType =
		conf.readEntry("helpButtonGlowType", "TitleBar");
	if(helpButtonGlowType=="TitleBar")
		helpButtonGlowColor = options->color(Options::TitleBar);
	else if(helpButtonGlowType=="TitleBlend")
		helpButtonGlowColor = options->color(Options::TitleBlend);
	else // if( helpButtonGlowType=="Custom" )
		helpButtonGlowColor = conf.readColorEntry(
			"helpButtonGlowColor", &defaultButtonColor);

	QString iconifyButtonGlowType =
		conf.readEntry("iconifyButtonGlowType", "TitleBar");
	if(iconifyButtonGlowType=="TitleBar")
		iconifyButtonGlowColor = options->color(Options::TitleBar);
	else if(iconifyButtonGlowType=="TitleBlend")
		iconifyButtonGlowColor = options->color(Options::TitleBlend);
	else // if( iconifyButtonGlowType=="Custom" )
		iconifyButtonGlowColor = conf.readColorEntry(
			"iconifyButtonGlowColor", &defaultButtonColor);

	QString maximizeButtonGlowType =
		conf.readEntry("maximizeButtonGlowType", "TitleBar");
	if(maximizeButtonGlowType=="TitleBar")
		maximizeButtonGlowColor = options->color(Options::TitleBar);
	else if(maximizeButtonGlowType=="TitleBlend")
		maximizeButtonGlowColor = options->color(Options::TitleBlend);
	else // if( maximizeButtonGlowType=="Custom" )
		maximizeButtonGlowColor = conf.readColorEntry(
			"maximizeButtonGlowColor", &defaultButtonColor);

	QString closeButtonGlowType =
		conf.readEntry("closeButtonGlowType", "Custom");
	if(closeButtonGlowType=="TitleBar")
		closeButtonGlowColor = options->color(Options::TitleBar);
	else if(closeButtonGlowType=="TitleBlend")
		closeButtonGlowColor = options->color(Options::TitleBlend);
	else // if( closeButtonGlowType=="Custom" )
		closeButtonGlowColor = conf.readColorEntry(
			"closeButtonGlowColor", &defaultCloseButtonColor);
}

//-----------------------------------------------------------------------------

GlowClientGlobals *GlowClientGlobals::m_instance = 0;

GlowClientGlobals *GlowClientGlobals::instance()
{
// cerr << "GlowClientGlobals " << "instance " << endl;
	if( ! m_instance )
		m_instance = new GlowClientGlobals();
	return m_instance;
}

GlowClientGlobals::~GlowClientGlobals()
{
	deletePixmaps();
	m_instance = 0;
}

QPixmap *GlowClientGlobals::getPixmap(
	int type, bool isActive, bool isLeft, bool isSmall)
{
	int modifiers = 0;
	modifiers |= isActive ? Active : NotActive;
	modifiers |= isLeft ? PosLeft : PosRight;
	modifiers |= isSmall ? SizeSmall : SizeNormal;
	return m_pixmapMap[type][modifiers];
}

GlowClientGlobals::GlowClientGlobals()
	: QObject()
{
// cerr << "GlowClientGlobals " << "GlowClientGlobals " << endl;
	buttonFactory = new GlowButtonFactory();
	readConfig();
	createPixmaps();
}

void GlowClientGlobals::readConfig()
{
	config = new GlowClientConfig();
	config->load();
}

std::vector<int> GlowClientGlobals::getPixmapTypes()
{
	std::vector<int> pixmapTypes;
	pixmapTypes.push_back(StickyOn);
	pixmapTypes.push_back(StickyOff);
	pixmapTypes.push_back(Help);
	pixmapTypes.push_back(Iconify);
	pixmapTypes.push_back(MaximizeOn);
	pixmapTypes.push_back(MaximizeOff);
	pixmapTypes.push_back(Close);

	return pixmapTypes;
}

std::vector<int> GlowClientGlobals::getPixmapModifiers()
{
	std::vector<int> pixmapModifiers;
	pixmapModifiers.push_back(Active | PosLeft | SizeSmall);
	pixmapModifiers.push_back(Active | PosLeft | SizeNormal);
	pixmapModifiers.push_back(Active | PosRight | SizeSmall);
	pixmapModifiers.push_back(Active | PosRight | SizeNormal);
	pixmapModifiers.push_back(NotActive | PosLeft | SizeSmall);
	pixmapModifiers.push_back(NotActive | PosLeft | SizeNormal);
	pixmapModifiers.push_back(NotActive | PosRight | SizeSmall);
	pixmapModifiers.push_back(NotActive | PosRight | SizeNormal);

	return pixmapModifiers;
}

void GlowClientGlobals::reset()
{
// cerr << "GlowClientGlobals " << "reset " << endl;
	deletePixmaps();
	delete config;
	readConfig();
	createPixmaps();
}

void GlowClientGlobals::createPixmaps()
{
// cerr << "GlowClientGlobals " << "createPixmaps " << endl;
	std::vector<int> types = getPixmapTypes();
	for( int i=0; i<types.size(); i++ )
	{
		std::vector<int> modifiers = getPixmapModifiers();
		for( int j=0; j<modifiers.size(); j++ )
			m_pixmapMap[types[i]][modifiers[j]] =
				createPixmap(types[i],modifiers[j]);
	}
}

void GlowClientGlobals::deletePixmaps()
{
	std::vector<int> types = getPixmapTypes();
	for( int i=0; i<types.size(); i++ )
	{
		if( m_pixmapMap.find(types[i]) == m_pixmapMap.end() )
			continue;
		std::vector<int> modifiers = getPixmapModifiers();
		std::map<int, QPixmap*> modifierMap = m_pixmapMap[types[i]];
		for( int j=0; j<modifiers.size(); j++ )
			if( modifierMap.find(modifiers[j]) == modifierMap.end() )
				delete modifierMap[modifiers[j]];
	}
}

QPixmap *GlowClientGlobals::createPixmap(int type, int modifiers)
{
// cerr << "GlowClientGlobals " << "createPixmap " << endl;
	bool isActive;
	if( modifiers & Active )
		isActive = true;
	else
		isActive = false;
	int size;
	if( modifiers & SizeNormal )
		size = DEFAULT_BITMAP_SIZE;
	else
		size = SMALL_BITMAP_SIZE;
	QColorGroup g;
	if( modifiers & PosLeft )
		g = options->colorGroup(Options::TitleBar, isActive);
	else
		g = options->colorGroup(Options::ButtonBg, isActive);
	QColor c;
	if( qGray(g.background().rgb()) <= 127 ) // background is dark
		c = Qt::white;
	else // background is light
		c = Qt::black;

	QPixmap pm(size, size);
	pm.fill(c);

	if( type == StickyOn )
	{
		if( modifiers & SizeNormal )
			pm.setMask(QBitmap(size, size, stickyon_bits, true));
		else
			pm.setMask(QBitmap(size, size, stickyon_small_bits, true));
		return buttonFactory->createGlowButtonPixmap(
			QSize(size,size), config->stickyButtonGlowColor, g, pm);
	}
	else if( type==StickyOff )
	{
		if( modifiers & SizeNormal )
			pm.setMask(QBitmap(size, size, stickyoff_bits, true));
		else
			pm.setMask(QBitmap(size, size, stickyoff_small_bits, true));
		return buttonFactory->createGlowButtonPixmap(
			QSize(size,size), config->stickyButtonGlowColor, g, pm);
	}
	else if( type==Help )
	{
		if( modifiers & SizeNormal )
			pm.setMask(QBitmap(size, size, help_bits, true));
		else
			pm.setMask(QBitmap(size, size, help_small_bits, true));
		return buttonFactory->createGlowButtonPixmap(
			QSize(size,size), config->helpButtonGlowColor, g, pm);
	}
	else if( type==Iconify )
	{
		if( modifiers & SizeNormal )
			pm.setMask(QBitmap(size, size, minimize_bits, true));
		else
			pm.setMask(QBitmap(size, size, minimize_small_bits, true));
		return buttonFactory->createGlowButtonPixmap(
			QSize(size,size), config->iconifyButtonGlowColor, g, pm);
	}
	else if( type==MaximizeOn )
	{
		if( modifiers & SizeNormal )
			pm.setMask(QBitmap(size, size, maximizeon_bits, true));
		else
			pm.setMask(QBitmap(size, size, maximizeon_small_bits, true));
		return buttonFactory->createGlowButtonPixmap(
			QSize(size,size), config->maximizeButtonGlowColor, g, pm);
	}
	else if( type==MaximizeOff )
	{
		if( modifiers & SizeNormal )
			pm.setMask(QBitmap(size, size, maximizeoff_bits, true));
		else
			pm.setMask(QBitmap(size, size, maximizeoff_small_bits, true));
		return buttonFactory->createGlowButtonPixmap(
			QSize(size,size), config->maximizeButtonGlowColor, g, pm);
	}
	else if( type==Close )
	{
		if( modifiers & SizeNormal )
			pm.setMask(QBitmap(size, size, close_bits, true));
		else
			pm.setMask(QBitmap(size, size, close_small_bits, true));
		return buttonFactory->createGlowButtonPixmap(
			QSize(size,size), config->closeButtonGlowColor, g, pm);
	}
	else // should not happen
		return buttonFactory->createGlowButtonPixmap(
			QSize(size,size), Qt::white, g, QPixmap());
}

//-----------------------------------------------------------------------------

GlowClient::GlowClient(KWinInternal::Workspace *ws, WId w,
	QWidget *parent, const char* name )
	: KWinInternal::Client(ws, w, parent, name),
		m_stickyButton(0), m_helpButton(0), m_minimizeButton(0),
		m_maximizeButton(0), m_closeButton(0),
		m_mainLayout(0)
{
// cerr << "GlowClient " << "GlowClient " << endl;
	createButtons();
	resetLayout();
	repaint();

	QObject::connect(KWinInternal::options, SIGNAL(resetClients()),
		this, SLOT(slotReset()));
}

GlowClient::~GlowClient()
{
// cerr << "GlowClient " << "~GlowClient " << endl;
}

void GlowClient::resizeEvent( QResizeEvent *e )
{
	Client::resizeEvent(e);
	doShape();
	repaint(false);
}

void GlowClient::paintEvent( QPaintEvent *e )
{
// cerr << "GlowClient " << "paintEvent " << endl;
	Client::paintEvent(e);

	QRect r_this = rect();
	QRect r_title = m_titleSpacer->geometry();
	QColorGroup titleCg =
		options->colorGroup(Options::TitleBar, isActive());
	QColorGroup titleBlendCg =
		options->colorGroup(Options::TitleBlend, isActive());
	QColorGroup cg = colorGroup();
	QColor titleColor = options->color(Options::TitleBar, isActive());
	QColor titleBlendColor = options->color(Options::TitleBlend, isActive());
	QPainter painter;

	painter.begin(this);
	painter.setPen(cg.dark());
	// draw an outer dark frame
	painter.drawRect(r_this);
	// draw a line below title bar
	painter.drawLine(0, r_title.height()-1,
		r_this.width()-1, r_title.height()-1);
	painter.end();

	// pixmap for title bar
//	int titleBufferWidth = r_title.x() + r_title.width();
	int titleBufferWidth = width()-2;
	int titleBufferHeight = r_title.height()-2;
	KPixmap titleBuffer(QSize(titleBufferWidth, titleBufferHeight));
	KPixmapEffect::gradient(titleBuffer, titleColor, titleBlendColor,
		KPixmapEffect::DiagonalGradient);
	painter.begin(&titleBuffer);
	painter.setFont(options->font(isActive()));
	painter.setPen(options->color(Options::Font, isActive()));
	painter.drawText(r_title.x(), 0,
		r_title.x()+r_title.width(), titleBufferHeight,
		Qt::AlignLeft | Qt::AlignVCenter, caption() );
	painter.end();

	painter.begin(this);
	painter.drawPixmap(1, 1, titleBuffer);
	painter.end();
}

void GlowClient::showEvent( QShowEvent *e )
{
	Client::showEvent(e);
	doShape();
	repaint(false);
}

void GlowClient::mouseDoubleClickEvent( QMouseEvent *e )
{
	if( m_titleSpacer->geometry().contains(e->pos()) )
		workspace()->performWindowOperation(
			this, options->operationTitlebarDblClick());
}

void GlowClient::captionChange(const QString&)
{
	repaint(false);
}

void GlowClient::activeChange(bool)
{
	updateButtonPixmaps();
	repaint(false);
}

void GlowClient::iconChange()
{
	// we have no (t yet an) icon button, so do nothing
}

void GlowClient::stickyChange(bool on)
{
	if(on)
		m_stickyButton->setPixmap(
			GlowClientGlobals::instance()->getPixmap(
				GlowClientGlobals::StickyOn, isActive(),
				isLeft(m_stickyButton), isTool()));
	else
		m_stickyButton->setPixmap(
			GlowClientGlobals::instance()->getPixmap(
				GlowClientGlobals::StickyOff, isActive(),
				isLeft(m_stickyButton), isTool()));
}

void GlowClient::maximizeChange(bool on)
{
	if(on)
		m_maximizeButton->setPixmap(
			GlowClientGlobals::instance()->getPixmap(
				GlowClientGlobals::MaximizeOn, isActive(),
				isLeft(m_maximizeButton), isTool()));
	else
		m_maximizeButton->setPixmap(
			GlowClientGlobals::instance()->getPixmap(
				GlowClientGlobals::MaximizeOff, isActive(),
				isLeft(m_maximizeButton), isTool()));
}

Client::MousePosition GlowClient::mousePosition(const QPoint &pos) const
{
	// we have no resize handle yet so we return the default position
	return Client::mousePosition(pos);
}

void GlowClient::createButtons()
{
// cerr << "GlowClient " << "createButtons " << endl;
	GlowClientGlobals *globals = GlowClientGlobals::instance();
	GlowButtonFactory *factory = globals->buttonFactory;
	int s = isTool() ? SMALL_BITMAP_SIZE : DEFAULT_BITMAP_SIZE;
	QSize size(s,s);

	m_stickyButton = factory->createGlowButton(
		this, "StickyButton", "Sticky");
	m_stickyButton->setFixedSize(size);
	QObject::connect(m_stickyButton, SIGNAL(clicked()),
		this, SLOT(toggleSticky()));
	m_buttonList.insert(m_buttonList.end(), m_stickyButton);

	m_helpButton = factory->createGlowButton(
		this, "HelpButton", "Help");
	m_helpButton->setFixedSize(size);
	QObject::connect(m_helpButton, SIGNAL(clicked()),
		this, SLOT(contextHelp()));
	m_buttonList.insert(m_buttonList.end(), m_helpButton);

	m_minimizeButton = factory->createGlowButton(
		this, "IconifyButton", "Minimize");
	m_minimizeButton->setFixedSize(size);
	QObject::connect(m_minimizeButton, SIGNAL(clicked()),
		this, SLOT(iconify()));
	m_buttonList.insert(m_buttonList.end(), m_minimizeButton);

	m_maximizeButton=factory->createGlowButton(
		this, "MaximizeButton", "Maximize");
	m_maximizeButton->setFixedSize(size);
	QObject::connect(m_maximizeButton, SIGNAL(clicked(int)),
		this, SLOT(slotMaximize(int)));
	m_buttonList.insert(m_buttonList.end(), m_maximizeButton);

	m_closeButton = factory->createGlowButton(
		this, "CloseButton", "Close");
	m_closeButton->setFixedSize(size);
	QObject::connect(m_closeButton, SIGNAL(clicked()),
		this, SLOT(closeWindow()));
	m_buttonList.insert(m_buttonList.end(), m_closeButton);
}

void GlowClient::resetLayout()
{
// cerr << "GlowClient " << "resetLayout " << endl;
	const unsigned int sideMargin = 2;
	const unsigned int bottomMargin = 2;
	const unsigned int titleVMargin = 2;

	QFontMetrics fm(KWinInternal::options->font(isActive(),isTool()));
	unsigned int titleHeight = QMAX(15, fm.height()+2*titleVMargin);
	if( titleHeight%2 != 0 )
		titleHeight+=1;

	if( m_mainLayout )
		delete m_mainLayout;
	m_mainLayout = new QVBoxLayout(this, 0, 0);

	// update button positions and colors
	updateButtonPositions();
	updateButtonPixmaps();

	QHBoxLayout *topLayout = new QHBoxLayout(m_mainLayout, 1);
	topLayout->addSpacing(sideMargin);
	for( unsigned int i=0; i<m_leftButtonList.size(); i++ )
		topLayout->addWidget(m_leftButtonList[i], Qt::AlignVCenter);
	topLayout->addSpacing(sideMargin);

	m_titleSpacer = new QSpacerItem(0, titleHeight,
		QSizePolicy::Expanding, QSizePolicy::Fixed);
	topLayout->addItem(m_titleSpacer);

	topLayout->addSpacing(sideMargin);
	for( unsigned int i=0; i<m_rightButtonList.size(); i++ )
		topLayout->addWidget(m_rightButtonList[i], Qt::AlignVCenter);
	topLayout->addSpacing(sideMargin);

	QHBoxLayout *midLayout = new QHBoxLayout(m_mainLayout, 0);
	midLayout->addSpacing(sideMargin);
	midLayout->addWidget(windowWrapper());
	midLayout->addSpacing(sideMargin);

	m_mainLayout->addSpacing(bottomMargin);
	m_mainLayout->setStretchFactor(topLayout, 0);
	m_mainLayout->setStretchFactor(midLayout, 1);
}

void GlowClient::updateButtonPositions()
{
// cerr << "GlowClient " << "updateButtonPositions " << endl;
	QString buttons = options->titleButtonsLeft() + "|"
		+ options->titleButtonsRight();
	std::vector<GlowButton*> *buttonList = &m_leftButtonList;

	// hide all buttons
	for( unsigned int i=0; i<m_buttonList.size(); i++ )
		m_buttonList[i]->hide();

	m_leftButtonList.clear();
	m_rightButtonList.clear();

	for( unsigned int i=0; i<buttons.length(); i++ )
	{
		char c = buttons[i].latin1();
		GlowButton *button = 0;
		if( c=='S' ) // sticky
			button = m_stickyButton;
		else if( c=='H' && providesContextHelp() ) // help
			button = m_helpButton;
		else if( c=='I' && isMinimizable() ) // iconify
			button = m_minimizeButton;
		else if( c=='A' && isMaximizable() ) // maximize
			button = m_maximizeButton;
		else if( c=='X' ) // close
			button= m_closeButton;
		else if( c=='|' )
			buttonList = &m_rightButtonList;

		if(button)
		{
			button->show(); // show visible buttons
			buttonList->insert(buttonList->end(), button);
		}
	}
}

void GlowClient::updateButtonPixmaps()
{
// cerr << "GlowClient " << "updateButtonPixmaps " << endl;
	GlowClientGlobals *globals = GlowClientGlobals::instance();

	if( isSticky() )
		m_stickyButton->setPixmap(globals->getPixmap(
			GlowClientGlobals::StickyOn, isActive(),
			isLeft(m_stickyButton), isTool()));
	else
		m_stickyButton->setPixmap(globals->getPixmap(
			GlowClientGlobals::StickyOff, isActive(),
			isLeft(m_stickyButton), isTool()));

	m_helpButton->setPixmap(globals->getPixmap(
		GlowClientGlobals::Help, isActive(),
		isLeft(m_helpButton), isTool()));

	m_minimizeButton->setPixmap(globals->getPixmap(
		GlowClientGlobals::Iconify, isActive(),
		isLeft(m_minimizeButton), isTool()));

	if( isMaximized() )
		m_maximizeButton->setPixmap(globals->getPixmap(
			GlowClientGlobals::MaximizeOn, isActive(),
			isLeft(m_maximizeButton), isTool()));
	else
		m_maximizeButton->setPixmap(globals->getPixmap(
			GlowClientGlobals::MaximizeOff, isActive(),
			isLeft(m_maximizeButton), isTool()));

	m_closeButton->setPixmap(globals->getPixmap(
		GlowClientGlobals::Close, isActive(),
		isLeft(m_closeButton), isTool()));
}

void GlowClient::doShape()
{
    QRegion mask(QRect(0, 0, width(), height()));
    mask -= QRect(0, 0, 1, 1);
    mask -= QRect(width()-1, 0, 1, 1);
    mask -= QRect(0, height()-1, 1, 1);
    mask -= QRect(width()-1, height()-1, 1, 1);
    setMask(mask);
}

bool GlowClient::isLeft(GlowButton *button)
{
	for( unsigned int i=0; i<m_leftButtonList.size(); i++ )
		if( m_leftButtonList[i] == button )
			return true;
	return false;
}

bool GlowClient::isRight(GlowButton *button)
{
	for( unsigned int i=0; i<m_rightButtonList.size(); i++ )
		if( m_rightButtonList[i] == button )
			return true;
	return false;
}

void GlowClient::slotReset()
{
	resetLayout();
	repaint(false);
}

void GlowClient::slotMaximize(int button)
{
	if(button == QMouseEvent::RightButton)
		maximize(MaximizeHorizontal);
	else if(button == QMouseEvent::MidButton)
		maximize(MaximizeVertical);
	else // if(button == QMouseEvent::LeftButton)
		maximize(MaximizeFull);
}

extern "C"
{
	Client * allocate(Workspace * ws, WId w)
	{
// cerr << "#### allocate " << endl;
		return new GlowClient(ws, w);
	}

	void init()
	{
// cerr << "#### init " << endl;
		GlowClientGlobals::instance();
	}

	void reset()
	{
// cerr << "#### reset " << endl;
		GlowClientGlobals::instance()->reset();
		Workspace::self()->slotResetAllClientsDelayed();
	}

	void deinit()
	{
// cerr << "#### deinit " << endl;
		delete GlowClientGlobals::instance();
	}
}

#include "glowclient.moc"
