/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

// own
#include "tabboxhandler.h"
// tabbox
#include "clientitemdelegate.h"
#include "clientmodel.h"
#include "desktopitemdelegate.h"
#include "desktopmodel.h"
#include "itemlayoutconfig.h"
#include "tabboxconfig.h"
#include "tabboxview.h"
// Qt
#include <qdom.h>
#include <QFile>
#include <QKeyEvent>
#include <QModelIndex>
#include <QPainter>
#include <QX11Info>
#include <X11/Xlib.h>
// KDE
#include <KDebug>
#include <KStandardDirs>

namespace KWin
{
namespace TabBox
{

class TabBoxHandlerPrivate
    {
    public:
        TabBoxHandlerPrivate();

        ~TabBoxHandlerPrivate();

        /**
        * Updates the currently shown outline.
        */
        void updateOutline();
        /**
        * Hides the currently shown outline.
        */
        void hideOutline();
        /**
        * Updates the current highlight window state
        */
        void updateHighlightWindows();
        /**
        * Ends window highlighting
        */
        void endHighlightWindows();

        ClientModel* clientModel() const;
        DesktopModel* desktopModel() const;
        void parseConfig( const QString& fileName );

        // members
        TabBoxConfig config;
        TabBoxView* view;
        QModelIndex index;
        Window outlineLeft;
        Window outlineRight;
        Window outlineTop;
        Window outlineBottom;
        /**
        * Indicates if the tabbox is shown.
        * Used to determine if the outline has to be updated, etc.
        */
        bool isShown;
        QMap< QString, ItemLayoutConfig > tabBoxLayouts;
    };

TabBoxHandlerPrivate::TabBoxHandlerPrivate()
    {
    config = TabBoxConfig();
    view = new TabBoxView();
    XSetWindowAttributes attr;
    attr.override_redirect = 1;
    outlineLeft = XCreateWindow( QX11Info::display(), QX11Info::appRootWindow(), 0, 0, 1, 1, 0,
        CopyFromParent, CopyFromParent, CopyFromParent, CWOverrideRedirect, &attr );
    outlineRight = XCreateWindow( QX11Info::display(), QX11Info::appRootWindow(), 0, 0, 1, 1, 0,
        CopyFromParent, CopyFromParent, CopyFromParent, CWOverrideRedirect, &attr );
    outlineTop = XCreateWindow( QX11Info::display(), QX11Info::appRootWindow(), 0, 0, 1, 1, 0,
        CopyFromParent, CopyFromParent, CopyFromParent, CWOverrideRedirect, &attr );
    outlineBottom = XCreateWindow( QX11Info::display(), QX11Info::appRootWindow(), 0, 0, 1, 1, 0,
        CopyFromParent, CopyFromParent, CopyFromParent, CWOverrideRedirect, &attr );

    // load the layouts
    parseConfig( KStandardDirs::locate( "data", "kwin/DefaultTabBoxLayouts.xml" ) );
    view->clientDelegate()->setConfig( tabBoxLayouts.value( "Default" ) );
    view->additionalClientDelegate()->setConfig( tabBoxLayouts.value( "Text" ) );
    view->desktopDelegate()->setConfig( tabBoxLayouts.value( "Desktop" ) );
    view->desktopDelegate()->setLayouts( tabBoxLayouts );
    }

TabBoxHandlerPrivate::~TabBoxHandlerPrivate()
    {
    delete view;
    XDestroyWindow( QX11Info::display(), outlineLeft );
    XDestroyWindow( QX11Info::display(), outlineRight );
    XDestroyWindow( QX11Info::display(), outlineTop );
    XDestroyWindow( QX11Info::display(), outlineBottom );
    }

ClientModel* TabBoxHandlerPrivate::clientModel() const
    {
    return view->clientModel();
    }

DesktopModel* TabBoxHandlerPrivate::desktopModel() const
    {
    return view->desktopModel();
    }

void TabBoxHandlerPrivate::updateOutline()
    {
    if( config.tabBoxMode() != TabBoxConfig::ClientTabBox )
        return;
//     if( c == NULL || !m_isShown || !c->isShown( true ) || !c->isOnCurrentDesktop())
    if( !isShown || view->clientModel()->data( index, ClientModel::EmptyRole ).toBool() )
        {
        hideOutline();
        return;
        }
    TabBoxClient* c = static_cast< TabBoxClient* >(
            view->clientModel()->data( index, ClientModel::ClientRole ).value<void *>());
    // left/right parts are between top/bottom, they don't reach as far as the corners
    XMoveResizeWindow( QX11Info::display(), outlineLeft, c->x(), c->y() + 5, 5, c->height() - 10 );
    XMoveResizeWindow( QX11Info::display(), outlineRight, c->x() + c->width() - 5, c->y() + 5, 5, c->height() - 10 );
    XMoveResizeWindow( QX11Info::display(), outlineTop, c->x(), c->y(), c->width(), 5 );
    XMoveResizeWindow( QX11Info::display(), outlineBottom, c->x(), c->y() + c->height() - 5, c->width(), 5 );
    {
    QPixmap pix( 5, c->height() - 10 );
    QPainter p( &pix );
    p.setPen( Qt::white );
    p.drawLine( 0, 0, 0, pix.height() - 1 );
    p.drawLine( 4, 0, 4, pix.height() - 1 );
    p.setPen( Qt::gray );
    p.drawLine( 1, 0, 1, pix.height() - 1 );
    p.drawLine( 3, 0, 3, pix.height() - 1 );
    p.setPen( Qt::black );
    p.drawLine( 2, 0, 2, pix.height() - 1 );
    p.end();
    XSetWindowBackgroundPixmap( QX11Info::display(), outlineLeft, pix.handle());
    XSetWindowBackgroundPixmap( QX11Info::display(), outlineRight, pix.handle());
    }
    {
    QPixmap pix( c->width(), 5 );
    QPainter p( &pix );
    p.setPen( Qt::white );
    p.drawLine( 0, 0, pix.width() - 1 - 0, 0 );
    p.drawLine( 4, 4, pix.width() - 1 - 4, 4 );
    p.drawLine( 0, 0, 0, 4 );
    p.drawLine( pix.width() - 1 - 0, 0, pix.width() - 1 - 0, 4 );
    p.setPen( Qt::gray );
    p.drawLine( 1, 1, pix.width() - 1 - 1, 1 );
    p.drawLine( 3, 3, pix.width() - 1 - 3, 3 );
    p.drawLine( 1, 1, 1, 4 );
    p.drawLine( 3, 3, 3, 4 );
    p.drawLine( pix.width() - 1 - 1, 1, pix.width() - 1 - 1, 4 );
    p.drawLine( pix.width() - 1 - 3, 3, pix.width() - 1 - 3, 4 );
    p.setPen( Qt::black );
    p.drawLine( 2, 2, pix.width() - 1 - 2, 2 );
    p.drawLine( 2, 2, 2, 4 );
    p.drawLine( pix.width() - 1 - 2, 2, pix.width() - 1 - 2, 4 );
    p.end();
    XSetWindowBackgroundPixmap( QX11Info::display(), outlineTop, pix.handle());
    }
    {
    QPixmap pix( c->width(), 5 );
    QPainter p( &pix );
    p.setPen( Qt::white );
    p.drawLine( 4, 0, pix.width() - 1 - 4, 0 );
    p.drawLine( 0, 4, pix.width() - 1 - 0, 4 );
    p.drawLine( 0, 4, 0, 0 );
    p.drawLine( pix.width() - 1 - 0, 4, pix.width() - 1 - 0, 0 );
    p.setPen( Qt::gray );
    p.drawLine( 3, 1, pix.width() - 1 - 3, 1 );
    p.drawLine( 1, 3, pix.width() - 1 - 1, 3 );
    p.drawLine( 3, 1, 3, 0 );
    p.drawLine( 1, 3, 1, 0 );
    p.drawLine( pix.width() - 1 - 3, 1, pix.width() - 1 - 3, 0 );
    p.drawLine( pix.width() - 1 - 1, 3, pix.width() - 1 - 1, 0 );
    p.setPen( Qt::black );
    p.drawLine( 2, 2, pix.width() - 1 - 2, 2 );
    p.drawLine( 2, 0, 2, 2 );
    p.drawLine( pix.width() - 1 - 2, 0, pix.width() - 1 - 2, 2 );
    p.end();
    XSetWindowBackgroundPixmap( QX11Info::display(), outlineBottom, pix.handle());
    }
    XClearWindow( QX11Info::display(), outlineLeft );
    XClearWindow( QX11Info::display(), outlineRight );
    XClearWindow( QX11Info::display(), outlineTop );
    XClearWindow( QX11Info::display(), outlineBottom );
    XMapWindow( QX11Info::display(), outlineLeft );
    XMapWindow( QX11Info::display(), outlineRight );
    XMapWindow( QX11Info::display(), outlineTop );
    XMapWindow( QX11Info::display(), outlineBottom );
    }

void TabBoxHandlerPrivate::hideOutline()
    {
    XUnmapWindow( QX11Info::display(), outlineLeft );
    XUnmapWindow( QX11Info::display(), outlineRight );
    XUnmapWindow( QX11Info::display(), outlineTop );
    XUnmapWindow( QX11Info::display(), outlineBottom );
    }

void TabBoxHandlerPrivate::updateHighlightWindows()
    {
    if( !isShown )
        return;
    QVector< WId > data( 2 );
    Display *dpy = QX11Info::display();
    const WId wId = view->winId();
    data[ 0 ] = wId;
    data[ 1 ] = view->clientModel()->data( index, ClientModel::WIdRole ).toULongLong();
    if( config.isShowOutline() )
        {
        data.resize( 6 );
        data[ 2 ] = outlineLeft;
        data[ 3 ] = outlineTop;
        data[ 4 ] = outlineRight;
        data[ 5 ] = outlineBottom;
        }
    Atom atom = XInternAtom(dpy, "_KDE_WINDOW_HIGHLIGHT", False);
    XChangeProperty(dpy, wId, atom, atom, 32, PropModeReplace,
                    reinterpret_cast<unsigned char *>(data.data()), data.size());
    }

void TabBoxHandlerPrivate::endHighlightWindows()
    {
    // highlight windows
    Display *dpy = QX11Info::display();
    Atom atom = XInternAtom(dpy, "_KDE_WINDOW_HIGHLIGHT", False);
    XDeleteProperty( dpy, view->winId(), atom );
    }

/***********************************************************
* Based on the implementation of Kopete's
* contaclistlayoutmanager.cpp by Nikolaj Hald Nielsen and
* Roman Jarosz
***********************************************************/
void TabBoxHandlerPrivate::parseConfig( const QString& fileName )
    {
    // open the file
    if( !QFile::exists( fileName ) )
        {
        kDebug( 1212 ) << "File " << fileName << " does not exist";
        return;
        }
    QDomDocument doc( "Layouts" );
    QFile file( fileName );
    if( !file.open( QIODevice::ReadOnly ) )
        {
        kDebug( 1212 ) << "Error reading file " << fileName;
        return;
        }
    if( !doc.setContent( &file ) )
        {
        kDebug( 1212 ) << "Error parsing file " << fileName;
        file.close();
        return;
        }
    file.close();

    QDomElement layouts_element = doc.firstChildElement( "tabbox_layouts" );
    QDomNodeList layouts = layouts_element.elementsByTagName( "layout" );

    for( int i=0; i<layouts.size(); i++ )
        {
        QDomNode layout = layouts.item( i );
        ItemLayoutConfig currentLayout;

        // parse top elements
        QDomElement element = layout.toElement();
        QString layoutName = element.attribute( "name", "" );

        const bool highlightIcon = ( element.attribute( "highlight_selected_icon", "true" ).compare( "true", Qt::CaseInsensitive ) == 0 );
        currentLayout.setHighlightSelectedIcons( highlightIcon );

        const bool grayscaleIcon = ( element.attribute( "grayscale_deselected_icon", "false" ).compare( "true", Qt::CaseInsensitive ) == 0 );
        currentLayout.setGrayscaleDeselectedIcons( grayscaleIcon );

        // rows
        QDomNodeList rows = element.elementsByTagName( "row" );
        for( int j=0; j<rows.size(); j++ )
            {
            QDomNode rowNode = rows.item( j );

            ItemLayoutConfigRow row;

            QDomNodeList elements = rowNode.toElement().elementsByTagName( "element" );
            for( int k=0; k<elements.size(); k++ )
                {
                QDomNode elementNode = elements.item( k );
                QDomElement currentElement = elementNode.toElement();

                ItemLayoutConfigRowElement::ElementType type = ItemLayoutConfigRowElement::ElementType(currentElement.attribute(
                    "type", int( ItemLayoutConfigRowElement::ElementClientName ) ).toInt());
                ItemLayoutConfigRowElement currentRowElement;
                currentRowElement.setType( type );

                // width - used by all types
                qreal width = currentElement.attribute( "width", "0.0" ).toDouble();
                currentRowElement.setWidth( width );
                switch( type )
                    {
                    case ItemLayoutConfigRowElement::ElementEmpty:
                        row.addElement( currentRowElement );
                        break;
                    case ItemLayoutConfigRowElement::ElementIcon:
                        {
                        qreal iconWidth = currentElement.attribute( "icon_size", "16.0" ).toDouble();
                        currentRowElement.setIconSize( QSizeF( iconWidth, iconWidth ) );
                        currentRowElement.setRowSpan( currentElement.attribute( "row_span", "true" ).compare(
                                                        "true", Qt::CaseInsensitive ) == 0 );
                        row.addElement( currentRowElement );
                        break;
                        }
                    case ItemLayoutConfigRowElement::ElementClientList:
                        {
                        currentRowElement.setStretch( currentElement.attribute( "stretch", "false" ).compare(
                                                        "true", Qt::CaseInsensitive ) == 0 );
                        currentRowElement.setClientListLayoutName( currentElement.attribute( "layout_name", "" ) );
                        QString layoutMode = currentElement.attribute( "layout_mode", "horizontal" );
                        if( layoutMode.compare( "horizontal", Qt::CaseInsensitive ) == 0 )
                            currentRowElement.setClientListLayoutMode( TabBoxConfig::HorizontalLayout );
                        else if( layoutMode.compare( "vertical", Qt::CaseInsensitive ) == 0 )
                            currentRowElement.setClientListLayoutMode( TabBoxConfig::VerticalLayout );
                        else if( layoutMode.compare( "tabular", Qt::CaseInsensitive ) == 0 )
                            currentRowElement.setClientListLayoutMode( TabBoxConfig::HorizontalVerticalLayout );
                        row.addElement( currentRowElement );
                        break;
                        }
                    default: // text elements
                        {
                        currentRowElement.setStretch( currentElement.attribute( "stretch", "false" ).compare(
                                                        "true", Qt::CaseInsensitive ) == 0 );
                        currentRowElement.setSmallTextSize( currentElement.attribute( "small", "false" ).compare(
                                                        "true", Qt::CaseInsensitive ) == 0 );
                        currentRowElement.setBold( currentElement.attribute( "bold", "false" ).compare(
                                                        "true", Qt::CaseInsensitive ) == 0 );
                        currentRowElement.setItalic( currentElement.attribute( "italic", "false" ).compare(
                                                        "true", Qt::CaseInsensitive ) == 0 );
                        currentRowElement.setItalicMinimized( currentElement.attribute( "italic_minimized", "true" ).compare(
                                                        "true", Qt::CaseInsensitive ) == 0 );

                        currentRowElement.setPrefix( currentElement.attribute( "prefix", "" ) );
                        currentRowElement.setSuffix( currentElement.attribute( "suffix", "" ) );
                        currentRowElement.setPrefixMinimized( currentElement.attribute( "prefix_minimized", "" ) );
                        currentRowElement.setSuffixMinimzed( currentElement.attribute( "suffix_minimized", "" ) );

                        QString halign = currentElement.attribute( "horizontal_alignment", "left" );
                        Qt::Alignment alignment;
                        if( halign.compare( "left", Qt::CaseInsensitive ) == 0 )
                            alignment = Qt::AlignLeft;
                        else if( halign.compare( "right", Qt::CaseInsensitive ) == 0 )
                            alignment = Qt::AlignRight;
                        else
                            alignment = Qt::AlignCenter;
                        QString valign = currentElement.attribute( "vertical_alignment", "center" );
                        if( valign.compare( "top", Qt::CaseInsensitive ) == 0 )
                            alignment = alignment | Qt::AlignTop;
                        else if( valign.compare( "bottom", Qt::CaseInsensitive ) == 0 )
                            alignment = alignment | Qt::AlignBottom;
                        else
                            alignment = alignment | Qt::AlignVCenter;
                        currentRowElement.setAlignment( alignment );

                        row.addElement( currentRowElement );
                        break;
                        }// case default
                    } // switch type
                } // for loop elements

            currentLayout.addRow( row );
            } // for loop rows
        if( !layoutName.isEmpty() )
            {
            tabBoxLayouts.insert( layoutName, currentLayout );
            }
        } // for loop layouts
    }

/***********************************************
* TabBoxHandler
***********************************************/

TabBoxHandler::TabBoxHandler()
    : QObject()
    {
    KWin::TabBox::tabBox = this;
    d = new TabBoxHandlerPrivate;
    }

TabBoxHandler::~TabBoxHandler()
    {
    delete d;
    }

const KWin::TabBox::TabBoxConfig& TabBoxHandler::config() const
    {
    return d->config;
    }

void TabBoxHandler::setConfig( const TabBoxConfig& config )
    {
    if( config.layoutName() != d->config.layoutName() )
        {
        // new item layout config
        if( d->tabBoxLayouts.contains( config.layoutName() ) )
            {
            d->view->clientDelegate()->setConfig( d->tabBoxLayouts.value( config.layoutName() ) );
            d->view->desktopDelegate()->setConfig( d->tabBoxLayouts.value( config.layoutName() ) );
            }
        }
    if( config.selectedItemLayoutName() != d->config.selectedItemLayoutName() )
        {
        // TODO: desktop layouts
        if( d->tabBoxLayouts.contains( config.selectedItemLayoutName() ) )
            d->view->additionalClientDelegate()->setConfig( d->tabBoxLayouts.value( config.selectedItemLayoutName() ) );
        }
    d->config = config;
    emit configChanged();
    }

void TabBoxHandler::show()
    {
    d->isShown = true;
    // show the outline
    if( d->config.isShowOutline() )
        {
        d->updateOutline();
        }
    if( d->config.isShowTabBox() )
        {
        d->view->show();
        d->view->updateGeometry();
        if( d->config.isHighlightWindows() )
            {
            d->updateHighlightWindows();
            }
        }
    }

void TabBoxHandler::hide()
    {
    d->isShown = false;
    if( d->config.isHighlightWindows() )
        {
        d->endHighlightWindows();
        }
    if( d->config.isShowOutline() )
        {
        d->hideOutline();
        }
    d->view->hide();
    }

QModelIndex TabBoxHandler::nextPrev( bool forward ) const
    {
    QModelIndex ret;
    QAbstractItemModel* model;
    switch( d->config.tabBoxMode() )
        {
        case TabBoxConfig::ClientTabBox:
            model = d->clientModel();
            break;
        case TabBoxConfig::DesktopTabBox:
            model = d->desktopModel();
            break;
        }
    if( forward )
        {
        int column = d->index.column() + 1;
        int row = d->index.row();
        if( column == model->columnCount() )
            {
            column = 0;
            row++;
            if( row == model->rowCount() )
                row = 0;
            }
        ret = model->index( row, column );
        }
    else
        {
        int column = d->index.column() - 1;
        int row = d->index.row();
        if( column < 0 )
            {
            column = model->columnCount() - 1;
            row--;
            if( row < 0 )
                row = model->rowCount() - 1;
            }
        ret = model->index( row, column );
        }
    if( ret.isValid() )
        return ret;
    else
        return d->index;
    }

QModelIndex TabBoxHandler::desktopIndex( int desktop ) const
    {
    if( d->config.tabBoxMode() != TabBoxConfig::DesktopTabBox )
        return QModelIndex();
    return d->desktopModel()->desktopIndex( desktop );
    }

QList< int > TabBoxHandler::desktopList() const
    {
    if( d->config.tabBoxMode() != TabBoxConfig::DesktopTabBox )
        return QList< int >();
    return d->desktopModel()->desktopList();
    }

int TabBoxHandler::desktop( const QModelIndex& index ) const
    {
    if( !index.isValid() || (d->config.tabBoxMode() != TabBoxConfig::DesktopTabBox))
        return -1;
    QVariant ret = d->desktopModel()->data( index, DesktopModel::DesktopRole );
    if( ret.isValid() )
        return ret.toInt();
    else
        return -1;
    }

int TabBoxHandler::currentSelectedDesktop() const
    {
    return desktop( d->index );
    }

void TabBoxHandler::setCurrentIndex( const QModelIndex& index )
    {
    d->view->setCurrentIndex( index );
    d->index = index;
    if( d->config.tabBoxMode() == TabBoxConfig::ClientTabBox )
        {
        if( d->config.isShowOutline() )
            {
            d->updateOutline();
            }
        if( d->config.isShowTabBox() && d->config.isHighlightWindows() )
            {
            d->updateHighlightWindows();
            }
        }
    }

QModelIndex TabBoxHandler::grabbedKeyEvent( QKeyEvent* event ) const
    {
    QModelIndex ret;
    QAbstractItemModel* model;
    switch( d->config.tabBoxMode() )
        {
        case TabBoxConfig::ClientTabBox:
            model = d->clientModel();
            break;
        case TabBoxConfig::DesktopTabBox:
            model = d->desktopModel();
            break;
        }
    int column = d->index.column();
    int row = d->index.row();
    switch( event->key() )
        {
        case Qt::Key_Left:
            column--;
            if( column < 0 )
                column = model->columnCount() - 1;
            break;
        case Qt::Key_Right:
            column++;
            if( column >= model->columnCount() )
                column = 0;
            break;
        case Qt::Key_Up:
            row--;
            if( row < 0 )
                row = model->rowCount() - 1;
            break;
        case Qt::Key_Down:
            row++;
            if( row >= model->rowCount() )
                row = 0;
            break;
        default:
            // do not do anything for any other key
            break;
        }
    ret = model->index( row, column );

    if( ret.isValid() )
        return ret;
    else
        return d->index;
    }

bool TabBoxHandler::containsPos( const QPoint& pos ) const
    {
    return d->view->geometry().contains( pos );
    }

QModelIndex TabBoxHandler::indexAt( const QPoint& pos ) const
    {
    QPoint widgetPos = d->view->mapFromGlobal( pos );
    QModelIndex ret = d->view->indexAt( widgetPos );
    return ret;
    }

QModelIndex TabBoxHandler::index( KWin::TabBox::TabBoxClient* client ) const
    {
    return d->clientModel()->index( client );
    }

TabBoxClientList TabBoxHandler::clientList() const
    {
    if( d->config.tabBoxMode() != TabBoxConfig::ClientTabBox )
        return TabBoxClientList();
    return d->clientModel()->clientList();
    }

TabBoxClient* TabBoxHandler::client( const QModelIndex& index ) const
    {
    if( (!index.isValid()) ||
        ( d->config.tabBoxMode() != TabBoxConfig::ClientTabBox ) ||
        ( d->clientModel()->data( index, ClientModel::EmptyRole ).toBool() ) )
        return NULL;
    TabBoxClient* c = static_cast< TabBoxClient* >(
                d->clientModel()->data( index, ClientModel::ClientRole ).value<void *>());
    return c;
    }

void TabBoxHandler::createModel( bool partialReset )
    {
    switch( d->config.tabBoxMode() )
        {
        case TabBoxConfig::ClientTabBox:
            d->clientModel()->createClientList( partialReset );
            break;
        case TabBoxConfig::DesktopTabBox:
            d->desktopModel()->createDesktopList();
            break;
        }
    d->view->updateGeometry();
    }

QModelIndex TabBoxHandler::first() const
    {
    QAbstractItemModel* model;
    switch( d->config.tabBoxMode() )
        {
        case TabBoxConfig::ClientTabBox:
            model = d->clientModel();
            break;
        case TabBoxConfig::DesktopTabBox:
            model = d->desktopModel();
            break;
        }
    return model->index( 0, 0 );
    }

QWidget* TabBoxHandler::tabBoxView() const
    {
    return d->view;
    }

TabBoxHandler* tabBox = 0;

TabBoxClient::TabBoxClient()
    {
    }

TabBoxClient::~TabBoxClient()
    {
    }

} // namespace TabBox
} // namespace KWin
