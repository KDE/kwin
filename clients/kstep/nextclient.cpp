#include "nextclient.h"
#include <qabstractlayout.h>
#include <qdatetime.h>
#include <qdrawutil.h>
#include <qlayout.h>
#include <qpainter.h>
#include <qbitmap.h>
#include <kdebug.h>
#include <klocale.h>
#include <kpixmapeffect.h>
#include "../../workspace.h"
#include "../../options.h"

using namespace KWinInternal;



static unsigned char close_bits[] = {
  0x03, 0x03, 0x87, 0x03, 0xce, 0x01, 0xfc, 0x00, 0x78, 0x00, 0x78, 0x00,
  0xfc, 0x00, 0xce, 0x01, 0x87, 0x03, 0x03, 0x03};

static unsigned char iconify_bits[] = {
  0xff, 0x03, 0xff, 0x03, 0xff, 0x03, 0xff, 0x03, 0x03, 0x03, 0x03, 0x03,
  0x03, 0x03, 0x03, 0x03, 0xff, 0x03, 0xff, 0x03};

static unsigned char question_bits[] = {
  0x00, 0x00, 0x78, 0x00, 0xcc, 0x00, 0xc0, 0x00, 0x60, 0x00, 0x30, 0x00,
  0x00, 0x00, 0x30, 0x00, 0x30, 0x00, 0x00, 0x00};

static unsigned char sticky_bits[] = {
  0x00, 0x00, 0x30, 0x00, 0x30, 0x00, 0x30, 0x00, 0xfe, 0x01, 0xfe, 0x01,
  0x30, 0x00, 0x30, 0x00, 0x30, 0x00, 0x00, 0x00};

static unsigned char unsticky_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x01, 0xfe, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static unsigned char maximize_bits[] = {
   0x30, 0x00, 0x78, 0x00, 0xfc, 0x00, 0xfe, 0x01, 0x00, 0x00, 0xfe, 0x01,
   0x02, 0x01, 0x84, 0x00, 0x48, 0x00, 0x30, 0x00 };

// If the maximize graphic above (which I did quickly in about a
// minute, just so I could have something) doesn't please, maybe one
// of the following would be better.  IMO it doesn't matter, as long
// as it's not offensive---people will get used to whatever you use.
// True NeXT fans won't turn on the maximize button anyway.
//
// static unsigned char maximize_bits[] = {
//    0xcf, 0x03, 0x87, 0x03, 0xcf, 0x03, 0xfd, 0x02, 0x48, 0x00, 0x48, 0x00,
//    0xfd, 0x02, 0xcf, 0x03, 0x87, 0x03, 0xcf, 0x03 };
//
// static unsigned char maximize_bits[] = {
//    0xcf, 0x03, 0x87, 0x03, 0x87, 0x03, 0x79, 0x02, 0x48, 0x00, 0x48, 0x00,
//    0x79, 0x02, 0x87, 0x03, 0x87, 0x03, 0xcf, 0x03 };
//
// static unsigned char maximize_bits[] = {
//    0x87, 0x03, 0x03, 0x03, 0xfd, 0x02, 0x84, 0x00, 0x84, 0x00, 0x84, 0x00,
//    0x84, 0x00, 0xfd, 0x02, 0x03, 0x03, 0x87, 0x03 };
//
// static unsigned char maximize_bits[] = {
//    0x30, 0x00, 0x78, 0x00, 0xcc, 0x00, 0x86, 0x01, 0x33, 0x03, 0x79, 0x02,
//    0xcd, 0x02, 0x87, 0x03, 0x03, 0x03, 0x01, 0x02 };
//
// static unsigned char maximize_bits[] = {
//    0x30, 0x00, 0x78, 0x00, 0x78, 0x00, 0xfc, 0x00, 0xfc, 0x00, 0xfe, 0x01,
//    0xfe, 0x01, 0xff, 0x03, 0xff, 0x03, 0xff, 0x03 };


static KPixmap *aTitlePix=0;
static KPixmap *iTitlePix=0;
static KPixmap *aFramePix=0;
static KPixmap *iFramePix=0;
static KPixmap *aHandlePix=0;
static KPixmap *iHandlePix=0;
static KPixmap *aBtn=0;
static KPixmap *aBtnDown=0;
static KPixmap *iBtn=0;
static KPixmap *iBtnDown=0;
static bool pixmaps_created = false;
static QColor *btnForeground;

