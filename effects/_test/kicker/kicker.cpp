/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2010 Martin Gräßlin <kde@martin-graesslin.com>

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
#include "kicker.h"

#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtWebKit/QWebElement>
#include <QtWebKit/QWebElementCollection>
#include <QtWebKit/QWebFrame>
#include <QtWebKit/QWebPage>

namespace KWin
{

KWIN_EFFECT( kicker, KickerEffect )

KickerEffect::KickerEffect()
    : m_page( new QWebPage( this ) )
    , m_timer( new QTimer( this ) )
    , m_goalTimer( new QTimer( this ) )
    , m_goalActive( false )
    , m_ascending( false )
    {
    connect(m_page, SIGNAL(loadFinished(bool)), this, SLOT(slotLoadFinished(bool)));
    connect(m_timer, SIGNAL(timeout()), this, SLOT(timeout()));
    connect(m_goalTimer, SIGNAL(timeout()), this, SLOT(goalTimeout()));

    reconfigure( ReconfigureAll );
    // reload the webpage each half a minute
    m_timer->start(30*1000);
    // goal animation should end after seven seconds
    m_goalTimer->setInterval( 7 * 1000 );
    m_goalTimer->setSingleShot( true );
    // the animations should have a duration of half a second with an
    // ease in out curve
    m_timeLine.setCurveShape( TimeLine::EaseInOutCurve );
    m_timeLine.setDuration( 500 );
    // let's download the webpage immediatelly
    timeout();
    }

KickerEffect::~KickerEffect()
    {
    while( !m_frames.isEmpty() )
        {
        delete m_frames.first();
        m_frames.removeFirst();
        }
    while( !m_goalFrames.isEmpty() )
        {
        delete m_goalFrames.first();
        m_goalFrames.removeFirst();
        }
    }

void KickerEffect::reconfigure( ReconfigureFlags )
    {
    }

void KickerEffect::prePaintScreen( ScreenPrePaintData& data, int time )
    {
    // the goal animation uses a KWin::TimeLine to modify the opacity values
    // as long as the goal timer has not timed out m_goalActive is true
    // after the timeout an animation might still be running, so we continue
    // till progress reaches 0.0 again
    if( m_goalActive ||  m_timeLine.progress() != 0.0 )
        {
        // the animation might either be ascending (increasing opacity) or
        // descending (decreasing opacity). In case of ascending we add time
        // to the timeline till progress reaches 1.0. There we switch direction
        // to descending. In descending case of course vice versa.
        if( m_ascending )
            {
            m_timeLine.addTime( time );
            if( m_timeLine.progress() == 1.0 )
                {
                m_ascending = false;
                }
            }
        else
            {
            m_timeLine.removeTime( time );
            // it is possible that this is the last animation. Therefore the
            // anding with goalActive. If it has been the last animation we
            // do not need to keep the goal EffectFrame around any more, so
            // we delete them.
            if( m_timeLine.progress() == 0.0 && m_goalActive )
                {
                m_ascending = true;
                }
            else if( m_timeLine.progress() == 0.0 )
                {
                // goal animation finshed, let's delete the EffectFrames
                while( !m_goalFrames.isEmpty() )
                    {
                    delete m_goalFrames.first();
                    m_goalFrames.removeFirst();
                    }
                m_goalFrames.clear();
                }
            }
        }
    effects->prePaintScreen( data, time );
    }

void KickerEffect::paintScreen( int mask, QRegion region, ScreenPaintData &data )
    {
    effects->paintScreen( mask, region, data );

    if( m_size.isValid() )
        {
        // let's paint the score EffectFrame with unmodified opacity. As it
        // uses Plasma styled textures it's still translucent, but the text
        // is fully opaque.
        foreach( EffectFrame* frame, m_frames )
            frame->render( region, 1.0 );
        }
    if( m_goalActive || m_timeLine.progress() != 0.0 )
        {
        // the goal animation changes the opacity. Therefore we modify the
        // opacity by multiplying with the timeline's progress.
        // The text should be fully opaque (1.0), while the background should
        // be translucent (0.75)
        foreach( EffectFrame* frame, m_goalFrames )
            frame->render( region, 1.0 * m_timeLine.progress(), 0.75 * m_timeLine.progress() );
        // we are animating: need a new frame
        effects->addRepaintFull();
        }
    }

void KickerEffect::slotLoadFinished( bool ok )
    {
    // if connection failed let's keep the last score
    if( !ok )
        return;

    QWebElement doc = m_page->mainFrame()->documentElement();
    // CSS selector for the teams
    QWebElementCollection teams = doc.findAll("td.lttabver h1 a");
    if( teams.count() != 2 )
        return;
    QString firstTeam = teams[0].toPlainText();
    QString secondTeam = teams[1].toPlainText();

    // CSS selector for the current score
    QString result = doc.findFirst("td.lttaberg h1").toPlainText();
    if( m_score != result )
        {
        // the score changed, a goal might have been scored
        bool activate = true;
        // but not if the web page has been loaded for the first time
        if( m_score.isNull() )
            activate = false;
        // not if we entered extra time
        if( !m_score.contains("i.V.") && result.contains("i.V.") )
            activate = false;
        // not if extra time ended
        if( !m_score.contains("n.V.") && result.contains("n.V.") )
            activate = false;
        // not if penality shootout begins
        if( !m_score.contains("i.E.") && result.contains("i.E.") )
            activate = false;
        // not if first or second half starts.
        if( m_score.count( '-' ) > result.count( '-' ) )
            activate = false;
        // yeah it's a goal - generate the EffectFrame and start the animation
        if( activate )
            {
            generateGoalImage();
            m_goalActive = true;
            m_ascending = true;
            m_goalTimer->start();
            }
        m_score = result;
        }

    QString text = firstTeam + ' ' + result + ' ' + secondTeam;
    QFont font;
    font.setBold( true );
    QFontMetrics fm(font);
    m_size = fm.boundingRect(text).adjusted(-10, -10, 10, 10).size();
    // we don't want to reposition the EffectFrames, therefore we delete the
    // old ones. Normally you wouldn't do that, but as we only update once in
    // half a minute and it's easier to code...
    while( !m_frames.isEmpty() )
        {
        delete m_frames.first();
        m_frames.removeFirst();
        }
    m_frames.clear();
    // and properly position the frame on each screen
    for( int i = 0; i < effects->numScreens(); i++ )
        {
        QRect area = effects->clientArea( ScreenArea, i, effects->currentDesktop());
        QRect geometry = QRect( area.x() + area.width() - m_size.width() - 20, area.y() + 20, m_size.width(), m_size.height() );
        EffectFrame *frame = new EffectFrame( EffectFrame::Styled );
        frame->setText( text );
        frame->setFont( font );
        frame->setGeometry( geometry );
        m_frames.append(frame);
        }
    // effect frame changed and animation might have started - we need a full repaint
    effects->addRepaintFull();
    }

void KickerEffect::timeout()
    {
    // hard coded URL to the liveticker of the match Argentina vs Germany at World Cup 2010.
    // If this URL is not valid anymore you can find newer examples on the referrenced web site.
    m_page->mainFrame()->load(QUrl("http://www.kicker.de/news/fussball/wm/spielplan/weltmeisterschaft/2010/5/833353/livematch_argentinien_deutschland.html"));
    }

void KickerEffect::generateGoalImage()
    {
    QFont font( "FreeMono", 172 );
    QString goal( "GOAL" );
    QFontMetrics fm( font );
    QSize size = fm.boundingRect( goal ).adjusted(-10, -10, 10, 10).size();
    for( int i = 0; i < effects->numScreens(); i++ )
        {
        // place one frame on the center of each screen
        QRect area = effects->clientArea( ScreenArea, i, effects->currentDesktop());
        QRect geometry = QRect( area.x() + (area.width() - size.width())/2,
                                area.y() + (area.height() - size.height())/2,
                                size.width(), size.height());
        EffectFrame *frame = new EffectFrame( EffectFrame::Unstyled, false );
        frame->setText( goal );
        frame->setFont( font );
        frame->setGeometry( geometry );
        m_goalFrames.append(frame);
        }
    }

void KickerEffect::goalTimeout()
    {
    // stop the animation
    m_goalActive = false;
    effects->addRepaintFull();
    }

} // namespace
