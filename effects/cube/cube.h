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
#include <kshortcut.h>
#include <QObject>
#include <QQueue>
#include <QMatrix4x4>
#include <QTimeLine>
#include "cube_inside.h"
#include "cube_proxy.h"

namespace KWin
{

class CubeEffect
    : public Effect
{
    Q_OBJECT
public:
    CubeEffect();
    ~CubeEffect();
    virtual void reconfigure(ReconfigureFlags);
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void paintScreen(int mask, QRegion region, ScreenPaintData& data);
    virtual void postPaintScreen();
    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual bool borderActivated(ElectricBorder border);
    virtual void grabbedKeyboardEvent(QKeyEvent* e);
    virtual void windowInputMouseEvent(Window w, QEvent* e);
    virtual bool isActive() const;

    // proxy functions
    virtual void* proxy();
    void registerCubeInsideEffect(CubeInsideEffect* effect);
    void unregisterCubeInsideEffect(CubeInsideEffect* effect);

    static bool supported();
private slots:
    void toggleCube();
    void toggleCylinder();
    void toggleSphere();
    // slots for global shortcut changed
    // needed to toggle the effect
    void cubeShortcutChanged(const QKeySequence& seq);
    void cylinderShortcutChanged(const QKeySequence& seq);
    void sphereShortcutChanged(const QKeySequence& seq);
    void slotTabBoxAdded(int mode);
    void slotTabBoxUpdated();
    void slotTabBoxClosed();
    void slotMouseChanged(const QPoint& pos, const QPoint& oldpos, Qt::MouseButtons buttons,
                              Qt::MouseButtons oldbuttons, Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);
    void slotCubeCapLoaded();
    void slotWallPaperLoaded();
private:
    enum RotationDirection {
        Left,
        Right,
        Upwards,
        Downwards
    };
    enum VerticalRotationPosition {
        Up,
        Normal,
        Down
    };
    enum CubeMode {
        Cube,
        Cylinder,
        Sphere
    };
    void toggle(CubeMode newMode = Cube);
    void paintCube(int mask, QRegion region, ScreenPaintData& data);
    void paintCap(bool frontFirst, float zOffset);
    void paintCubeCap();
    void paintCylinderCap();
    void paintSphereCap();
    bool loadShader();
    void loadConfig(QString config);
    void rotateCube();
    void rotateToDesktop(int desktop);
    void setActive(bool active);
    QImage loadCubeCap(const QString &capPath);
    QImage loadWallPaper(const QString &file);
    bool activated;
    bool mousePolling;
    bool cube_painting;
    bool keyboard_grab;
    bool schedule_close;
    QList<ElectricBorder> borderActivate;
    QList<ElectricBorder> borderActivateCylinder;
    QList<ElectricBorder> borderActivateSphere;
    int painting_desktop;
    Window input;
    int frontDesktop;
    float cubeOpacity;
    bool opacityDesktopOnly;
    bool displayDesktopName;
    EffectFrame* desktopNameFrame;
    QFont desktopNameFont;
    bool reflection;
    bool rotating;
    bool verticalRotating;
    bool desktopChangedWhileRotating;
    bool paintCaps;
    QTimeLine timeLine;
    QTimeLine verticalTimeLine;
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
    QTimeLine::CurveShape currentShape;
    bool start;
    bool stop;
    bool reflectionPainting;
    int rotationDuration;
    int activeScreen;
    bool bottomCap;
    bool closeOnMouseRelease;
    float zoom;
    float zPosition;
    bool useForTabBox;
    bool invertKeys;
    bool invertMouse;
    bool tabBoxMode;
    bool shortcutsRegistered;
    CubeMode mode;
    bool useShaders;
    GLShader* cylinderShader;
    GLShader* sphereShader;
    GLShader* m_reflectionShader;
    GLShader* m_capShader;
    float capDeformationFactor;
    bool useZOrdering;
    float zOrderingFactor;
    bool useList;
    // needed for reflection
    float mAddedHeightCoeff1;
    float mAddedHeightCoeff2;

    QMatrix4x4 m_rotationMatrix;
    QMatrix4x4 m_reflectionMatrix;
    QMatrix4x4 m_textureMirrorMatrix;
    GLVertexBuffer *m_cubeCapBuffer;

    // Shortcuts - needed to toggle the effect
    KShortcut cubeShortcut;
    KShortcut cylinderShortcut;
    KShortcut sphereShortcut;

    // proxy
    CubeEffectProxy m_proxy;
    QList< CubeInsideEffect* > m_cubeInsideEffects;
};

} // namespace

#endif