static void create_pixmaps()
{
    if(pixmaps_created)
        return;
    pixmaps_created = true;

    aTitlePix = new KPixmap();
    aTitlePix->resize(32, 14);
    KPixmapEffect::gradient(*aTitlePix,
                            options->color(Options::TitleBar, true),
                            options->color(Options::TitleBlend, true),
                            KPixmapEffect::VerticalGradient);
    iTitlePix = new KPixmap();
    iTitlePix->resize(32, 14);
    KPixmapEffect::gradient(*iTitlePix,
                            options->color(Options::TitleBar, false),
                            options->color(Options::TitleBlend, false),
                            KPixmapEffect::VerticalGradient);
    // Bottom frame gradient
    aFramePix = new KPixmap();
    aFramePix->resize(32, 6);
    KPixmapEffect::gradient(*aFramePix,
                            options->color(Options::Frame, true).light(150),
                            options->color(Options::Frame, true).dark(120),
                            KPixmapEffect::VerticalGradient);
    iFramePix = new KPixmap();
    iFramePix->resize(32, 6);
    KPixmapEffect::gradient(*iFramePix,
                            options->color(Options::Frame, false).light(150),
                            options->color(Options::Frame, false).dark(120),
                            KPixmapEffect::VerticalGradient);

    // Handle gradient
    aHandlePix = new KPixmap();
    aHandlePix->resize(32, 6);
    KPixmapEffect::gradient(*aHandlePix,
                            options->color(Options::Handle, true).light(150),
                            options->color(Options::Handle, true).dark(120),
                            KPixmapEffect::VerticalGradient);
    iHandlePix = new KPixmap();
    iHandlePix->resize(32, 6);
    KPixmapEffect::gradient(*iHandlePix,
                            options->color(Options::Handle, false).light(150),
                            options->color(Options::Handle, false).dark(120),
                            KPixmapEffect::VerticalGradient);

    iBtn = new KPixmap;
    iBtn->resize(18, 18);
    iBtnDown = new KPixmap;
    iBtnDown->resize(18, 18);
    aBtn = new KPixmap;
    aBtn->resize(18, 18);
    aBtnDown = new KPixmap;
    aBtnDown->resize(18, 18);
    KPixmap internal;
    internal.resize(12, 12);

    // inactive buttons
    QColor c(options->color(Options::ButtonBg, false));
    KPixmapEffect::gradient(*iBtn, c.light(120), c.dark(120),
                            KPixmapEffect::DiagonalGradient);
    KPixmapEffect::gradient(internal, c.dark(120), c.light(120),
                            KPixmapEffect::DiagonalGradient);
    bitBlt(iBtn, 3, 3, &internal, 0, 0, 12, 12, Qt::CopyROP, true);

    KPixmapEffect::gradient(*iBtnDown, c.dark(120), c.light(120),
                            KPixmapEffect::DiagonalGradient);
    KPixmapEffect::gradient(internal, c.light(120), c.dark(120),
                            KPixmapEffect::DiagonalGradient);
    bitBlt(iBtnDown, 3, 3, &internal, 0, 0, 12, 12, Qt::CopyROP, true);

    // active buttons
    c = options->color(Options::ButtonBg, true);
    KPixmapEffect::gradient(*aBtn, c.light(120), c.dark(120),
                            KPixmapEffect::DiagonalGradient);
    KPixmapEffect::gradient(internal, c.dark(120), c.light(120),
                            KPixmapEffect::DiagonalGradient);
    bitBlt(aBtn, 3, 3, &internal, 0, 0, 12, 12, Qt::CopyROP, true);

    KPixmapEffect::gradient(*aBtnDown, c.dark(120), c.light(120),
                            KPixmapEffect::DiagonalGradient);
    KPixmapEffect::gradient(internal, c.light(120), c.dark(120),
                            KPixmapEffect::DiagonalGradient);
    bitBlt(aBtnDown, 3, 3, &internal, 0, 0, 12, 12, Qt::CopyROP, true);

    QPainter p;
    p.begin(aBtn);
    p.setPen(Qt::black);
    p.drawRect(0, 0, 18, 18);
    p.end();
    p.begin(iBtn);
    p.setPen(Qt::black);
    p.drawRect(0, 0, 18, 18);
    p.end();
    p.begin(aBtnDown);
    p.setPen(Qt::black);
    p.drawRect(0, 0, 18, 18);
    p.end();
    p.begin(iBtnDown);
    p.setPen(Qt::black);
    p.drawRect(0, 0, 18, 18);
    p.end();

    if(qGray(options->color(Options::ButtonBg, true).rgb()) > 128)
        btnForeground = new QColor(Qt::black);
    else
        btnForeground = new QColor(Qt::white);
}

