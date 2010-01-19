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
#include "auroraepreview.h"
#include "themeconfig.h"
// Qt
#include <QFile>
#include <QFont>
#include <QPainter>
// KDE
#include <KGlobalSettings>
#include <KIcon>
#include <KLocale>
#include <KStandardDirs>
// Plasma
#include <Plasma/FrameSvg>

namespace KWin
{

AuroraePreview::AuroraePreview( const QString& name, const QString& packageName,
                                const QString& themeRoot, QObject* parent )
    : QObject( parent )
    , m_title( name )
    {
    m_svg = new Plasma::FrameSvg( this );
    QString svgFile = themeRoot + "/decoration.svg";
    if( QFile::exists( svgFile ) )
        {
        m_svg->setImagePath( svgFile );
        }
    else
        {
        m_svg->setImagePath( svgFile + 'z' );
        }
    m_svg->setEnabledBorders( Plasma::FrameSvg::AllBorders );

    m_themeConfig = new Aurorae::ThemeConfig();
    KConfig conf( "aurorae/themes/" + packageName + '/' + packageName + "rc", KConfig::FullConfig, "data" );
    m_themeConfig->load( &conf );

    initButtonFrame( "minimize", packageName );
    initButtonFrame( "maximize", packageName );
    initButtonFrame( "restore", packageName );
    initButtonFrame( "close", packageName );
    initButtonFrame( "alldesktops", packageName );
    initButtonFrame( "keepabove", packageName );
    initButtonFrame( "keepbelow", packageName );
    initButtonFrame( "shade", packageName );
    initButtonFrame( "help", packageName );
    }

AuroraePreview::~AuroraePreview()
    {
    delete m_themeConfig;
    }

void AuroraePreview::initButtonFrame( const QString &button, const QString &themeName )
    {
    QString file( "aurorae/themes/" + themeName + '/' + button + ".svg" );
    QString path = KGlobal::dirs()->findResource( "data", file );
    if( path.isEmpty() )
        {
        // let's look for svgz
        file.append("z");
        path = KGlobal::dirs()->findResource( "data", file );
        }
    if( !path.isEmpty() )
        {
        Plasma::FrameSvg *frame = new Plasma::FrameSvg( this );
        frame->setImagePath( path );
        frame->setCacheAllRenderedFrames( true );
        frame->setEnabledBorders( Plasma::FrameSvg::NoBorder );
        m_buttons.insert( button, frame );
        }
    }

QPixmap AuroraePreview::preview( const QSize& size, bool custom,
                                 const QString& left, const QString& right ) const
    {
    QPixmap pixmap( size );
    pixmap.fill( Qt::transparent );
    QPainter painter( &pixmap );

    painter.save();
    paintDeco( &painter, false, pixmap.rect(), 5 + m_themeConfig->paddingLeft() + m_themeConfig->borderLeft(),
               5, 5, 5 + m_themeConfig->paddingBottom() + m_themeConfig->borderBottom(),
               custom, left, right );
    painter.restore();
    painter.save();
    int activeLeft = 5;
    int activeTop = 5 + m_themeConfig->paddingTop() + m_themeConfig->titleEdgeTop() +
                    m_themeConfig->titleEdgeBottom() + m_themeConfig->titleHeight();
    int activeRight = 5 + m_themeConfig->paddingRight() + m_themeConfig->borderRight();
    int activeBottom = 5;
    paintDeco( &painter, true,  pixmap.rect(), activeLeft, activeTop, activeRight, activeBottom,
               custom, left, right );
    painter.restore();

    // paint title
    painter.save();
    QFont font = painter.font();
    font.setWeight( QFont::Bold );
    painter.setPen( m_themeConfig->activeTextColor() );
    painter.setFont( font );
    painter.drawText( QRect( pixmap.rect().topLeft() + QPoint( activeLeft, activeTop ),
                              pixmap.rect().bottomRight() - QPoint( activeRight, activeBottom ) ),
                       Qt::AlignCenter | Qt::TextWordWrap, m_title );
    painter.restore();

    return pixmap;
    }

void AuroraePreview::paintDeco( QPainter *painter, bool active, const QRect &rect,
                                int leftMargin, int topMargin, int rightMargin, int bottomMargin,
                                bool custom, const QString& left, const QString& right ) const
    {
    m_svg->setElementPrefix( "decoration" );
    if( !active && m_svg->hasElementPrefix("decoration-inactive") )
        {
        m_svg->setElementPrefix( "decoration-inactive" );
        }
    m_svg->resizeFrame( QSize( rect.width() - leftMargin - rightMargin, rect.height() - topMargin - bottomMargin ) );
    m_svg->paintFrame( painter, rect.topLeft() + QPoint( leftMargin, topMargin ) );

    int y = rect.top() + topMargin + m_themeConfig->paddingTop() + m_themeConfig->titleEdgeTop() + m_themeConfig->buttonMarginTop();
    int x = rect.left() + leftMargin + m_themeConfig->paddingLeft() + m_themeConfig->titleEdgeLeft();
    int buttonWidth = m_themeConfig->buttonWidth();
    int buttonHeight = m_themeConfig->buttonHeight();
    foreach( const QChar &character, custom ? left : m_themeConfig->defaultButtonsLeft() )
        {
        QString buttonName;
        int width = buttonWidth;
        if( character == '_' )
            {
            x += m_themeConfig->explicitButtonSpacer() + m_themeConfig->buttonSpacing();
            continue;
            }
        else if( character == 'M' )
            {
            KIcon icon = KIcon( "xorg" );
            int iconSize = qMin( m_themeConfig->buttonWidthMenu(), m_themeConfig->buttonHeight() );
            QSize buttonSize( iconSize,iconSize );
            painter->drawPixmap( QPoint( x, y ), icon.pixmap( buttonSize ) );
            x += m_themeConfig->buttonWidthMenu();
            }
        else if( character == 'S' )
            {
            buttonName = "alldesktops";
            width = m_themeConfig->buttonWidthAllDesktops();
            }
        else if( character == 'H' )
            {
            buttonName = "help";
            width = m_themeConfig->buttonWidthHelp();
            }
        else if( character == 'I' )
            {
            buttonName = "minimize";
            width = m_themeConfig->buttonWidthMinimize();
            }
        else if( character == 'A' )
            {
            buttonName = "restore";
            if( !m_buttons.contains( buttonName ) )
                {
                buttonName = "maximize";
                }
            width = m_themeConfig->buttonWidthMaximizeRestore();
            }
        else if( character == 'X' )
            {
            buttonName = "close";
            width = m_themeConfig->buttonWidthClose();
            }
        else if( character == 'F' )
            {
            buttonName = "keepabove";
            width = m_themeConfig->buttonWidthKeepAbove();
            }
        else if( character == 'B' )
            {
            buttonName = "keepbelow";
            width = m_themeConfig->buttonWidthKeepBelow();
            }
        else if( character == 'L' )
            {
            buttonName = "shade";
            width = m_themeConfig->buttonWidthShade();
            }
        if( !buttonName.isEmpty() && m_buttons.contains( buttonName ) )
            {
            Plasma::FrameSvg *frame = m_buttons.value( buttonName );
            frame->setElementPrefix( "active" );
            if( !active && frame->hasElementPrefix( "inactive" ) )
                {
                frame->setElementPrefix( "inactive" );
                }
            frame->resizeFrame( QSize( width, buttonHeight ) );
            frame->paintFrame( painter, QPoint( x, y ) );
            x += width;
            }
        x += m_themeConfig->buttonSpacing();
        }
    if( !m_themeConfig->defaultButtonsLeft().isEmpty() )
        {
        x -= m_themeConfig->buttonSpacing();
        }
    int titleLeft = x;

    x = rect.right() - rightMargin - m_themeConfig->paddingRight() - m_themeConfig->titleEdgeRight();
    QString rightButtons;
    foreach( const QChar &character, custom ? right : m_themeConfig->defaultButtonsRight() )
        {
        rightButtons.prepend(character);
        }
    foreach (const QChar &character, rightButtons)
        {
        QString buttonName;
        int width = buttonWidth;
        if( character == '_' )
            {
            x -= m_themeConfig->explicitButtonSpacer() + m_themeConfig->buttonSpacing();
            continue;
            }
        else if( character == 'M' )
            {
            KIcon icon = KIcon( "xorg" );
            QSize buttonSize( buttonWidth, buttonHeight );
            x -= m_themeConfig->buttonWidthMenu();
            painter->drawPixmap( QPoint( x, y ), icon.pixmap( buttonSize ) );
            }
        else if( character == 'S' )
            {
            buttonName = "alldesktops";
            width = m_themeConfig->buttonWidthAllDesktops();
            }
        else if( character == 'H' )
            {
            buttonName = "help";
            width = m_themeConfig->buttonWidthHelp();
            }
        else if( character == 'I' )
            {
            buttonName = "minimize";
            width = m_themeConfig->buttonWidthMinimize();
            }
        else if( character == 'A' )
            {
            buttonName = "restore";
            if( !m_buttons.contains( buttonName ) )
                {
                buttonName = "maximize";
                }
            width = m_themeConfig->buttonWidthMaximizeRestore();
            }
        else if( character == 'X' )
            {
            buttonName = "close";
            width = m_themeConfig->buttonWidthClose();
            }
        else if( character == 'F' )
            {
            buttonName = "keepabove";
            width = m_themeConfig->buttonWidthKeepAbove();
            }
        else if( character == 'B' )
            {
            buttonName = "keepbelow";
            width = m_themeConfig->buttonWidthKeepBelow();
            }
        else if( character == 'L' )
            {
            buttonName = "shade";
            width = m_themeConfig->buttonWidthShade();
            }
        if( !buttonName.isEmpty() && m_buttons.contains( buttonName ) )
            {
            Plasma::FrameSvg *frame = m_buttons.value( buttonName );
            frame->setElementPrefix( "active" );
            if( !active && frame->hasElementPrefix( "inactive" ) )
                {
                frame->setElementPrefix( "inactive" );
                }
            frame->resizeFrame( QSize( width, buttonHeight ) );
            x -= width;
            frame->paintFrame( painter, QPoint( x, y) );
            }
        x -= m_themeConfig->buttonSpacing();
        }
    if( !rightButtons.isEmpty() )
        {
        x += m_themeConfig->buttonSpacing();
        }
    int titleRight = x;

    // draw text
    y = rect.top() + topMargin + m_themeConfig->paddingTop() + m_themeConfig->titleEdgeTop();
    QRectF titleRect( QPointF( titleLeft, y ), QPointF( titleRight, y + m_themeConfig->titleHeight() ) );
    QString caption = i18n( "Active Window" );
    if( !active )
        {
        caption = i18n( "Inactive Window" );
        }
    painter->setFont( KGlobalSettings::windowTitleFont() );
    if( m_themeConfig->useTextShadow() )
        {
        // shadow code is inspired by Qt FAQ: How can I draw shadows behind text?
        // see http://www.qtsoftware.com/developer/faqs/faq.2007-07-27.3052836051
        painter->save();
        if( active )
            {
            painter->setPen( m_themeConfig->activeTextShadowColor() );
            }
        else
            {
            painter->setPen( m_themeConfig->inactiveTextShadowColor() );
            }
        int dx = m_themeConfig->textShadowOffsetX();
        int dy = m_themeConfig->textShadowOffsetY();
        painter->setOpacity( 0.5 );
        painter->drawText( titleRect.translated( dx, dy ),
                           m_themeConfig->alignment() | m_themeConfig->verticalAlignment() | Qt::TextSingleLine,
                           caption );
        painter->setOpacity( 0.2 );
        painter->drawText( titleRect.translated( dx+1, dy ),
                           m_themeConfig->alignment() | m_themeConfig->verticalAlignment() | Qt::TextSingleLine,
                           caption );
        painter->drawText( titleRect.translated( dx-1, dy ),
                           m_themeConfig->alignment() | m_themeConfig->verticalAlignment() | Qt::TextSingleLine,
                           caption );
        painter->drawText( titleRect.translated( dx, dy+1 ),
                           m_themeConfig->alignment() | m_themeConfig->verticalAlignment() | Qt::TextSingleLine,
                           caption );
        painter->drawText( titleRect.translated( dx, dy-1 ),
                           m_themeConfig->alignment() | m_themeConfig->verticalAlignment() | Qt::TextSingleLine,
                           caption );
        painter->restore();
        painter->save();
        }
    if( active )
        {
        painter->setPen( m_themeConfig->activeTextColor() );
        }
    else
        {
        painter->setPen( m_themeConfig->inactiveTextColor() );
        }
    painter->drawText( titleRect,
                       m_themeConfig->alignment() | m_themeConfig->verticalAlignment() | Qt::TextSingleLine,
                       caption );
    painter->restore();
    }

} // namespace KWin
