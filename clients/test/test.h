#ifndef KWIN_TEST
#define KWIN_TEST

#include <kdecoration.h>
#include <kdecorationfactory.h>
#include <qpushbutton.h>

namespace KWinTest
{

class Decoration
    : public KDecoration
    {
    Q_OBJECT
    public:
	Decoration( KDecorationBridge* bridge, KDecorationFactory* factory );
        virtual void init();
	virtual MousePosition mousePosition( const QPoint& p ) const;
	virtual void borders( int& left, int& right, int& top, int& bottom ) const;
	virtual void resize( const QSize& s );
	virtual QSize minimumSize() const;
        virtual void activeChange() {};
        virtual void captionChange() {};
        virtual void maximizeChange() {};
        virtual void desktopChange() {};
        virtual void shadeChange() {};
        virtual void iconChange() {};
	virtual bool eventFilter( QObject* o, QEvent* e );
        virtual void reset( unsigned long changed );
        virtual bool animateMinimize( bool minimize );
    private:
	QPushButton* button;
    };

class Factory
    : public KDecorationFactory
    {
    public:
        virtual KDecoration* createDecoration( KDecorationBridge* );
        virtual bool reset( unsigned long changed );
    };
    
} // namespace

#endif