static void delete_pixmaps()
{
    delete aTitlePix;
    delete iTitlePix;
    delete aFramePix;
    delete iFramePix;
    delete aHandlePix;
    delete iHandlePix;
    delete aBtn;
    delete iBtn;
    delete aBtnDown;
    delete iBtnDown;
    delete btnForeground;

    pixmaps_created = false;
}

NextButton::NextButton(Client *parent, const char *name,
                       const unsigned char *bitmap, int bw, int bh,
                       const QString& tip)
    : KWinButton(parent, name, tip),
      deco(NULL), client(parent), last_button(NoButton)
{
    setBackgroundMode( NoBackground );
    resize(18, 18);

    if(bitmap)
        setBitmap(bitmap, bw, bh);
}

void NextButton::reset()
{
    repaint(false);
}

void NextButton::setBitmap(const unsigned char *bitmap, int w, int h)
{
    deco = new QBitmap(w, h, bitmap, true);
    deco->setMask(*deco);
    repaint();
}

void NextButton::drawButton(QPainter *p)
{
    if(client->isActive())
        p->drawPixmap(0, 0, isDown() ? *aBtnDown : *aBtn);
    else
        p->drawPixmap(0, 0, isDown() ? *iBtnDown : *iBtn);

    // If we have a decoration, draw it; otherwise, we have the menu
    // button (remember, we set the bitmap to NULL).
    if (deco) {
        p->setPen(*btnForeground);
        p->drawPixmap(isDown()? 5 : 4, isDown() ? 5 : 4, *deco);
    } else {
        KPixmap btnpix = client->miniIcon();
        p->drawPixmap( 0, 0, btnpix );
    }
}

void NextButton::mousePressEvent( QMouseEvent* e )
{
    last_button = e->button();
    QMouseEvent me( e->type(), e->pos(), e->globalPos(),
                    LeftButton, e->state() );
    KWinButton::mousePressEvent( &me );
}

void NextButton::mouseReleaseEvent( QMouseEvent* e )
{
    last_button = e->button();
    QMouseEvent me( e->type(), e->pos(), e->globalPos(),
                    LeftButton, e->state() );
    KWinButton::mouseReleaseEvent( &me );
}


NextClient::NextClient( Workspace *ws, WId w, QWidget *parent,
                            const char *name )
    : Client( ws, w, parent, name, WResizeNoErase )
{
    setBackgroundMode( NoBackground );
    connect(options, SIGNAL(resetClients()), this, SLOT(slotReset()));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QHBoxLayout *titleLayout = new QHBoxLayout();
    QHBoxLayout *windowLayout = new QHBoxLayout();

    mainLayout->addLayout(titleLayout);
    mainLayout->addLayout(windowLayout, 1);
    mainLayout->addSpacing(6);

    windowLayout->addSpacing(1);
    windowLayout->addWidget(windowWrapper(), 1);
    windowLayout->addSpacing(1);

    initializeButtonsAndTitlebar(titleLayout);
}

/**
   Preconditions:
       + this->button is an array of length MAX_NUM_BUTTONS

   Postconditions:
       + Title bar and buttons have been initialized and laid out
       + for all i in 0..(MAX_NUM_BUTTONS-1), button[i] points to
         either (1) a valid NextButton instance, if the corresponding
         button is selected in the current button scheme, or (2) null
         otherwise.
 */
void NextClient::initializeButtonsAndTitlebar(QHBoxLayout* titleLayout)
{
    // Null the buttons to begin with (they are not guaranteed to be null).
    for (int i=0; i<MAX_NUM_BUTTONS; i++) {
        button[i] = NULL;
    }

    // The default button positions for other styles do not match the
    // behavior of older versions of KStep, so we have to set these
    // manually when customButtonPositions isn't enabled.
    QString left, right;
    if (options->customButtonPositions()) {
        left = options->titleButtonsLeft();
        right = options->titleButtonsRight();
    } else {
        left = QString("I");
        right = QString("SX");
    }

    // Do actual creation and addition to titleLayout
    addButtons(titleLayout, left);
    titlebar = new QSpacerItem(10, 16, QSizePolicy::Expanding,
                               QSizePolicy::Minimum );
    titleLayout->addItem(titlebar);
    addButtons(titleLayout, right);

    // Finally, activate all live buttons
    for ( int i = 0; i < MAX_NUM_BUTTONS; i++) {
        if (button[i]) {
            button[i]->setMouseTracking( TRUE );
            button[i]->setFixedSize( 18, 18 );
        }
    }
}

