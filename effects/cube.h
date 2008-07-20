/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <ubuntu@martin-graesslin.com>

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

#ifndef KWIN_CUBE_H
#define KWIN_CUBE_H

#include <kwineffects.h>
#include <kwinglutils.h>
#include <QObject>
#include <QQueue>

namespace KWin
{

class CubeEffect
    : public QObject, public Effect
    {
    Q_OBJECT
    public:
        CubeEffect();
        ~CubeEffect();
        virtual void prePaintScreen( ScreenPrePaintData& data, int time );
        virtual void paintScreen( int mask, QRegion region, ScreenPaintData& data );
        virtual void postPaintScreen();
        virtual void prePaintWindow( EffectWindow* w, WindowPrePaintData& data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( EffectWindow* w);
        virtual bool borderActivated( ElectricBorder border );
        virtual void grabbedKeyboardEvent( QKeyEvent* e );
        virtual void mouseChanged( const QPoint& pos, const QPoint& oldpos, Qt::MouseButtons buttons, 
            Qt::MouseButtons oldbuttons, Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers );
        virtual void desktopChanged( int old );
    protected slots:
        void toggle();
    protected:
        enum RotationDirection
            {
            Left,
            Right,
            Upwards,
            Downwards
            };
        enum VerticalRotationPosition
            {
            Up,
            Normal,
            Down
            };
        void paintScene( int mask, QRegion region, ScreenPaintData& data );
        void paintCap( float left, float right, float z, float zTexture );
        void rotateToDesktop( int desktop );
        void setActive( bool active );
        bool activated;
        bool cube_painting;
        bool keyboard_grab;
        bool schedule_close;
        ElectricBorder borderActivate;
        int painting_desktop;
        Window input;
        int frontDesktop;
        float cubeOpacity;
        bool displayDesktopName;
        bool reflection;
        bool rotating;
        bool verticalRotating;
        bool desktopChangedWhileRotating;
        bool paintCaps;
        TimeLine timeLine;
        TimeLine verticalTimeLine;
        RotationDirection rotationDirection;
        RotationDirection verticalRotationDirection;
        VerticalRotationPosition verticalPosition;
        QQueue<RotationDirection> rotations;
        QQueue<RotationDirection> verticalRotations;
        QColor backgroundColor;
        QColor capColor;
        GLTexture* wallpaper;
        bool texturedCaps;
        GLTexture* capTexture;
        float manualAngle;
        float manualVerticalAngle;
        TimeLine::CurveShape currentShape;
        bool start;
        bool stop;
        bool reflectionPainting;
        bool slide;
        int oldDesktop;

        // variables for defining the projection matrix
        float fovy;
        float aspect;
        float ymax;
        float ymin;
        float xmax;
        float xmin;
        float zNear;
        float zFar;
    };

} // namespace

#endif
