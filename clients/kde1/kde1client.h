/*****************************************************************
kwin - the KDE window manager
								
Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
#ifndef STDCLIENT_H
#define STDCLIENT_H
#include "../../client.h"
#include "../../kwinbutton.h"
class QLabel;
class QSpacerItem;

namespace KWinInternal {

class StdClient : public KWinInternal::Client
{
    Q_OBJECT
public:
    StdClient( Workspace *ws, WId w, QWidget *parent=0, const char *name=0 );
    ~StdClient();
protected:
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );

    void mouseDoubleClickEvent( QMouseEvent * );
    void init();
    void captionChange( const QString& name );
    void iconChange();
    void maximizeChange( bool );
    void stickyChange( bool );
    void activeChange( bool );

private slots:
    void menuButtonPressed();
    void maxButtonClicked( int );
    void slotReset();

private:
    KWinToolButton* button[7];
    QSpacerItem* titlebar;
};

class StdToolClient : public Client
{
    Q_OBJECT
public:
    StdToolClient( Workspace *ws, WId w, QWidget *parent=0, const char *name=0 );
    ~StdToolClient();
protected:
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );

    void mouseDoubleClickEvent( QMouseEvent * );
    void init();
    void captionChange( const QString& name );
    void activeChange( bool );

private slots:
    void slotReset();
private:
    KWinToolButton* closeBtn;
    QSpacerItem* titlebar;
};



/*
  Like QToolButton, but provides a clicked(int) signals that
  has the last pressed mouse button as argument
 */
class ThreeButtonButton: public KWinInternal:: KWinToolButton
{
    Q_OBJECT
public:
  ThreeButtonButton ( QWidget *parent = 0, const char* name = 0, 
		      const QString& tip = 0 )
      : KWinToolButton( parent, name, tip )
    {
	connect( this, SIGNAL( clicked() ), this, SLOT( handleClicked() ) );
    }
    ~ThreeButtonButton () {}

signals:
    void clicked( int );

protected:
    void mousePressEvent( QMouseEvent* e )
    {
	last_button = e->button();
	QMouseEvent me ( e->type(), e->pos(), e->globalPos(), LeftButton, e->state() );
	KWinToolButton::mousePressEvent( &me );
    }

    void mouseReleaseEvent( QMouseEvent* e )
    {
	QMouseEvent me ( e->type(), e->pos(), e->globalPos(), LeftButton, e->state() );
	KWinToolButton::mouseReleaseEvent( &me );
    }

private slots:
    void handleClicked()
    {
	emit clicked( last_button );
    }

private:
    int last_button;

};

};

#endif