/** Adds the buttons for one side of the title bar, based on the spec
 * string; see the KWinInternal::Options class, methods
 * titleButtonsLeft and titleBUttonsRight. */
void NextClient::addButtons(QHBoxLayout* titleLayout, const QString& spec)
{
    int str_len = spec.length();
    if (str_len <= 0)
	return;

    for (unsigned int i=0; i<spec.length(); i++) {
        //switch (spec[i].latin1()) {
	switch( (QApplication::reverseLayout() && (!options->reverseBIDIWindows()))?
		spec[str_len-i-1].latin1():spec[i].latin1()){
        case 'A':
            if (isMaximizable()) {
                button[MAXIMIZE_IDX] =
                    new NextButton(this, "maximize", maximize_bits, 10, 10,
                                   i18n("Maximize"));
                titleLayout->addWidget( button[MAXIMIZE_IDX] );
                connect( button[MAXIMIZE_IDX], SIGNAL(clicked()),
                         this, SLOT(maximizeButtonClicked()) );
            }
            break;

        case 'H':
            button[HELP_IDX] =
                new NextButton(this, "help", question_bits, 10, 10,
                               i18n("Help"));
            titleLayout->addWidget( button[HELP_IDX] );
            connect( button[HELP_IDX], SIGNAL(clicked()),
                     this, SLOT(contextHelp()) );
            break;

        case 'I':
            if (isMinimizable()) {
                button[ICONIFY_IDX] =
                    new NextButton(this, "iconify", iconify_bits, 10, 10,
                                   i18n("Minimize"));
                titleLayout->addWidget( button[ICONIFY_IDX] );
                connect( button[ICONIFY_IDX], SIGNAL(clicked()),
                         this, SLOT(iconify()) );
            }
            break;

        case 'M':
            button[MENU_IDX] =
                new NextButton(this, "menu", NULL, 10, 10, i18n("Menu"));
            titleLayout->addWidget( button[MENU_IDX] );
            // NOTE DIFFERENCE: capture pressed(), not clicked()
            connect( button[MENU_IDX], SIGNAL(pressed()),
                     this, SLOT(menuButtonPressed()) );
            break;

        case 'S':
            button[STICKY_IDX] =
                new NextButton(this, "sticky", NULL, 0, 0, i18n("Sticky"));
            titleLayout->addWidget( button[STICKY_IDX] );
            connect( button[STICKY_IDX], SIGNAL(clicked()),
                     this, SLOT(toggleSticky()) );
            // NOTE DIFFERENCE: set the pixmap separately (2 states)
            stickyChange(isSticky());
            break;

        case 'X':
            button[CLOSE_IDX] =
                new NextButton(this, "close", close_bits, 10, 10,
                               i18n("Close"));
            titleLayout->addWidget( button[CLOSE_IDX] );
            connect( button[CLOSE_IDX], SIGNAL(clicked()),
                     this, SLOT(closeWindow()) );
            break;

        case '_':
            // TODO: Add spacer handling
            break;

        default:
            kdDebug() << " Can't happen: unknown button code "
                      << QString(spec[i]);
            break;
        }
    }
}

// Make sure the menu button follows double click conventions set in kcontrol
// (Note: this was almost straight copy and paste from KDEDefaultClient.)
void NextClient::menuButtonPressed()
{
    // Probably don't need this null check, but we might as well.
    if (button[MENU_IDX]) {
        QPoint menupoint ( button[MENU_IDX]->rect().bottomLeft().x()-1,
                           button[MENU_IDX]->rect().bottomLeft().y()+2 );
        QPoint pos = button[MENU_IDX]->mapToGlobal( menupoint );
        workspace()->showWindowMenu( pos.x(), pos.y(), this );
    }
}

// Copied, with minor edits, from KDEDefaultClient::slotMaximize()
void NextClient::maximizeButtonClicked()
{
    if (button[MAXIMIZE_IDX]) {
        switch (button[MAXIMIZE_IDX]->lastButton()) {
        case MidButton:
            maximize( MaximizeVertical );
            break;
        case RightButton:
            maximize( MaximizeHorizontal );
            break;
        default:
            maximize();
            break;
        }
    }
}

