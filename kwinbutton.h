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

#ifndef _KWIN_BUTTON_H
#define _KWIN_BUTTON_H

#include <qlabel.h>
#include <qtimer.h>
#include <qtoolbutton.h>

namespace KWinInternal {


class KWinToolTip: public QLabel
{
	Q_OBJECT

	public:
		KWinToolTip( QWidget* parent, const QString& tip );
		~KWinToolTip();
		void setTipText( const QString& tip );
		void enterTip();
		void leaveTip();
	private slots:
		void showTip();
		void hideTip();
	private:
		void positionTip();
		QTimer   showTimer;
		QTimer   hideTimer;
		QWidget* btn;
};


class KWinButton: public QButton
{
	Q_OBJECT

	public:
		KWinButton( QWidget *parent, const char *name, 
					const QString& tip );
		~KWinButton(); 
		void setTipText(const QString& newTip);
	protected:
		/* Ensure you call these event handlers from derived classes */
		void enterEvent(QEvent *);
		void leaveEvent(QEvent *);
		void mousePressEvent( QMouseEvent* e );
	private:
		KWinToolTip* buttonTip; 
};


class KWinToolButton: public QToolButton
{
	Q_OBJECT

	public:
		KWinToolButton( QWidget *parent, const char *name, 
						const QString& tip );
		~KWinToolButton(); 
		void setTipText(const QString& newTip);
	protected:
		/* Ensure you call these event handlers from derived classes */
		void enterEvent(QEvent *);
		void leaveEvent(QEvent *);
		void mousePressEvent( QMouseEvent* e );
	private:
		KWinToolTip* buttonTip; 
};


class KWinWidgetButton: public QWidget
{
	Q_OBJECT

	public:
		KWinWidgetButton( QWidget *parent, const char *name, 
						  WFlags f, const QString& tip );
		~KWinWidgetButton(); 
		void setTipText(const QString& newTip);
	protected:
		/* Ensure you call these event handlers from derived classes */
		void enterEvent(QEvent *);
		void leaveEvent(QEvent *);
		void mousePressEvent( QMouseEvent* e );
	private:
		KWinToolTip* buttonTip; 
};


}

#endif
// vim: ts=4
