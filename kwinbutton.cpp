/*
 *	$Id$
 *
 *	KWin client button tooltip support module
 *
 *  Copyright (c) 2001
 *	Karol Szwed <gallium@kde.org>
 * 
 *  Uses portions of QToolTip code 
 *  Copyright (c) TrollTech AS
 *
 *  Since Qt installs its own internal event filter when using
 *  a QToolTip, kwin doesn't obtain the necessary events and hence
 *  focus problems and loss of window frames occur if they are used
 *  for kwin styles. Thus, this small class implements a bare subset
 *  of QToolTip designed especially for kwin client buttons without 
 *  installing any event filters that confuse kwin.
 * 
 *  For all normal clients, simply use the KWinButton class instead
 *  of QButton, which will use a KWinToolTip internally. Other clients
 *  which rely on QToolButton or QWidget for client buttons may use the 
 *  KWinToolButton and KWinWidgetButton classes to automatically take
 *  care of tooltips.
 *
 */

#include <qpoint.h>
#include "kwinbutton.h"
#include "options.h"
#include "workspace.h"

// This is a hack to ensure that we use Qt fade and scroll effects

// This code is copied directly from qeffects_p.h
struct QEffects
{
    enum Direction {
        LeftScroll      = 0x0001,
        RightScroll     = 0x0002,
        UpScroll        = 0x0004,
        DownScroll      = 0x0008
    };

    typedef uint DirFlags;
};

extern void Q_EXPORT qScrollEffect( QWidget*, 
	QEffects::DirFlags dir = QEffects::DownScroll, int time = -1 );
extern void Q_EXPORT qFadeEffect( QWidget*, int time = -1 );
// end qeffects_p.h code

using namespace KWinInternal;


KWinToolTip::KWinToolTip( QWidget* parent, const QString& tip )
	: QLabel( NULL, "KWinToolTip", WStyle_StaysOnTop + 
			  WStyle_Customize + WStyle_NoBorder + WStyle_Tool )
{
	setMargin(1);
	setIndent(0);
	setFrameStyle( QFrame::Plain | QFrame::Box );
	setLineWidth(1);
	setAlignment( AlignLeft | AlignTop );
	setText(tip);
	adjustSize();

	// Use stock Qt tooltip colors as defined in qapplication.cpp
	QColorGroup clrGroup( Qt::black, QColor(255,255,220),
					QColor(96,96,96), Qt::black, Qt::black,
					Qt::black, QColor(255,255,220) );
	setPalette( QPalette( clrGroup, clrGroup, clrGroup ) );
           
	btn = parent;
	connect(&hideTimer, SIGNAL(timeout()), SLOT(hideTip()));
	connect(&showTimer, SIGNAL(timeout()), SLOT(showTip()));
}

KWinToolTip::~KWinToolTip()
{
}

void KWinToolTip::setTipText( const QString& tip )
{
	bool visible = isVisible();

	if (visible) 
		hide();

	setText(tip);
	adjustSize();
	positionTip();

	if (visible) 
		showTip();
}

void KWinToolTip::enterTip()
{
	// Show after 1 second
	showTimer.start( 1000, TRUE );
}

void KWinToolTip::leaveTip()
{
	if (hideTimer.isActive())
		hideTimer.stop();
	if (showTimer.isActive())
		showTimer.stop();
	if (isVisible())
		hide();
}

/* Internal */
void KWinToolTip::showTip()
{
	if (isVisible())
		return;

	// Ignore empty tooltips
	if (text().isEmpty())
		return;

	positionTip();

	// Show tooltips the Qt way
	if (options->fadeTooltips())
		qFadeEffect(this); 
	else if (options->animateTooltips()) 
		qScrollEffect(this);
	else
		show();
	raise();

	// Hide after 10 seconds
	hideTimer.start( 10000, TRUE );
}

/* Internal */
void KWinToolTip::hideTip()
{
	if (isVisible())
		hide();
}