void NextClient::resizeEvent( QResizeEvent* e)
{
    Client::resizeEvent( e );

    if ( isVisibleToTLW() && !testWFlags( WStaticContents )) {
        QPainter p( this );
	QRect t = titlebar->geometry();
	t.setTop( 0 );
	QRegion r = rect();
	r = r.subtract( t );
	p.setClipRegion( r );
	p.eraseRect( rect() );
    }
}

void NextClient::captionChange( const QString& )
{
    repaint( titlebar->geometry(), false );
}


void NextClient::paintEvent( QPaintEvent* )
{
    QPainter p( this );
    p.setPen(Qt::black);
    p.drawRect(rect());

    QRect t = titlebar->geometry();
    t.setTop(1);
    p.drawTiledPixmap(t.x()+1, t.y()+1, t.width()-2, t.height()-2,
                      isActive() ? *aTitlePix : *iTitlePix);
    qDrawShadePanel(&p, t.x(), t.y(), t.width(), t.height()-1,
                   options->colorGroup(Options::TitleBar, isActive()));
    p.drawLine(t.x(), t.bottom(), t.right(), t.bottom());
    QRegion r = rect();
    r = r.subtract( t );
    p.setClipRegion( r );
    p.setClipping( FALSE );

    t.setTop( 1 );
    t.setHeight(t.height()-2);
    t.setLeft( t.left() + 4 );
    t.setRight( t.right() - 2 );

    p.setPen(options->color(Options::Font, isActive()));
    p.setFont(options->font(isActive()));
    p.drawText( t, AlignCenter | AlignVCenter, caption() );


    qDrawShadePanel(&p, rect().x()+1, rect().bottom()-6, 24, 6,
                    options->colorGroup(Options::Handle, isActive()), false);
    p.drawTiledPixmap(rect().x()+2, rect().bottom()-5, 22, 4,
                      isActive() ? *aHandlePix : *iHandlePix);

    qDrawShadePanel(&p, rect().x()+25, rect().bottom()-6, rect().width()-50, 6,
                    options->colorGroup(Options::Frame, isActive()), false);
    p.drawTiledPixmap(rect().x()+26, rect().bottom()-5, rect().width()-52, 4,
                      isActive() ? *aFramePix : *iFramePix);

    qDrawShadePanel(&p, rect().right()-24, rect().bottom()-6, 24, 6,
                    options->colorGroup(Options::Handle, isActive()), false);
    p.drawTiledPixmap(rect().right()-23, rect().bottom()-5, 22, 4,
                      isActive() ? *aHandlePix : *iHandlePix);
}

void NextClient::mouseDoubleClickEvent( QMouseEvent * e )
{
    if (titlebar->geometry().contains( e->pos() ) )
	workspace()->performWindowOperation( this, options->operationTitlebarDblClick() );
}

void NextClient::stickyChange(bool on)
{
    if (NextButton * b = button[STICKY_IDX]) {
        b->setBitmap( on ? unsticky_bits : sticky_bits, 10, 10);
        b->setTipText( on ? i18n("Un-Sticky") : i18n("Sticky") );
    }
}


void NextClient::init()
{
    Client::init();
}

void NextClient::activeChange(bool)
{
    repaint(false);
    slotReset();
}

void NextClient::slotReset()
{
    for (int i=0; i<MAX_NUM_BUTTONS; i++) {
        if (button[i]) {
            button[i]->reset();
        }
    }
}

Client::MousePosition
NextClient::mousePosition( const QPoint& p ) const
{
  MousePosition m = Nowhere;

  if (p.y() < (height() - 6))
    m = Client::mousePosition(p);

  else {
    if (p.x() >= (width() - 25))
      m = BottomRight;
    else if (p.x() <= 25)
      m = BottomLeft;
    else
      m = Bottom;
  }

  return m;
}

extern "C"
{
    Client *allocate(Workspace *ws, WId w, int )
    {
        return(new NextClient(ws, w));
    }
    void init()
    {
       create_pixmaps();
    }
    void reset()
    {
       delete_pixmaps();
       create_pixmaps();
       // Ensure change in tooltip state gets applied
       Workspace::self()->slotResetAllClientsDelayed();
    }
    void deinit()
    {
       delete_pixmaps();
    }
}

#include "nextclient.moc"
