/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

//#define QT_CLEAN_NAMESPACE
#include "tabbox.h"
#include "workspace.h"
#include "client.h"
#include <qpainter.h>
#include <qlabel.h>
#include <qdrawutil.h>
#include <qstyle.h>
#include <kglobal.h>
#include <fixx11h.h>
#include <kconfig.h>
#include <klocale.h>
#include <qapplication.h>
#include <qdesktopwidget.h>
#include <qcursor.h>
#include <kstringhandler.h>
#include <stdarg.h>
#include <kdebug.h>
#include <kglobalaccel.h>
#include <kkeynative.h>
#include <kglobalsettings.h>
#include <kiconeffect.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <QX11Info>

// specify externals before namespace

namespace KWinInternal
{

extern QPixmap* kwin_get_menu_pix_hack();

TabBox::TabBox( Workspace *ws, const char *name )
    : Q3Frame( 0, name, Qt::WNoAutoErase ), client(0), wspace(ws)
    {
    setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
    setLineWidth(2);
    setMargin(2);

    showMiniIcon = false;

    no_tasks = i18n("*** No Windows ***");
    m = DesktopMode; // init variables
    updateKeyMapping();
    reconfigure();
    reset();
    connect(&delayedShowTimer, SIGNAL(timeout()), this, SLOT(show()));
    }

TabBox::~TabBox()
    {
    }


/*!
  Sets the current mode to \a mode, either DesktopListMode or WindowsMode

  \sa mode()
 */
void TabBox::setMode( Mode mode )
    {
    m = mode;
    }


/*!
  Create list of clients on specified desktop, starting with client c
*/
void TabBox::createClientList(ClientList &list, int desktop /*-1 = all*/, Client *c, bool chain)
    {
    ClientList::size_type idx = 0;

    list.clear();

    Client* start = c;

    if ( chain )
        c = workspace()->nextFocusChainClient(c);
    else
        c = workspace()->stackingOrder().first();

    Client* stop = c;

    while ( c )
        {
        if ( ((desktop == -1) || c->isOnDesktop(desktop))
             && c->wantsTabFocus() )
            {
            if ( start == c )
                {
                list.remove( c );
                list.prepend( c );
                }
            else
                { // don't add windows that have modal dialogs
                Client* modal = c->findModal();
                if( modal == NULL || modal == c )
                    list += c;
                else if( !list.contains( modal ))
                    list += modal;
                else
                    ; // nothing
                }
            }

        if ( chain )
          c = workspace()->nextFocusChainClient( c );
        else
          {
          if ( idx >= (workspace()->stackingOrder().size()-1) )
            c = 0;
          else
            c = workspace()->stackingOrder()[++idx];
          }

        if ( c == stop )
            break;
        }
    }


/*!
  Resets the tab box to display the active client in WindowsMode, or the
  current desktop in DesktopListMode
 */
void TabBox::reset()
    {
    int w, h, cw = 0, wmax = 0;

    QRect r = KGlobalSettings::desktopGeometry(QCursor::pos());

    // calculate height of 1 line
    // fontheight + 1 pixel above + 1 pixel below, or 32x32 icon + 2 pixel above + below
    lineHeight = qMax(fontMetrics().height() + 2, 32 + 4);

    if ( mode() == WindowsMode )
        {
        client = workspace()->activeClient();

        // get all clients to show
        createClientList(clients, options_traverse_all ? -1 : workspace()->currentDesktop(), client, true);

        // calculate maximum caption width
        cw = fontMetrics().width(no_tasks)+20;
        for (ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it)
          {
          cw = fontMetrics().width( (*it)->caption() );
          if ( cw > wmax ) wmax = cw;
          }

        // calculate height for the popup
        if ( clients.count() == 0 )  // height for the "not tasks" text
          {
          QFont f = font();
          f.setBold( true );
          f.setPointSize( 14 );

          h = QFontMetrics(f).height()*4;
          }
        else
          {
          showMiniIcon = false;
          h = clients.count() * lineHeight;

          if ( h > (r.height()-(2*frameWidth())) )  // if too high, use mini icons
            {
            showMiniIcon = true;
            // fontheight + 1 pixel above + 1 pixel below, or 16x16 icon + 1 pixel above + below
            lineHeight = qMax(fontMetrics().height() + 2, 16 + 2);

            h = clients.count() * lineHeight;

            if ( h > (r.height()-(2*frameWidth())) ) // if still too high, remove some clients
              {
                // how many clients to remove
                int howMany = (h - (r.height()-(2*frameWidth())))/lineHeight;
                for (; howMany; howMany--)
                  clients.remove(clients.last());

                h = clients.count() * lineHeight;
              }
            }
          }
        }
    else
        { // DesktopListMode
        showMiniIcon = false;
        desk = workspace()->currentDesktop();

        for ( int i = 1; i <= workspace()->numberOfDesktops(); i++ )
          {
          cw = fontMetrics().width( workspace()->desktopName(i) );
          if ( cw > wmax ) wmax = cw;
          }

        // calculate height for the popup (max. 16 desktops always fit in a 800x600 screen)
        h = workspace()->numberOfDesktops() * lineHeight;
        }

    // height, width for the popup
    h += 2 * frameWidth();
    w = 2*frameWidth() + 5*2 + ( showMiniIcon ? 16 : 32 ) + 8 + wmax; // 5*2=margins, ()=icon, 8=space between icon+text
    w = qBound( r.width()/3 , w, r.width() * 4 / 5 );

    setGeometry( (r.width()-w)/2 + r.x(),
                 (r.height()-h)/2+ r.y(),
                 w, h );
    }


/*!
  Shows the next or previous item, depending on \a next
 */
void TabBox::nextPrev( bool next)
    {
    if ( mode() == WindowsMode )
        {
        Client* firstClient = 0;
        do
            {
            if ( next )
                client = workspace()->nextFocusChainClient(client);
            else
                client = workspace()->previousFocusChainClient(client);
            if (!firstClient)
                {
		// When we see our first client for the second time,
		// it's time to stop.
                firstClient = client;
                }
            else if (client == firstClient)
                {
		// No candidates found.
                client = 0;
                break;
                }
            } while ( client && !clients.contains( client ));
        }
    else if( mode() == DesktopMode )
        {
        if ( next )
            desk = workspace()->nextDesktopFocusChain( desk );
        else
            desk = workspace()->previousDesktopFocusChain( desk );
        }
    else
        { // DesktopListMode
        if ( next )
            {
            desk++;
            if ( desk > workspace()->numberOfDesktops() )
                desk = 1;
            }
        else
            {
            desk--;
            if ( desk < 1 )
                desk = workspace()->numberOfDesktops();
            }
        }

    update();
    }



/*!
  Returns the currently displayed client ( only works in WindowsMode ).
  Returns 0 if no client is displayed.
 */
Client* TabBox::currentClient()
    {
    if ( mode() != WindowsMode )
        return 0;
    if (!workspace()->hasClient( client ))
        return 0;
    return client;
    }

/*!
  Returns the currently displayed virtual desktop ( only works in
  DesktopListMode )
  Returns -1 if no desktop is displayed.
 */
int TabBox::currentDesktop()
    {
    if ( mode() == DesktopListMode || mode() == DesktopMode )
        return desk;
    else
        return -1;
    }


/*!
  Reimplemented to raise the tab box as well
 */
void TabBox::showEvent( QShowEvent* )
    {
    raise();
    }


/*!
  hide the icon box if necessary
 */
void TabBox::hideEvent( QHideEvent* )
    {
    }

/*!
  Paints the tab box
 */
void TabBox::drawContents( QPainter * )
    {
    QRect r(contentsRect());
    QPixmap pix(r.size());  // do double buffering to avoid flickers
    pix.fill(this, 0, 0);

    QPainter p;
    p.begin(&pix, this);

    QPixmap* menu_pix = kwin_get_menu_pix_hack();

    int iconWidth = showMiniIcon ? 16 : 32;
    int x = 0;
    int y = 0;

    if ( mode () == WindowsMode )
        {
        if ( !currentClient() )
            {
            QFont f = font();
            f.setBold( true );
            f.setPointSize( 14 );

            p.setFont(f);
            p.drawText( r, Qt::AlignCenter, no_tasks);
            }
        else
            {
            for (ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it)
              {
              if ( workspace()->hasClient( *it ) )  // safety
                  {
                  // draw highlight background
                  if ( (*it) == currentClient() )
                    p.fillRect(x, y, r.width(), lineHeight, colorGroup().highlight());

                  // draw icon
                  QPixmap icon;
                  if ( showMiniIcon )
                    {
                    if ( !(*it)->miniIcon().isNull() )
                      icon = (*it)->miniIcon();
                    }
                  else
                    if ( !(*it)->icon().isNull() )
                      icon = (*it)->icon();
                    else if ( menu_pix )
                      icon = *menu_pix;
                
                  if( !icon.isNull())
                    {
                    if( (*it)->isMinimized())
                        KIconEffect::semiTransparent( icon );
                    p.drawPixmap( x+5, y + (lineHeight - iconWidth)/2, icon );
                    }

                  // generate text to display
                  QString s;

                  if ( !(*it)->isOnDesktop(workspace()->currentDesktop()) )
                    s = workspace()->desktopName((*it)->desktop()) + ": ";

                  if ( (*it)->isMinimized() )
                    s += QString("(") + (*it)->caption() + ")";
                  else
                    s += (*it)->caption();

                  s = KStringHandler::cPixelSqueeze(s, fontMetrics(), r.width() - 5 - iconWidth - 8);

                  // draw text
                  if ( (*it) == currentClient() )
                    p.setPen(colorGroup().highlightedText());
                  else if( (*it)->isMinimized())
                    {
                    QColor c1 = colorGroup().text();
                    QColor c2 = colorGroup().background();
                    // from kicker's TaskContainer::blendColors()
                    int r1, g1, b1;
                    int r2, g2, b2;

                    c1.rgb( &r1, &g1, &b1 );
                    c2.rgb( &r2, &g2, &b2 );

                    r1 += (int) ( .5 * ( r2 - r1 ) );
                    g1 += (int) ( .5 * ( g2 - g1 ) );
                    b1 += (int) ( .5 * ( b2 - b1 ) );

                    p.setPen(QColor( r1, g1, b1 ));
                    }
                  else
                    p.setPen(colorGroup().text());

                  p.drawText(x+5 + iconWidth + 8, y, r.width() - 5 - iconWidth - 8, lineHeight,
                              Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, s);

                  y += lineHeight;
                  }
              if ( y >= r.height() ) break;
              }
            }
        }
    else
        { // DesktopMode || DesktopListMode
        int iconHeight = iconWidth;

        // get widest desktop name/number
        QFont f(font());
        f.setBold(true);
        f.setPixelSize(iconHeight - 4);  // pixel, not point because I need to know the pixels
        QFontMetrics fm(f);

        int wmax = 0;
        for ( int i = 1; i <= workspace()->numberOfDesktops(); i++ )
            {
            wmax = qMax(wmax, fontMetrics().width(workspace()->desktopName(i)));

            // calculate max width of desktop-number text
            QString num = QString::number(i);
            iconWidth = qMax(iconWidth - 4, fm.boundingRect(num).width()) + 4;
            }

        // In DesktopMode, start at the current desktop
        // In DesktopListMode, start at desktop #1
        int iDesktop = (mode() == DesktopMode) ? workspace()->currentDesktop() : 1;
        for ( int i = 1; i <= workspace()->numberOfDesktops(); i++ )
            {
            // draw highlight background
            if ( iDesktop == desk )  // current desktop
              p.fillRect(x, y, r.width(), lineHeight, colorGroup().highlight());

            p.save();

            // draw "icon" (here: number of desktop)
            p.fillRect(x+5, y+2, iconWidth, iconHeight, colorGroup().base());
            p.setPen(colorGroup().text());
            p.drawRect(x+5, y+2, iconWidth, iconHeight);

            // draw desktop-number
            p.setFont(f);
            QString num = QString::number(iDesktop);
            p.drawText(x+5, y+2, iconWidth, iconHeight, Qt::AlignCenter, num);

            p.restore();

            // draw desktop name text
            if ( iDesktop == desk )
              p.setPen(colorGroup().highlightedText());
            else
              p.setPen(colorGroup().text());

            p.drawText(x+5 + iconWidth + 8, y, r.width() - 5 - iconWidth - 8, lineHeight,
                       Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                       workspace()->desktopName(iDesktop));

            // show mini icons from that desktop aligned to each other
            int x1 = x + 5 + iconWidth + 8 + wmax + 5;

            ClientList list;
            createClientList(list, iDesktop, 0, false);
            // clients are in reversed stacking order
	    for ( int i = list.size() - 1; i>=0; i-- )
              {
              if ( !list.at( i )->miniIcon().isNull() )
                {
                if ( x1+18 >= x+r.width() )  // only show full icons
                  break;

                p.drawPixmap( x1, y + (lineHeight - 16)/2, list.at(  i )->miniIcon() );
                x1 += 18;
                }
              }

            // next desktop
            y += lineHeight;
            if ( y >= r.height() ) break;

            if( mode() == DesktopMode )
                iDesktop = workspace()->nextDesktopFocusChain( iDesktop );
            else
                iDesktop++;
            }
        }
    p.end();
    bitBlt(this, r.x(), r.y(), &pix);
    }

void TabBox::hide()
    {
    delayedShowTimer.stop();
    QWidget::hide();
    QApplication::syncX();
    XEvent otherEvent;
    while (XCheckTypedEvent (QX11Info::display(), EnterNotify, &otherEvent ) )
        ;
    }


void TabBox::reconfigure()
    {
    KConfig * c(KGlobal::config());
    c->setGroup("TabBox");
    options_traverse_all = c->readEntry("TraverseAll", false );
    }

/*!
  Rikkus: please document!   (Matthias)

  Ok, here's the docs :)

  You call delayedShow() instead of show() directly.

  If the 'ShowDelay' setting is false, show() is simply called.

  Otherwise, we start a timer for the delay given in the settings and only
  do a show() when it times out.

  This means that you can alt-tab between windows and you don't see the
  tab box immediately. Not only does this make alt-tabbing faster, it gives
  less 'flicker' to the eyes. You don't need to see the tab box if you're
  just quickly switching between 2 or 3 windows. It seems to work quite
  nicely.
 */
void TabBox::delayedShow()
    {
    KConfig * c(KGlobal::config());
    c->setGroup("TabBox");
    bool delay = c->readEntry("ShowDelay", true);

    if (!delay)
        {
        show();
        return;
        }

    int delayTime = c->readEntry("DelayTime", 90);
    delayedShowTimer.start(delayTime, true);
    }


void TabBox::handleMouseEvent( XEvent* e )
    {
    XAllowEvents( QX11Info::display(), AsyncPointer, QX11Info::appTime() );
    if( e->type != ButtonPress )
        return;
    QPoint pos( e->xbutton.x_root, e->xbutton.y_root );
    if( !geometry().contains( pos ))
        {
        workspace()->closeTabBox();  // click outside closes tab
        return;
        }
    pos.rx() -= x(); // pos is now inside tabbox
    pos.ry() -= y();
    int num = (pos.y()-frameWidth()) / lineHeight;

    if( mode() == WindowsMode )
        {
        for( ClientList::ConstIterator it = clients.begin();
             it != clients.end();
             ++it)
            {
            if( workspace()->hasClient( *it ) && (num == 0) ) // safety
                {
                client = *it;
                break;
                }
            num--;
            }
        }
    else
        {
        int iDesktop = (mode() == DesktopMode) ? workspace()->currentDesktop() : 1;
        for( int i = 1;
             i <= workspace()->numberOfDesktops();
             ++i )
            {
            if( num == 0 )
                {
                desk = iDesktop;
                break;
                }
            num--;
            if( mode() == DesktopMode )
                iDesktop = workspace()->nextDesktopFocusChain( iDesktop );
            else
                iDesktop++;
            }
        }
    update();
    }

//*******************************
// Workspace
//*******************************


/*!
  Handles alt-tab / control-tab
 */

static
bool areKeySymXsDepressed( bool bAll, const uint keySyms[], int nKeySyms )
    {
    char keymap[32];

    kDebug(125) << "areKeySymXsDepressed: " << (bAll ? "all of " : "any of ") << nKeySyms << endl;

    XQueryKeymap( QX11Info::display(), keymap );

    for( int iKeySym = 0; iKeySym < nKeySyms; iKeySym++ )
        {
        uint keySymX = keySyms[ iKeySym ];
        uchar keyCodeX = XKeysymToKeycode( QX11Info::display(), keySymX );
        int i = keyCodeX / 8;
        char mask = 1 << (keyCodeX - (i * 8));

        kDebug(125) << iKeySym << ": keySymX=0x" << QString::number( keySymX, 16 )
                << " i=" << i << " mask=0x" << QString::number( mask, 16 )
                << " keymap[i]=0x" << QString::number( keymap[i], 16 ) << endl;

                // Abort if bad index value,
        if( i < 0 || i >= 32 )
                return false;

                // If ALL keys passed need to be depressed,
        if( bAll )
            {
            if( (keymap[i] & mask) == 0 )
                    return false;
            }
        else
            {
                        // If we are looking for ANY key press, and this key is depressed,
            if( keymap[i] & mask )
                    return true;
            }
        }

        // If we were looking for ANY key press, then none was found, return false,
        // If we were looking for ALL key presses, then all were found, return true.
    return bAll;
    }

static const int MAX_KEYSYMS = 4;
static uint alt_keysyms[ MAX_KEYSYMS ];
static uint win_keysyms[ MAX_KEYSYMS ];

static bool areModKeysDepressed( const KKeySequence& seq )
    {
    uint rgKeySyms[10];
    int nKeySyms = 0;
    if( seq.isNull())
	return false;
    int mod = seq.key(seq.count()-1).modFlags();

    if ( mod & KKey::SHIFT )
        {
        rgKeySyms[nKeySyms++] = XK_Shift_L;
        rgKeySyms[nKeySyms++] = XK_Shift_R;
        }
    if ( mod & KKey::CTRL )
        {
        rgKeySyms[nKeySyms++] = XK_Control_L;
        rgKeySyms[nKeySyms++] = XK_Control_R;
        }
    if( mod & KKey::ALT )
        {
        for( int i = 0;
             i < MAX_KEYSYMS && alt_keysyms[ i ] != NoSymbol;
             ++i )
            rgKeySyms[nKeySyms++] = alt_keysyms[ i ];
        }
    if( mod & KKey::WIN )
        {
        for( int i = 0;
             i < MAX_KEYSYMS && win_keysyms[ i ] != NoSymbol;
             ++i )
            rgKeySyms[nKeySyms++] = win_keysyms[ i ];
        }

    return areKeySymXsDepressed( false, rgKeySyms, nKeySyms );
    }

static bool areModKeysDepressed( const KShortcut& cut )
    {
    for( unsigned int i = 0;
	 i < cut.count();
	 ++i )
	{
	if( areModKeysDepressed( cut.seq( i )))
	    return true;
	}
    return false;
    }

void TabBox::updateKeyMapping()
    {
    const int size = 6;
    uint keysyms[ size ] = { XK_Alt_L, XK_Alt_R, XK_Super_L, XK_Super_R, XK_Meta_L, XK_Meta_R };
    XModifierKeymap* map = XGetModifierMapping( QX11Info::display() );
    int altpos = 0;
    int winpos = 0;
    int winmodpos = -1;
    int winmod = KKeyNative::modXWin();
    while( winmod > 0 ) // get position of the set bit in winmod
        {
        winmod >>= 1;
        ++winmodpos;
        }
    for( int i = 0;
         i < MAX_KEYSYMS;
         ++i )
        alt_keysyms[ i ] = win_keysyms[ i ] = NoSymbol;
    for( int i = 0;
         i < size;
         ++i )
        {
        KeyCode keycode = XKeysymToKeycode( QX11Info::display(), keysyms[ i ] );
        for( int j = 0;
             j < map->max_keypermod;
             ++j )
            {
            if( map->modifiermap[ 3 * map->max_keypermod + j ] == keycode ) // Alt
                if( altpos < MAX_KEYSYMS )
                    alt_keysyms[ altpos++ ] = keysyms[ i ];
            if( winmodpos >= 0 && map->modifiermap[ winmodpos * map->max_keypermod + j ] == keycode )
                if( winpos < MAX_KEYSYMS )
                    win_keysyms[ winpos++ ] = keysyms[ i ];
            }
        }
    XFreeModifiermap( map );
    }

void Workspace::slotWalkThroughWindows()
    {
    if ( root != QX11Info::appRootWindow() )
        return;
    if ( tab_grab || control_grab )
        return;
    if ( options->altTabStyle == Options::CDE )
        {
        //XUngrabKeyboard(QX11Info::display(), QX11Info::appTime()); // need that because of accelerator raw mode
        // CDE style raise / lower
        CDEWalkThroughWindows( true );
        }
    else
        {
        if ( areModKeysDepressed( cutWalkThroughWindows ) )
            {
            if ( startKDEWalkThroughWindows() )
                KDEWalkThroughWindows( true );
            }
        else
            // if the shortcut has no modifiers, don't show the tabbox,
            // don't grab, but simply go to the next window
            KDEOneStepThroughWindows( true );
        }
    }

void Workspace::slotWalkBackThroughWindows()
    {
    if ( root != QX11Info::appRootWindow() )
        return;
    if( tab_grab || control_grab )
        return;
    if ( options->altTabStyle == Options::CDE )
        {
        // CDE style raise / lower
        CDEWalkThroughWindows( false );
        }
    else
        {
        if ( areModKeysDepressed( cutWalkThroughWindowsReverse ) )
            {
            if ( startKDEWalkThroughWindows() )
                KDEWalkThroughWindows( false );
            }
        else
            {
            KDEOneStepThroughWindows( false );
            }
        }
    }

void Workspace::slotWalkThroughDesktops()
    {
    if ( root != QX11Info::appRootWindow() )
        return;
    if( tab_grab || control_grab )
        return;
    if ( areModKeysDepressed( cutWalkThroughDesktops ) )
        {
        if ( startWalkThroughDesktops() )
            walkThroughDesktops( true );
        }
    else
        {
        oneStepThroughDesktops( true );
        }
    }

void Workspace::slotWalkBackThroughDesktops()
    {
    if ( root != QX11Info::appRootWindow() )
        return;
    if( tab_grab || control_grab )
        return;
    if ( areModKeysDepressed( cutWalkThroughDesktopsReverse ) )
        {
        if ( startWalkThroughDesktops() )
            walkThroughDesktops( false );
        }
    else
        {
        oneStepThroughDesktops( false );
        }
    }

void Workspace::slotWalkThroughDesktopList()
    {
    if ( root != QX11Info::appRootWindow() )
        return;
    if( tab_grab || control_grab )
        return;
    if ( areModKeysDepressed( cutWalkThroughDesktopList ) )
        {
        if ( startWalkThroughDesktopList() )
            walkThroughDesktops( true );
        }
    else
        {
        oneStepThroughDesktopList( true );
        }
    }

void Workspace::slotWalkBackThroughDesktopList()
    {
    if ( root != QX11Info::appRootWindow() )
        return;
    if( tab_grab || control_grab )
        return;
    if ( areModKeysDepressed( cutWalkThroughDesktopListReverse ) )
        {
        if ( startWalkThroughDesktopList() )
            walkThroughDesktops( false );
        }
    else
        {
        oneStepThroughDesktopList( false );
        }
    }

bool Workspace::startKDEWalkThroughWindows()
    {
    if( !establishTabBoxGrab())
        return false;
    tab_grab        = true;
    keys->setEnabled( false );
    disable_shortcuts_keys->setEnabled( false );
    client_keys->setEnabled( false );
    tab_box->setMode( TabBox::WindowsMode );
    tab_box->reset();
    return true;
    }

bool Workspace::startWalkThroughDesktops( int mode )
    {
    if( !establishTabBoxGrab())
        return false;
    control_grab = true;
    keys->setEnabled( false );
    disable_shortcuts_keys->setEnabled( false );
    client_keys->setEnabled( false );
    tab_box->setMode( (TabBox::Mode) mode );
    tab_box->reset();
    return true;
    }

bool Workspace::startWalkThroughDesktops()
    {
    return startWalkThroughDesktops( TabBox::DesktopMode );
    }

bool Workspace::startWalkThroughDesktopList()
    {
    return startWalkThroughDesktops( TabBox::DesktopListMode );
    }

void Workspace::KDEWalkThroughWindows( bool forward )
    {
    tab_box->nextPrev( forward );
    tab_box->delayedShow();
    }

void Workspace::walkThroughDesktops( bool forward )
    {
    tab_box->nextPrev( forward );
    tab_box->delayedShow();
    }

void Workspace::CDEWalkThroughWindows( bool forward )
    {
    Client* c = activeClient();
    Client* nc = c;
    bool options_traverse_all;
        {
        KConfigGroup group( KGlobal::config(), "TabBox" );
        options_traverse_all = group.readEntry("TraverseAll", false );
        }

    Client* firstClient = 0;
    do
        {
        nc = forward ? nextStaticClient(nc) : previousStaticClient(nc);
        if (!firstClient)
            {
            // When we see our first client for the second time,
            // it's time to stop.
            firstClient = nc;
            }
        else if (nc == firstClient)
            {
            // No candidates found.
            nc = 0;
            break;
            }
        } while (nc && nc != c &&
            (( !options_traverse_all && !nc->isOnDesktop(currentDesktop())) ||
             nc->isMinimized() || !nc->wantsTabFocus() ) );
    if (nc)
        {
        if (c && c != nc)
            lowerClient( c );
        if ( options->focusPolicyIsReasonable() )
            {
            activateClient( nc );
            if( nc->isShade())
                nc->setShade( ShadeActivated );
            }
        else
            {
            if( !nc->isOnDesktop( currentDesktop()))
                setCurrentDesktop( nc->desktop());
            raiseClient( nc );
            }
        }
    }

void Workspace::KDEOneStepThroughWindows( bool forward )
    {
    tab_box->setMode( TabBox::WindowsMode );
    tab_box->reset();
    tab_box->nextPrev( forward );
    if( Client* c = tab_box->currentClient() )
        {
        activateClient( c );
        if( c->isShade())
            c->setShade( ShadeActivated );
        }
    }

void Workspace::oneStepThroughDesktops( bool forward, int mode )
    {
    tab_box->setMode( (TabBox::Mode) mode );
    tab_box->reset();
    tab_box->nextPrev( forward );
    if ( tab_box->currentDesktop() != -1 )
        setCurrentDesktop( tab_box->currentDesktop() );
    }

void Workspace::oneStepThroughDesktops( bool forward )
    {
    oneStepThroughDesktops( forward, TabBox::DesktopMode );
    }

void Workspace::oneStepThroughDesktopList( bool forward )
    {
    oneStepThroughDesktops( forward, TabBox::DesktopListMode );
    }

/*!
  Handles holding alt-tab / control-tab
 */
void Workspace::tabBoxKeyPress( const KKeyNative& keyX )
    {
    bool forward = false;
    bool backward = false;

    if (tab_grab)
        {
        forward = cutWalkThroughWindows.contains( keyX );
        backward = cutWalkThroughWindowsReverse.contains( keyX );
        if (forward || backward)
            {
            kDebug(125) << "== " << cutWalkThroughWindows.toStringInternal()
                << " or " << cutWalkThroughWindowsReverse.toStringInternal() << endl;
            KDEWalkThroughWindows( forward );
            }
        }
    else if (control_grab)
        {
        forward = cutWalkThroughDesktops.contains( keyX ) ||
                  cutWalkThroughDesktopList.contains( keyX );
        backward = cutWalkThroughDesktopsReverse.contains( keyX ) ||
                   cutWalkThroughDesktopListReverse.contains( keyX );
        if (forward || backward)
            walkThroughDesktops(forward);
        }

    if (control_grab || tab_grab)
        {
        uint keyQt = keyX.keyCodeQt();
        if ( ((keyQt & 0xffff) == Qt::Key_Escape)
            && !(forward || backward) )
            { // if Escape is part of the shortcut, don't cancel
            closeTabBox();
            }
        }
    }

void Workspace::closeTabBox()
    {
    removeTabBoxGrab();
    tab_box->hide();
    keys->setEnabled( true );
    disable_shortcuts_keys->setEnabled( true );
    client_keys->setEnabled( true );
    tab_grab = false;
    control_grab = false;
    }

/*!
  Handles alt-tab / control-tab releasing
 */
void Workspace::tabBoxKeyRelease( const XKeyEvent& ev )
    {
    unsigned int mk = ev.state &
        (KKeyNative::modXShift() |
         KKeyNative::modXCtrl() |
         KKeyNative::modXAlt() |
         KKeyNative::modXWin() );
    // ev.state is state before the key release, so just checking mk being 0 isn't enough
    // using XQueryPointer() also doesn't seem to work well, so the check that all
    // modifiers are released: only one modifier is active and the currently released
    // key is this modifier - if yes, release the grab
    int mod_index = -1;
    for( int i = ShiftMapIndex;
         i <= Mod5MapIndex;
         ++i )
        if(( mk & ( 1 << i )) != 0 )
        {
        if( mod_index >= 0 )
            return;
        mod_index = i;
        }
    bool release = false;
    if( mod_index == -1 )
        release = true;
    else
        {
        XModifierKeymap* xmk = XGetModifierMapping(QX11Info::display());
        for (int i=0; i<xmk->max_keypermod; i++)
            if (xmk->modifiermap[xmk->max_keypermod * mod_index + i]
                == ev.keycode)
                release = true;
        XFreeModifiermap(xmk);
        }
    if( !release )
         return;
    if (tab_grab)
        {
        removeTabBoxGrab();
        tab_box->hide();
        keys->setEnabled( true );
        disable_shortcuts_keys->setEnabled( true );
        client_keys->setEnabled( true );
        tab_grab = false;
        if( Client* c = tab_box->currentClient())
            {
            activateClient( c );
            if( c->isShade())
                c->setShade( ShadeActivated );
            }
        }
    if (control_grab)
        {
        removeTabBoxGrab();
        tab_box->hide();
        keys->setEnabled( true );
        disable_shortcuts_keys->setEnabled( true );
        client_keys->setEnabled( true );
        control_grab = False;
        if ( tab_box->currentDesktop() != -1 )
            {
            setCurrentDesktop( tab_box->currentDesktop() );
            }
        }
    }


int Workspace::nextDesktopFocusChain( int iDesktop ) const
    {
    int i = desktop_focus_chain.indexOf( iDesktop );
    if( i >= 0 && i+1 < (int)desktop_focus_chain.size() )
            return desktop_focus_chain[i+1];
    else if( desktop_focus_chain.size() > 0 )
            return desktop_focus_chain[ 0 ];
    else
            return 1;
    }

int Workspace::previousDesktopFocusChain( int iDesktop ) const
    {
    int i = desktop_focus_chain.indexOf( iDesktop );
    if( i-1 >= 0 )
            return desktop_focus_chain[i-1];
    else if( desktop_focus_chain.size() > 0 )
            return desktop_focus_chain[desktop_focus_chain.size()-1];
    else
            return numberOfDesktops();
    }

/*!
  auxiliary functions to travers all clients according the focus
  order. Useful for kwms Alt-tab feature.
*/
Client* Workspace::nextFocusChainClient( Client* c ) const
    {
    if ( focus_chain.isEmpty() )
        return 0;
    ClientList::ConstIterator it = focus_chain.find( c );
    if ( it == focus_chain.end() )
        return focus_chain.last();
    if ( it == focus_chain.begin() )
        return focus_chain.last();
    --it;
    return *it;
    }

/*!
  auxiliary functions to travers all clients according the focus
  order. Useful for kwms Alt-tab feature.
*/
Client* Workspace::previousFocusChainClient( Client* c ) const
    {
    if ( focus_chain.isEmpty() )
        return 0;
    ClientList::ConstIterator it = focus_chain.find( c );
    if ( it == focus_chain.end() )
        return focus_chain.first();
    ++it;
    if ( it == focus_chain.end() )
        return focus_chain.first();
    return *it;
    }

/*!
  auxiliary functions to travers all clients according the static
  order. Useful for the CDE-style Alt-tab feature.
*/
Client* Workspace::nextStaticClient( Client* c ) const
    {
    if ( !c || clients.isEmpty() )
        return 0;
    ClientList::ConstIterator it = clients.find( c );
    if ( it == clients.end() )
        return clients.first();
    ++it;
    if ( it == clients.end() )
        return clients.first();
    return *it;
    }
/*!
  auxiliary functions to travers all clients according the static
  order. Useful for the CDE-style Alt-tab feature.
*/
Client* Workspace::previousStaticClient( Client* c ) const
    {
    if ( !c || clients.isEmpty() )
        return 0;
    ClientList::ConstIterator it = clients.find( c );
    if ( it == clients.end() )
        return clients.last();
    if ( it == clients.begin() )
        return clients.last();
    --it;
    return *it;
    }

bool Workspace::establishTabBoxGrab()
    {
    if( XGrabKeyboard( QX11Info::display(), root, false,
        GrabModeAsync, GrabModeAsync, QX11Info::appTime()) != GrabSuccess )
        return false;
    // Don't try to establish a global mouse grab using XGrabPointer, as that would prevent
    // using Alt+Tab while DND (#44972). However force passive grabs on all windows
    // in order to catch MouseRelease events and close the tabbox (#67416).
    // All clients already have passive grabs in their wrapper windows, so check only
    // the active client, which may not have it.
    assert( !forced_global_mouse_grab );
    forced_global_mouse_grab = true;
    if( active_client != NULL )
        active_client->updateMouseGrab();
    return true;
    }

void Workspace::removeTabBoxGrab()
    {
    XUngrabKeyboard(QX11Info::display(), QX11Info::appTime());
    assert( forced_global_mouse_grab );
    forced_global_mouse_grab = false;
    if( active_client != NULL )
        active_client->updateMouseGrab();
    }

} // namespace

#include "tabbox.moc"
