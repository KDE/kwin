#ifndef BECLIENT_H
#define BECLIENT_H
#include "client.h"
#include <qpalette.h>
class QToolButton;
class QLabel;
class QSpacerItem;


class BeClient : public Client
{
    Q_OBJECT
public:
    BeClient( Workspace *ws, WId w, QWidget *parent=0, const char *name=0 );
    ~BeClient();

protected:
    QColorGroup makeColorGroup(const QColor &bg, const QColor &fg=Qt::white);
    void resizeEvent( QResizeEvent* );
    void paintEvent( QPaintEvent* );
    void mousePressEvent( QMouseEvent * );
    void mouseReleaseEvent( QMouseEvent * );
    void mouseDoubleClickEvent( QMouseEvent * e );

    void captionChange( const QString& name );

    void showEvent( QShowEvent* );
    void activeChange( bool );

    MousePosition mousePosition( const QPoint& p ) const;

private:
    QSpacerItem* titlebar;
    void doShape();
    QColorGroup aFrameGrp, iFrameGrp, aTitleGrp, iTitleGrp;
};



#endif
