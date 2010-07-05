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

#ifndef KWIN_KICKER_H
#define KWIN_KICKER_H

#include <kwineffects.h>
#include <QObject>

class QTimer;
class QWebPage;

namespace KWin
{

/**
 * This effect was written for a talk at Akademy 2010 to have the current
 * score of the 2010 World Cup Quarterfinal Argentina vs Germany on screen
 * during the presentation.
 *
 * The effect connects to the German football website "kicker.de" and parses
 * the returned website. This explains the name of the effect - it is unrelated
 * to the panel of KDE 3.
 *
 * The effect is a nice illustration on how to use EffectFrames and animations.
 * Therefore it was imported into the test directory in SVN to provide a small
 * example.
 *
 * The effect consists of two parts:
 * displaying the current score and showing a goal animation when a goal is
 * scored. The first uses a Plasma styled EffectFrame, while the latter uses
 * an unstyled effect frame.
 *
 * @author Martin Gräßlin
 */
class KickerEffect : public QObject, public Effect
    {
    Q_OBJECT
    public:
        KickerEffect();
        ~KickerEffect();
        virtual void reconfigure( ReconfigureFlags );
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );

    private Q_SLOTS:
        void slotLoadFinished(bool ok);
        void timeout();
        void goalTimeout();

    private:
        void generateGoalImage();
        QWebPage *m_page;
        QTimer *m_timer;
        QTimer *m_goalTimer;
        QSize m_size;
        QList<EffectFrame*> m_frames;
        QList<EffectFrame*> m_goalFrames;
        bool m_goalActive;
        QString m_score;
        TimeLine m_timeLine;
        bool m_ascending;
    };

}

#endif // KWIN_KICKER_H