/* Internal */
void KWinToolTip::positionTip()
{
	QPoint p = btn->mapToGlobal(btn->rect().bottomLeft()) + QPoint(0, 16);

	// Ensure the tooltip is displayed within the current desktop
	if ( p.x() + width() > Workspace::self()->desktopWidget()->width() )
		p.setX( Workspace::self()->desktopWidget()->width() - width());
	if ( p.y() + height() > Workspace::self()->desktopWidget()->height() )
		p.setY( Workspace::self()->desktopWidget()->height() - height() );
	if (p.x() < 0) p.setX(0);
	if (p.y() < 0) p.setY(0); 
	move(p); 

	// Ensure we don't get enter/leave event race conditions when a
	// tooltip is over a button.
	QRect btnGlobalRect( btn->mapToGlobal(btn->rect().topLeft()),
						 btn->mapToGlobal(btn->rect().bottomRight()) );
	QRect tipGlobalRect( mapToGlobal(rect().topLeft()),
						 mapToGlobal(rect().bottomRight()) );
	if (btnGlobalRect.intersects( tipGlobalRect ))
	{
		// Only intersects when button is at lower part of screen
		p.setY( btn->mapToGlobal(btn->rect().topLeft()).y() - height() - 5 );
		move(p);
	}
}


// KWinButton class
KWinButton::KWinButton(QWidget *parent, const char *name, const QString& tip)
    : QButton(parent, name, WStyle_Customize | WRepaintNoErase |
                            WResizeNoErase   | WStyle_NoBorder )
{
	buttonTip = options->showTooltips() ? new KWinToolTip(this, tip) : NULL;
}

KWinButton::~KWinButton()
{
    if (buttonTip)
        delete buttonTip;
}

void KWinButton::setTipText(const QString& newTip)
{
	if (buttonTip)
		buttonTip->setTipText( newTip );
}

void KWinButton::enterEvent(QEvent *e)
{
    if (buttonTip)
        buttonTip->enterTip();
	QButton::enterEvent( e );
}

void KWinButton::leaveEvent(QEvent *e)
{
    if (buttonTip)
        buttonTip->leaveTip();
	QButton::leaveEvent( e );
}

void KWinButton::mousePressEvent(QMouseEvent* e)
{
    // Hide tooltips if a button is pressed
    if (buttonTip)
        buttonTip->leaveTip();
	QButton::mousePressEvent( e );
}


// KWinToolButton class
KWinToolButton::KWinToolButton(QWidget *parent, const char *name, const QString& tip)
    : QToolButton(parent, name)
{
	buttonTip = options->showTooltips() ? new KWinToolTip(this, tip) : NULL;
}

KWinToolButton::~KWinToolButton()
{
    if (buttonTip)
        delete buttonTip;
}

void KWinToolButton::setTipText(const QString& newTip)
{
	if (buttonTip)
		buttonTip->setTipText( newTip );
}

void KWinToolButton::enterEvent(QEvent *e)
{
    if (buttonTip)
        buttonTip->enterTip();
	QToolButton::enterEvent( e );
}

void KWinToolButton::leaveEvent(QEvent *e)
{
    if (buttonTip)
        buttonTip->leaveTip();
	QToolButton::leaveEvent( e );
}

void KWinToolButton::mousePressEvent(QMouseEvent* e)
{
    // Hide tooltips if a button is pressed
    if (buttonTip)
        buttonTip->leaveTip();
	QToolButton::mousePressEvent( e );
}


// KWinWidgetButton class
KWinWidgetButton::KWinWidgetButton(QWidget *parent, const char *name, 
	  WFlags f, const QString& tip)
    : QWidget(parent, name, f)
{
	buttonTip = options->showTooltips() ? new KWinToolTip(this, tip) : NULL;
}

KWinWidgetButton::~KWinWidgetButton()
{
    if (buttonTip)
        delete buttonTip;
}

void KWinWidgetButton::setTipText(const QString& newTip)
{
	if (buttonTip)
		buttonTip->setTipText( newTip );
}

void KWinWidgetButton::enterEvent(QEvent *e)
{
    if (buttonTip)
        buttonTip->enterTip();
	QWidget::enterEvent( e );
}

void KWinWidgetButton::leaveEvent(QEvent *e)
{
    if (buttonTip)
        buttonTip->leaveTip();
	QWidget::leaveEvent( e );
}

void KWinWidgetButton::mousePressEvent(QMouseEvent* e)
{
    // Hide tooltips if a button is pressed
    if (buttonTip)
        buttonTip->leaveTip();
	QWidget::mousePressEvent( e );
}


#include "kwinbutton.moc"
// vim: ts=4
