/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

 Copyright (C) 2008 Martin Gräßlin <mgraesslin@kde.org>

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
#include <QMatrix4x4>
#include <QTimeLine>
#include <QFont>
#include "cube_inside.h"
#include "cube_proxy.h"

namespace KWin
{

class CubeEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(qreal cubeOpacity READ configuredCubeOpacity)
    Q_PROPERTY(bool opacityDesktopOnly READ isOpacityDesktopOnly)
    Q_PROPERTY(bool displayDesktopName READ isDisplayDesktopName)
    Q_PROPERTY(bool reflection READ isReflection)
    Q_PROPERTY(int rotationDuration READ configuredRotationDuration)
    Q_PROPERTY(QColor backgroundColor READ configuredBackgroundColor)
    Q_PROPERTY(QColor capColor READ configuredCapColor)
    Q_PROPERTY(bool paintCaps READ isPaintCaps)
    Q_PROPERTY(bool closeOnMouseRelease READ isCloseOnMouseRelease)
    Q_PROPERTY(qreal zPosition READ configuredZPosition)
    Q_PROPERTY(bool useForTabBox READ isUseForTabBox)
    Q_PROPERTY(bool invertKeys READ isInvertKeys)
    Q_PROPERTY(bool invertMouse READ isInvertMouse)
    Q_PROPERTY(qreal capDeformationFactor READ configuredCapDeformationFactor)
    Q_PROPERTY(bool useZOrdering READ isUseZOrdering)
    Q_PROPERTY(bool texturedCaps READ isTexturedCaps)
    // TODO: electric borders: not a registered type
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
    virtual void windowInputMouseEvent(QEvent* e);
    virtual bool isActive() const;

    int requestedEffectChainPosition() const override {
        return 50;
    }

    // proxy functions
    virtual void* proxy();
    void registerCubeInsideEffect(CubeInsideEffect* effect);
    void unregisterCubeInsideEffect(CubeInsideEffect* effect);

    static bool supported();

    // for properties
    qreal configuredCubeOpacity() const {
        return cubeOpacity;
    }
    bool isOpacityDesktopOnly() const {
        return opacityDesktopOnly;
    }
    bool isDisplayDesktopName() const {
        return displayDesktopName;
    }
    bool isReflection() const {
        return reflection;
    }
    int configuredRotationDuration() const {
        return rotationDuration;
    }
    QColor configuredBackgroundColor() const {
        return backgroundColor;
    }
    QColor configuredCapColor() const {
        return capColor;
    }
    bool isPaintCaps() const {
        return paintCaps;
    }
    bool isCloseOnMouseRelease() const {
        return closeOnMouseRelease;
    }
    qreal configuredZPosition() const {
        return zPosition;
    }
    bool isUseForTabBox() const {
        return useForTabBox;
    }
    bool isInvertKeys() const {
        return invertKeys;
    }
    bool isInvertMouse() const {
        return invertMouse;
    }
    qreal configuredCapDeformationFactor() const {
        return capDeformationFactor;
    }
    bool isUseZOrdering() const {
        return useZOrdering;
    }
    bool isTexturedCaps() const {
        return texturedCaps;
    }
private Q_SLOTS:
    void toggleCube();
    void toggleCylinder();
    void toggleSphere();
    // slots for global shortcut changed
    // needed to toggle the effect
    void globalShortcutChanged(QAction *action, const QKeySequence &seq);
    void slotTabBoxAdded(int mode);
    void slotTabBoxUpdated();
    void slotTabBoxClosed();
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
    void paintCap(bool frontFirst, float zOffset, const QMatrix4x4 &projection);
    void paintCubeCap();
    void paintCylinderCap();
    void paintSphereCap();
    bool loadShader();
    void rotateCube();
    void rotateToDesktop(int desktop);
    void setActive(bool active);
    QImage loadCubeCap(const QString &capPath);
    QImage loadWallPaper(const QString &file);
    bool activated;
    bool cube_painting;
    bool keyboard_grab;
    bool schedule_close;
    QList<ElectricBorder> borderActivate;
    QList<ElectricBorder> borderActivateCylinder;
    QList<ElectricBorder> borderActivateSphere;
    int painting_desktop;
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
    QMatrix4x4 m_currentFaceMatrix;
    GLVertexBuffer *m_cubeCapBuffer;

    // Shortcuts - needed to toggle the effect
    QList<QKeySequence> cubeShortcut;
    QList<QKeySequence> cylinderShortcut;
    QList<QKeySequence> sphereShortcut;

    // proxy
    CubeEffectProxy m_proxy;
    QList< CubeInsideEffect* > m_cubeInsideEffects;

    QAction *m_cubeAction;
    QAction *m_cylinderAction;
    QAction *m_sphereAction;
};

} // namespace

#endif
