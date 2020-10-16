/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2008 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "cube.h"
// KConfigSkeleton
#include "cubeconfig.h"

#include "cube_inside.h"

#include <QAction>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <kwinconfig.h>

#include <QApplication>
#include <QColor>
#include <QElapsedTimer>
#include <QRect>
#include <QEvent>
#include <QFutureWatcher>
#include <QKeyEvent>
#include <QtConcurrentRun>
#include <QVector2D>
#include <QVector3D>

#include <cmath>

#include <kwinglutils.h>
#include <kwinglplatform.h>

namespace KWin
{

CubeEffect::CubeEffect()
    : activated(false)
    , cube_painting(false)
    , keyboard_grab(false)
    , painting_desktop(1)
    , frontDesktop(0)
    , cubeOpacity(1.0)
    , opacityDesktopOnly(true)
    , displayDesktopName(false)
    , desktopNameFrame(nullptr)
    , reflection(true)
    , desktopChangedWhileRotating(false)
    , paintCaps(true)
    , wallpaper(nullptr)
    , texturedCaps(true)
    , capTexture(nullptr)
    , reflectionPainting(false)
    , activeScreen(0)
    , bottomCap(false)
    , closeOnMouseRelease(false)
    , zoom(0.0)
    , zPosition(0.0)
    , useForTabBox(false)
    , tabBoxMode(false)
    , shortcutsRegistered(false)
    , mode(Cube)
    , useShaders(false)
    , cylinderShader(nullptr)
    , sphereShader(nullptr)
    , zOrderingFactor(0.0f)
    , mAddedHeightCoeff1(0.0f)
    , mAddedHeightCoeff2(0.0f)
    , m_cubeCapBuffer(nullptr)
    , m_proxy(this)
    , m_cubeAction(new QAction(this))
    , m_cylinderAction(new QAction(this))
    , m_sphereAction(new QAction(this))
{
    initConfig<CubeConfig>();
    desktopNameFont.setBold(true);
    desktopNameFont.setPointSize(14);

    if (effects->compositingType() == OpenGL2Compositing) {
        m_reflectionShader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture, QString(), QStringLiteral("cube-reflection.glsl"));
        m_capShader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture, QString(), QStringLiteral("cube-cap.glsl"));
    } else {
        m_reflectionShader = nullptr;
        m_capShader = nullptr;
    }
    m_textureMirrorMatrix.scale(1.0, -1.0, 1.0);
    m_textureMirrorMatrix.translate(0.0, -1.0, 0.0);
    connect(effects, &EffectsHandler::tabBoxAdded, this, &CubeEffect::slotTabBoxAdded);
    connect(effects, &EffectsHandler::tabBoxClosed, this, &CubeEffect::slotTabBoxClosed);
    connect(effects, &EffectsHandler::tabBoxUpdated, this, &CubeEffect::slotTabBoxUpdated);
    connect(effects, &EffectsHandler::screenAboutToLock, this, [this]() {
        // Set active(false) does not release key grabs until the animation completes
        // As we know the lockscreen is trying to grab them, release them early
        // all other grabs are released in the normal way
        setActive(false);
        if (keyboard_grab) {
            effects->ungrabKeyboard();
            keyboard_grab = false;
        }
    });

    reconfigure(ReconfigureAll);
}

bool CubeEffect::supported()
{
    return effects->isOpenGLCompositing();
}

void CubeEffect::reconfigure(ReconfigureFlags)
{
    CubeConfig::self()->read();
    foreach (ElectricBorder border, borderActivate) {
        effects->unreserveElectricBorder(border, this);
    }
    foreach (ElectricBorder border, borderActivateCylinder) {
        effects->unreserveElectricBorder(border, this);
    }
    foreach (ElectricBorder border, borderActivateSphere) {
        effects->unreserveElectricBorder(border, this);
    }
    borderActivate.clear();
    borderActivateCylinder.clear();
    borderActivateSphere.clear();
    QList<int> borderList = QList<int>();
    borderList.append(int(ElectricNone));
    borderList = CubeConfig::borderActivate();
    foreach (int i, borderList) {
        borderActivate.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i), this);
    }
    borderList.clear();
    borderList.append(int(ElectricNone));
    borderList = CubeConfig::borderActivateCylinder();
    foreach (int i, borderList) {
        borderActivateCylinder.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i), this);
    }
    borderList.clear();
    borderList.append(int(ElectricNone));
    borderList = CubeConfig::borderActivateSphere();
    foreach (int i, borderList) {
        borderActivateSphere.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i), this);
    }

    cubeOpacity = (float)CubeConfig::opacity() / 100.0f;
    opacityDesktopOnly = CubeConfig::opacityDesktopOnly();
    displayDesktopName = CubeConfig::displayDesktopName();
    reflection = CubeConfig::reflection();
    // TODO: Rename rotationDuration to duration so we
    // can use animationTime<CubeConfig>(500).
    const int d = CubeConfig::rotationDuration() != 0
        ? CubeConfig::rotationDuration()
        : 500;
    rotationDuration = std::chrono::milliseconds(static_cast<int>(animationTime(d)));
    backgroundColor = CubeConfig::backgroundColor();
    capColor = CubeConfig::capColor();
    paintCaps = CubeConfig::caps();
    closeOnMouseRelease = CubeConfig::closeOnMouseRelease();
    zPosition = CubeConfig::zPosition();

    useForTabBox = CubeConfig::tabBox();
    invertKeys = CubeConfig::invertKeys();
    invertMouse = CubeConfig::invertMouse();
    capDeformationFactor = (float)CubeConfig::capDeformation() / 100.0f;
    useZOrdering = CubeConfig::zOrdering();
    delete wallpaper;
    wallpaper = nullptr;
    delete capTexture;
    capTexture = nullptr;
    texturedCaps = CubeConfig::texturedCaps();

    timeLine.setEasingCurve(QEasingCurve::InOutSine);
    timeLine.setDuration(rotationDuration);

    verticalTimeLine.setEasingCurve(QEasingCurve::InOutSine);
    verticalTimeLine.setDuration(rotationDuration);

    // do not connect the shortcut if we use cylinder or sphere
    if (!shortcutsRegistered) {
        QAction* cubeAction = m_cubeAction;
        cubeAction->setObjectName(QStringLiteral("Cube"));
        cubeAction->setText(i18n("Desktop Cube"));
        KGlobalAccel::self()->setDefaultShortcut(cubeAction, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F11);
        KGlobalAccel::self()->setShortcut(cubeAction, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F11);
        effects->registerGlobalShortcut(Qt::CTRL + Qt::Key_F11, cubeAction);
        effects->registerPointerShortcut(Qt::ControlModifier | Qt::AltModifier, Qt::LeftButton, cubeAction);
        cubeShortcut = KGlobalAccel::self()->shortcut(cubeAction);
        QAction* cylinderAction = m_cylinderAction;
        cylinderAction->setObjectName(QStringLiteral("Cylinder"));
        cylinderAction->setText(i18n("Desktop Cylinder"));
        KGlobalAccel::self()->setShortcut(cylinderAction, QList<QKeySequence>());
        effects->registerGlobalShortcut(QKeySequence(), cylinderAction);
        cylinderShortcut = KGlobalAccel::self()->shortcut(cylinderAction);
        QAction* sphereAction = m_sphereAction;
        sphereAction->setObjectName(QStringLiteral("Sphere"));
        sphereAction->setText(i18n("Desktop Sphere"));
        KGlobalAccel::self()->setShortcut(sphereAction, QList<QKeySequence>());
        sphereShortcut = KGlobalAccel::self()->shortcut(sphereAction);
        effects->registerGlobalShortcut(QKeySequence(), sphereAction);
        connect(cubeAction, &QAction::triggered, this, &CubeEffect::toggleCube);
        connect(cylinderAction, &QAction::triggered, this, &CubeEffect::toggleCylinder);
        connect(sphereAction, &QAction::triggered, this, &CubeEffect::toggleSphere);
        connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, this, &CubeEffect::globalShortcutChanged);
        shortcutsRegistered = true;
    }

    // set the cap color on the shader
    if (m_capShader && m_capShader->isValid()) {
        ShaderBinder binder(m_capShader);
        m_capShader->setUniform(GLShader::Color, capColor);
    }

    // touch borders
    const QVector<ElectricBorder> relevantBorders{ElectricLeft, ElectricTop, ElectricRight, ElectricBottom};
    for (auto e : relevantBorders) {
        effects->unregisterTouchBorder(e, m_cubeAction);
        effects->unregisterTouchBorder(e, m_sphereAction);
        effects->unregisterTouchBorder(e, m_cylinderAction);
    }
    auto touchEdge = [&relevantBorders] (const QList<int> touchBorders, QAction *action) {
        for (int i : touchBorders) {
            if (!relevantBorders.contains(ElectricBorder(i))) {
                continue;
            }
            effects->registerTouchBorder(ElectricBorder(i), action);
        }
    };
    touchEdge(CubeConfig::touchBorderActivate(), m_cubeAction);
    touchEdge(CubeConfig::touchBorderActivateCylinder(), m_cylinderAction);
    touchEdge(CubeConfig::touchBorderActivateSphere(), m_sphereAction);
}

CubeEffect::~CubeEffect()
{
    delete wallpaper;
    delete capTexture;
    delete cylinderShader;
    delete sphereShader;
    delete desktopNameFrame;
    delete m_reflectionShader;
    delete m_capShader;
    delete m_cubeCapBuffer;
}

QImage CubeEffect::loadCubeCap(const QString &capPath)
{
    if (!texturedCaps) {
        return QImage();
    }
    return QImage(capPath);
}

void CubeEffect::slotCubeCapLoaded()
{
    QFutureWatcher<QImage> *watcher = dynamic_cast<QFutureWatcher<QImage>*>(sender());
    if (!watcher) {
        // not invoked from future watcher
        return;
    }
    QImage img = watcher->result();
    if (!img.isNull()) {
        effects->makeOpenGLContextCurrent();
        capTexture = new GLTexture(img);
        capTexture->setFilter(GL_LINEAR);
        if (!GLPlatform::instance()->isGLES()) {
            capTexture->setWrapMode(GL_CLAMP_TO_BORDER);
        }
        // need to recreate the VBO for the cube cap
        delete m_cubeCapBuffer;
        m_cubeCapBuffer = nullptr;
        effects->addRepaintFull();
    }
    watcher->deleteLater();
}

QImage CubeEffect::loadWallPaper(const QString &file)
{
    return QImage(file);
}

void CubeEffect::slotWallPaperLoaded()
{
    QFutureWatcher<QImage> *watcher = dynamic_cast<QFutureWatcher<QImage>*>(sender());
    if (!watcher) {
        // not invoked from future watcher
        return;
    }
    QImage img = watcher->result();
    if (!img.isNull()) {
        effects->makeOpenGLContextCurrent();
        wallpaper = new GLTexture(img);
        effects->addRepaintFull();
    }
    watcher->deleteLater();
}

bool CubeEffect::loadShader()
{
    effects->makeOpenGLContextCurrent();
    if (!(GLPlatform::instance()->supports(GLSL) &&
            (effects->compositingType() == OpenGL2Compositing)))
        return false;

    cylinderShader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture | ShaderTrait::AdjustSaturation | ShaderTrait::Modulate, QStringLiteral("cylinder.vert"), QString());
    if (!cylinderShader->isValid()) {
        qCCritical(KWINEFFECTS) << "The cylinder shader failed to load!";
        return false;
    } else {
        ShaderBinder binder(cylinderShader);
        cylinderShader->setUniform("sampler", 0);
        QRect rect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
        cylinderShader->setUniform("width", (float)rect.width() * 0.5f);
    }

    sphereShader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture | ShaderTrait::AdjustSaturation | ShaderTrait::Modulate, QStringLiteral("sphere.vert"), QString());
    if (!sphereShader->isValid()) {
        qCCritical(KWINEFFECTS) << "The sphere shader failed to load!";
        return false;
    } else {
        ShaderBinder binder(sphereShader);
        sphereShader->setUniform("sampler", 0);
        QRect rect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
        sphereShader->setUniform("width", (float)rect.width() * 0.5f);
        sphereShader->setUniform("height", (float)rect.height() * 0.5f);
        sphereShader->setUniform("u_offset", QVector2D(0, 0));
    }
    return true;
}

void CubeEffect::startAnimation(AnimationState state)
{
    QEasingCurve curve;
    /* If this is first and only animation -> EaseInOut
     *                      there is more  -> EaseIn
     * If there was an animation before, and this is the last one -> EaseOut
     *                                       there is more        -> Linear */
    if (animationState == AnimationState::None) {
        curve.setType(animations.empty() ? QEasingCurve::InOutSine : QEasingCurve::InCurve);
    } else {
        curve.setType(animations.empty() ? QEasingCurve::OutCurve : QEasingCurve::Linear);
    }
    timeLine.reset();
    timeLine.setEasingCurve(curve);
    startAngle = currentAngle;
    startFrontDesktop = frontDesktop;
    animationState = state;
}

void CubeEffect::startVerticalAnimation(VerticalAnimationState state)
{
    /* Ignore if there is nowhere to rotate */
    if ((qFuzzyIsNull(verticalCurrentAngle - 90.0f) && state == VerticalAnimationState::Upwards) ||
        (qFuzzyIsNull(verticalCurrentAngle + 90.0f) && state == VerticalAnimationState::Downwards)) {
        return;
    }
    verticalTimeLine.reset();
    verticalStartAngle = verticalCurrentAngle;
    verticalAnimationState = state;
}

void CubeEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (activated) {
        data.mask |= PAINT_SCREEN_TRANSFORMED | Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS | PAINT_SCREEN_BACKGROUND_FIRST;
        if (animationState == AnimationState::None && !animations.empty()) {
            startAnimation(animations.dequeue());
        }
        if (verticalAnimationState == VerticalAnimationState::None && !verticalAnimations.empty()) {
            startVerticalAnimation(verticalAnimations.dequeue());
        }

        if (animationState != AnimationState::None || verticalAnimationState != VerticalAnimationState::None) {
            if (animationState != AnimationState::None) {
                timeLine.update(std::chrono::milliseconds(time));
            }
            if (verticalAnimationState != VerticalAnimationState::None) {
                verticalTimeLine.update(std::chrono::milliseconds(time));
            }
            rotateCube();
        }
    }
    effects->prePaintScreen(data, time);
}

void CubeEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData& data)
{
    if (activated) {
        QRect rect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());

        // background
        float clearColor[4];
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
        glClearColor(backgroundColor.redF(), backgroundColor.greenF(), backgroundColor.blueF(), 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);

        // wallpaper
        if (wallpaper) {
            ShaderBinder binder(ShaderTrait::MapTexture);
            binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, data.projectionMatrix());
            wallpaper->bind();
            wallpaper->render(region, rect);
            wallpaper->unbind();
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // some veriables needed for painting the caps
        float cubeAngle = (float)((float)(effects->numberOfDesktops() - 2) / (float)effects->numberOfDesktops() * 180.0f);
        float point = rect.width() / 2 * tan(cubeAngle * 0.5f * M_PI / 180.0f);
        float zTranslate = zPosition + zoom;
        if (animationState == AnimationState::Start) {
            zTranslate *= timeLine.value();
        } else if (animationState == AnimationState::Stop) {
            zTranslate *= (1.0 - timeLine.value());
        }
        // reflection
        if (reflection) {
            // we can use a huge scale factor (needed to calculate the rearground vertices)
            float scaleFactor = 1000000 * tan(60.0 * M_PI / 360.0f) / rect.height();
            m_reflectionMatrix.setToIdentity();
            m_reflectionMatrix.scale(1.0, -1.0, 1.0);

            double translate = 0.0;
            if (mode == Cube) {
                double addedHeight1 = -rect.height() * cos(verticalCurrentAngle*M_PI/180.0f) - rect.width() * sin(fabs(verticalCurrentAngle)*M_PI/180.0f)/tan(M_PI/effects->numberOfDesktops());
                double addedHeight2 = -rect.width() * sin(fabs(verticalCurrentAngle)*M_PI/180.0f)*tan(M_PI*0.5f/effects->numberOfDesktops());
                if (verticalCurrentAngle > 0.0f && effects->numberOfDesktops() & 1)
                    translate = cos(fabs(currentAngle)*effects->numberOfDesktops()*M_PI/360.0f) * addedHeight2 + addedHeight1 - float(rect.height());
                else
                    translate = sin(fabs(currentAngle)*effects->numberOfDesktops()*M_PI/360.0f) * addedHeight2 + addedHeight1 - float(rect.height());
            } else if (mode == Cylinder) {
                double addedHeight1 = -rect.height() * cos(verticalCurrentAngle*M_PI/180.0f) - rect.width() * sin(fabs(verticalCurrentAngle)*M_PI/180.0f)/tan(M_PI/effects->numberOfDesktops());
                translate = addedHeight1 - float(rect.height());
            } else {
                float radius = (rect.width() * 0.5) / cos(cubeAngle * 0.5 * M_PI / 180.0);
                translate = -rect.height()-2*radius;
            }
            m_reflectionMatrix.translate(0.0f, translate, 0.0f);

            reflectionPainting = true;
            glEnable(GL_CULL_FACE);
            paintCap(true, -point - zTranslate, data.projectionMatrix());

            // cube
            glCullFace(GL_BACK);
            paintCube(mask, region, data);

            glCullFace(GL_FRONT);
            paintCube(mask, region, data);

            paintCap(false, -point - zTranslate, data.projectionMatrix());
            glDisable(GL_CULL_FACE);
            reflectionPainting = false;

            const float width = rect.width();
            const float height = rect.height();
            float vertices[] = {
                -width * 0.5f, height, 0.0,
                width * 0.5f, height, 0.0,
                width * scaleFactor, height, -5000,
                -width * scaleFactor, height, -5000
            };
            // foreground
            float alpha = 0.7;
            if (animationState == AnimationState::Start) {
                alpha = 0.3 + 0.4 * timeLine.value();
            } else if (animationState == AnimationState::Stop) {
                alpha = 0.3 + 0.4 * (1.0 - timeLine.value());
            }
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            if (m_reflectionShader && m_reflectionShader->isValid()) {
                // ensure blending is enabled - no attribute stack
                ShaderBinder binder(m_reflectionShader);
                QMatrix4x4 windowTransformation = data.projectionMatrix();
                windowTransformation.translate(rect.x() + rect.width() * 0.5f, 0.0, 0.0);
                m_reflectionShader->setUniform(GLShader::ModelViewProjectionMatrix, windowTransformation);
                m_reflectionShader->setUniform("u_alpha", alpha);
                QVector<float> verts;
                QVector<float> texcoords;
                verts.reserve(18);
                texcoords.reserve(12);
                texcoords << 0.0 << 0.0;
                verts << vertices[6] << vertices[7] << vertices[8];
                texcoords << 0.0 << 0.0;
                verts << vertices[9] << vertices[10] << vertices[11];
                texcoords << 1.0 << 0.0;
                verts << vertices[0] << vertices[1] << vertices[2];
                texcoords << 1.0 << 0.0;
                verts << vertices[0] << vertices[1] << vertices[2];
                texcoords << 1.0 << 0.0;
                verts << vertices[3] << vertices[4] << vertices[5];
                texcoords << 0.0 << 0.0;
                verts << vertices[6] << vertices[7] << vertices[8];
                GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
                vbo->reset();
                vbo->setData(6, 3, verts.data(), texcoords.data());
                vbo->render(GL_TRIANGLES);
            }
            glDisable(GL_BLEND);
        }
        glEnable(GL_CULL_FACE);
        // caps
        paintCap(false, -point - zTranslate, data.projectionMatrix());

        // cube
        glCullFace(GL_FRONT);
        paintCube(mask, region, data);

        glCullFace(GL_BACK);
        paintCube(mask, region, data);

        // cap
        paintCap(true, -point - zTranslate, data.projectionMatrix());
        glDisable(GL_CULL_FACE);

        glDisable(GL_BLEND);

        // desktop name box - inspired from coverswitch
        if (displayDesktopName) {
            double opacity = 1.0;
            if (animationState == AnimationState::Start) {
                opacity = timeLine.value();
            } else if (animationState == AnimationState::Stop) {
                opacity = 1.0 - timeLine.value();
            }
            QRect screenRect = effects->clientArea(ScreenArea, activeScreen, frontDesktop);
            QRect frameRect = QRect(screenRect.width() * 0.33f + screenRect.x(), screenRect.height() * 0.95f + screenRect.y(),
                                    screenRect.width() * 0.34f, QFontMetrics(desktopNameFont).height());
            if (!desktopNameFrame) {
                desktopNameFrame = effects->effectFrame(EffectFrameStyled);
                desktopNameFrame->setFont(desktopNameFont);
            }
            desktopNameFrame->setGeometry(frameRect);
            desktopNameFrame->setText(effects->desktopName(frontDesktop));
            desktopNameFrame->render(region, opacity);
        }
    } else {
        effects->paintScreen(mask, region, data);
    }
}

void CubeEffect::rotateCube()
{
    QRect rect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
    m_rotationMatrix.setToIdentity();
    float internalCubeAngle = 360.0f / effects->numberOfDesktops();
    float zTranslate = zPosition + zoom;
    float cubeAngle = (float)((float)(effects->numberOfDesktops() - 2) / (float)effects->numberOfDesktops() * 180.0f);
    float point = rect.width() / 2 * tan(cubeAngle * 0.5f * M_PI / 180.0f);
    /* Animations */
    if (animationState == AnimationState::Start) {
        zTranslate *= timeLine.value();
    } else if (animationState == AnimationState::Stop) {
        currentAngle = startAngle * (1.0 - timeLine.value());
        zTranslate *= (1.0 - timeLine.value());
    } else if (animationState != AnimationState::None) {
        /* Left or right */
        float endAngle = animationState == AnimationState::Right ? internalCubeAngle : -internalCubeAngle;
        currentAngle = startAngle + timeLine.value() * (endAngle - startAngle);
        frontDesktop = startFrontDesktop;
    }
    /* Switching to next desktop: either by mouse or due to animation */
    if (currentAngle > internalCubeAngle * 0.5f) {
        currentAngle -= internalCubeAngle;
        frontDesktop--;
        if (frontDesktop < 1) {
            frontDesktop = effects->numberOfDesktops();
        }
    }
    if (currentAngle < -internalCubeAngle * 0.5f) {
        currentAngle += internalCubeAngle;
        frontDesktop++;
        if (frontDesktop > effects->numberOfDesktops()) {
            frontDesktop = 1;
        }
    }
    /* Vertical animations */
    if (verticalAnimationState != VerticalAnimationState::None) {
        float verticalEndAngle = 0.0;
        if (verticalAnimationState == VerticalAnimationState::Upwards && verticalStartAngle >= 0.0) {
            verticalEndAngle = 90.0;
        }
        if (verticalAnimationState == VerticalAnimationState::Downwards && verticalStartAngle <= 0.0) {
            verticalEndAngle = -90.0;
        }
        // This also handles the "VerticalAnimationState::Stop" correctly, since it has endAngle = 0.0
        verticalCurrentAngle = verticalStartAngle + verticalTimeLine.value() * (verticalEndAngle - verticalStartAngle);
    }
    /* Updating rotation matrix */
    if (verticalAnimationState != VerticalAnimationState::None || verticalCurrentAngle != 0.0f) {
        m_rotationMatrix.translate(rect.width() / 2, rect.height() / 2, -point - zTranslate);
        m_rotationMatrix.rotate(verticalCurrentAngle, 1.0, 0.0, 0.0);
        m_rotationMatrix.translate(-rect.width() / 2, -rect.height() / 2, point + zTranslate);
    }
    if (animationState != AnimationState::None || currentAngle != 0.0f) {
        m_rotationMatrix.translate(rect.width() / 2, rect.height() / 2, -point - zTranslate);
        m_rotationMatrix.rotate(currentAngle, 0.0, 1.0, 0.0);
        m_rotationMatrix.translate(-rect.width() / 2, -rect.height() / 2, point + zTranslate);
    }
}

void CubeEffect::paintCube(int mask, QRegion region, ScreenPaintData& data)
{
    QRect rect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
    float internalCubeAngle = 360.0f / effects->numberOfDesktops();
    cube_painting = true;
    float zTranslate = zPosition + zoom;
    if (animationState == AnimationState::Start) {
        zTranslate *= timeLine.value();
    } else if (animationState == AnimationState::Stop) {
        zTranslate *= (1.0 - timeLine.value());
    }

    // Rotation of the cube
    float cubeAngle = (float)((float)(effects->numberOfDesktops() - 2) / (float)effects->numberOfDesktops() * 180.0f);
    float point = rect.width() / 2 * tan(cubeAngle * 0.5f * M_PI / 180.0f);

    for (int i = 0; i < effects->numberOfDesktops(); i++) {
        // start painting the cube
        painting_desktop = (i + frontDesktop) % effects->numberOfDesktops();
        if (painting_desktop == 0) {
            painting_desktop = effects->numberOfDesktops();
        }
        QMatrix4x4 matrix;
        matrix.translate(0, 0, -zTranslate);
        const QVector3D origin(rect.width() / 2, 0.0, -point);
        matrix.translate(origin);
        matrix.rotate(internalCubeAngle * i, 0, 1, 0);
        matrix.translate(-origin);
        m_currentFaceMatrix = matrix;
        effects->paintScreen(mask, region, data);
    }
    cube_painting = false;
    painting_desktop = effects->currentDesktop();
}

void CubeEffect::paintCap(bool frontFirst, float zOffset, const QMatrix4x4 &projection)
{
    if ((!paintCaps) || effects->numberOfDesktops() <= 2)
        return;
    GLenum firstCull = frontFirst ? GL_FRONT : GL_BACK;
    GLenum secondCull = frontFirst ? GL_BACK : GL_FRONT;
    const QRect rect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());

    // create the VBO if not yet created
    if (!m_cubeCapBuffer) {
        switch(mode) {
        case Cube:
            paintCubeCap();
            break;
        case Cylinder:
            paintCylinderCap();
            break;
        case Sphere:
            paintSphereCap();
            break;
        default:
            // impossible
            break;
        }
    }

    QMatrix4x4 capMvp;
    QMatrix4x4 capMatrix;
    capMatrix.translate(rect.width() / 2, 0.0, zOffset);
    capMatrix.rotate((1 - frontDesktop) * 360.0f / effects->numberOfDesktops(), 0.0, 1.0, 0.0);
    capMatrix.translate(0.0, rect.height(), 0.0);
    if (mode == Sphere) {
        capMatrix.scale(1.0, -1.0, 1.0);
    }

    bool capShader = false;
    if (effects->compositingType() == OpenGL2Compositing && m_capShader && m_capShader->isValid()) {
        capShader = true;
        ShaderManager::instance()->pushShader(m_capShader);
        float opacity = cubeOpacity;
        if (animationState == AnimationState::Start) {
            opacity *= timeLine.value();
        } else if (animationState == AnimationState::Stop) {
            opacity *= (1.0 - timeLine.value());
        }
        m_capShader->setUniform("u_opacity", opacity);
        m_capShader->setUniform("u_mirror", 1);
        if (reflectionPainting) {
            capMvp = projection * m_reflectionMatrix * m_rotationMatrix;
        } else {
            capMvp = projection * m_rotationMatrix;
        }
        m_capShader->setUniform(GLShader::ModelViewProjectionMatrix, capMvp * capMatrix);
        m_capShader->setUniform("u_untextured", texturedCaps ? 0 : 1);
        if (texturedCaps && effects->numberOfDesktops() > 3 && capTexture) {
            capTexture->bind();
        }
    }

    glEnable(GL_BLEND);
    glCullFace(firstCull);
    m_cubeCapBuffer->render(GL_TRIANGLES);

    if (mode == Sphere) {
        capMatrix.scale(1.0, -1.0, 1.0);
    }
    capMatrix.translate(0.0, -rect.height(), 0.0);
    if (capShader) {
        m_capShader->setUniform(GLShader::ModelViewProjectionMatrix, capMvp * capMatrix);
        m_capShader->setUniform("u_mirror", 0);
    }
    glCullFace(secondCull);
    m_cubeCapBuffer->render(GL_TRIANGLES);
    glDisable(GL_BLEND);

    if (capShader) {
        ShaderManager::instance()->popShader();
        if (texturedCaps && effects->numberOfDesktops() > 3 && capTexture) {
            capTexture->unbind();
        }
    }
}

void CubeEffect::paintCubeCap()
{
    QRect rect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
    float cubeAngle = (float)((float)(effects->numberOfDesktops() - 2) / (float)effects->numberOfDesktops() * 180.0f);
    float z = rect.width() / 2 * tan(cubeAngle * 0.5f * M_PI / 180.0f);
    float zTexture = rect.width() / 2 * tan(45.0f * M_PI / 180.0f);
    float angle = 360.0f / effects->numberOfDesktops();
    bool texture = texturedCaps && effects->numberOfDesktops() > 3 && capTexture;
    QVector<float> verts;
    QVector<float> texCoords;
    for (int i = 0; i < effects->numberOfDesktops(); i++) {
        int triangleRows = effects->numberOfDesktops() * 5;
        float zTriangleDistance = z / (float)triangleRows;
        float widthTriangle = tan(angle * 0.5 * M_PI / 180.0) * zTriangleDistance;
        float currentWidth = 0.0;
        float cosValue = cos(i * angle  * M_PI / 180.0);
        float sinValue = sin(i * angle  * M_PI / 180.0);
        for (int j = 0; j < triangleRows; j++) {
            float previousWidth = currentWidth;
            currentWidth = tan(angle * 0.5 * M_PI / 180.0) * zTriangleDistance * (j + 1);
            int evenTriangles = 0;
            int oddTriangles = 0;
            for (int k = 0; k < floor(currentWidth / widthTriangle * 2 - 1 + 0.5f); k++) {
                float x1 = -previousWidth;
                float x2 = -currentWidth;
                float x3 = 0.0;
                float z1 = 0.0;
                float z2 = 0.0;
                float z3 = 0.0;
                if (k % 2 == 0) {
                    x1 += evenTriangles * widthTriangle * 2;
                    x2 += evenTriangles * widthTriangle * 2;
                    x3 = x2 + widthTriangle * 2;
                    z1 = j * zTriangleDistance;
                    z2 = (j + 1) * zTriangleDistance;
                    z3 = (j + 1) * zTriangleDistance;
                    float xRot = cosValue * x1 - sinValue * z1;
                    float zRot = sinValue * x1 + cosValue * z1;
                    x1 = xRot;
                    z1 = zRot;
                    xRot = cosValue * x2 - sinValue * z2;
                    zRot = sinValue * x2 + cosValue * z2;
                    x2 = xRot;
                    z2 = zRot;
                    xRot = cosValue * x3 - sinValue * z3;
                    zRot = sinValue * x3 + cosValue * z3;
                    x3 = xRot;
                    z3 = zRot;
                    evenTriangles++;
                } else {
                    x1 += oddTriangles * widthTriangle * 2;
                    x2 += (oddTriangles + 1) * widthTriangle * 2;
                    x3 = x1 + widthTriangle * 2;
                    z1 = j * zTriangleDistance;
                    z2 = (j + 1) * zTriangleDistance;
                    z3 = j * zTriangleDistance;
                    float xRot = cosValue * x1 - sinValue * z1;
                    float zRot = sinValue * x1 + cosValue * z1;
                    x1 = xRot;
                    z1 = zRot;
                    xRot = cosValue * x2 - sinValue * z2;
                    zRot = sinValue * x2 + cosValue * z2;
                    x2 = xRot;
                    z2 = zRot;
                    xRot = cosValue * x3 - sinValue * z3;
                    zRot = sinValue * x3 + cosValue * z3;
                    x3 = xRot;
                    z3 = zRot;
                    oddTriangles++;
                }
                float texX1 = 0.0;
                float texX2 = 0.0;
                float texX3 = 0.0;
                float texY1 = 0.0;
                float texY2 = 0.0;
                float texY3 = 0.0;
                if (texture) {
                    if (capTexture->isYInverted()) {
                        texX1 = x1 / (rect.width()) + 0.5;
                        texY1 = 0.5 + z1 / zTexture * 0.5;
                        texX2 = x2 / (rect.width()) + 0.5;
                        texY2 = 0.5 + z2 / zTexture * 0.5;
                        texX3 = x3 / (rect.width()) + 0.5;
                        texY3 = 0.5 + z3 / zTexture * 0.5;
                        texCoords << texX1 << texY1;
                    } else {
                        texX1 = x1 / (rect.width()) + 0.5;
                        texY1 = 0.5 - z1 / zTexture * 0.5;
                        texX2 = x2 / (rect.width()) + 0.5;
                        texY2 = 0.5 - z2 / zTexture * 0.5;
                        texX3 = x3 / (rect.width()) + 0.5;
                        texY3 = 0.5 - z3 / zTexture * 0.5;
                        texCoords << texX1 << texY1;
                    }
                }
                verts << x1 << 0.0 << z1;
                if (texture) {
                    texCoords << texX2 << texY2;
                }
                verts << x2 << 0.0 << z2;
                if (texture) {
                    texCoords << texX3 << texY3;
                }
                verts << x3 << 0.0 << z3;
            }
        }
    }
    delete m_cubeCapBuffer;
    m_cubeCapBuffer = new GLVertexBuffer(GLVertexBuffer::Static);
    m_cubeCapBuffer->setData(verts.count() / 3, 3, verts.constData(), texture ? texCoords.constData() : nullptr);
}

void CubeEffect::paintCylinderCap()
{
    QRect rect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
    float cubeAngle = (float)((float)(effects->numberOfDesktops() - 2) / (float)effects->numberOfDesktops() * 180.0f);

    float radian = (cubeAngle * 0.5) * M_PI / 180;
    float radius = (rect.width() * 0.5) * tan(radian);
    float segment = radius / 30.0f;

    bool texture = texturedCaps && effects->numberOfDesktops() > 3 && capTexture;
    QVector<float> verts;
    QVector<float> texCoords;
    for (int i = 1; i <= 30; i++) {
        int steps =  72;
        for (int j = 0; j <= steps; j++) {
            const float azimuthAngle = (j * (360.0f / steps)) * M_PI / 180.0f;
            const float azimuthAngle2 = ((j + 1) * (360.0f / steps)) * M_PI / 180.0f;
            const float x1 = segment * (i - 1) * sin(azimuthAngle);
            const float x2 = segment * i * sin(azimuthAngle);
            const float x3 = segment * (i - 1) * sin(azimuthAngle2);
            const float x4 = segment * i * sin(azimuthAngle2);
            const float z1 = segment * (i - 1) * cos(azimuthAngle);
            const float z2 = segment * i * cos(azimuthAngle);
            const float z3 = segment * (i - 1) * cos(azimuthAngle2);
            const float z4 = segment * i * cos(azimuthAngle2);
            if (texture) {
                if (capTexture->isYInverted()) {
                    texCoords << (radius + x1) / (radius * 2.0f) << (z1 + radius) / (radius * 2.0f);
                    texCoords << (radius + x2) / (radius * 2.0f) << (z2 + radius) / (radius * 2.0f);
                    texCoords << (radius + x3) / (radius * 2.0f) << (z3 + radius) / (radius * 2.0f);
                    texCoords << (radius + x4) / (radius * 2.0f) << (z4 + radius) / (radius * 2.0f);
                    texCoords << (radius + x3) / (radius * 2.0f) << (z3 + radius) / (radius * 2.0f);
                    texCoords << (radius + x2) / (radius * 2.0f) << (z2 + radius) / (radius * 2.0f);
                } else {
                    texCoords << (radius + x1) / (radius * 2.0f) << 1.0f - (z1 + radius) / (radius * 2.0f);
                    texCoords << (radius + x2) / (radius * 2.0f) << 1.0f - (z2 + radius) / (radius * 2.0f);
                    texCoords << (radius + x3) / (radius * 2.0f) << 1.0f - (z3 + radius) / (radius * 2.0f);
                    texCoords << (radius + x4) / (radius * 2.0f) << 1.0f - (z4 + radius) / (radius * 2.0f);
                    texCoords << (radius + x3) / (radius * 2.0f) << 1.0f - (z3 + radius) / (radius * 2.0f);
                    texCoords << (radius + x2) / (radius * 2.0f) << 1.0f - (z2 + radius) / (radius * 2.0f);
                }
            }
            verts << x1 << 0.0 << z1;
            verts << x2 << 0.0 << z2;
            verts << x3 << 0.0 << z3;
            verts << x4 << 0.0 << z4;
            verts << x3 << 0.0 << z3;
            verts << x2 << 0.0 << z2;
        }
    }
    delete m_cubeCapBuffer;
    m_cubeCapBuffer = new GLVertexBuffer(GLVertexBuffer::Static);
    m_cubeCapBuffer->setData(verts.count() / 3, 3, verts.constData(), texture ? texCoords.constData() : nullptr);
}

void CubeEffect::paintSphereCap()
{
    QRect rect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
    float cubeAngle = (float)((float)(effects->numberOfDesktops() - 2) / (float)effects->numberOfDesktops() * 180.0f);
    float zTexture = rect.width() / 2 * tan(45.0f * M_PI / 180.0f);
    float radius = (rect.width() * 0.5) / cos(cubeAngle * 0.5 * M_PI / 180.0);
    float angle = acos((rect.height() * 0.5) / radius) * 180.0 / M_PI;
    angle /= 30;
    bool texture = texturedCaps && effects->numberOfDesktops() > 3 && capTexture;
    QVector<float> verts;
    QVector<float> texCoords;
    for (int i = 0; i < 30; i++) {
        float topAngle = angle * i * M_PI / 180.0;
        float bottomAngle = angle * (i + 1) * M_PI / 180.0;
        float yTop = (rect.height() * 0.5 - radius * cos(topAngle));
        yTop *= (1.0f-capDeformationFactor);
        float yBottom = rect.height() * 0.5 - radius * cos(bottomAngle);
        yBottom *= (1.0f - capDeformationFactor);
        for (int j = 0; j < 36; j++) {
            const float x1 = radius * sin(topAngle) * sin((90.0 + j * 10.0) * M_PI / 180.0);
            const float z1 = radius * sin(topAngle) * cos((90.0 + j * 10.0) * M_PI / 180.0);
            const float x2 = radius * sin(bottomAngle) * sin((90.0 + j * 10.0) * M_PI / 180.00);
            const float z2 = radius * sin(bottomAngle) * cos((90.0 + j * 10.0) * M_PI / 180.0);
            const float x3 = radius * sin(bottomAngle) * sin((90.0 + (j + 1) * 10.0) * M_PI / 180.0);
            const float z3 = radius * sin(bottomAngle) * cos((90.0 + (j + 1) * 10.0) * M_PI / 180.0);
            const float x4 = radius * sin(topAngle) * sin((90.0 + (j + 1) * 10.0) * M_PI / 180.0);
            const float z4 = radius * sin(topAngle) * cos((90.0 + (j + 1) * 10.0) * M_PI / 180.0);
            if (texture) {
                if (capTexture->isYInverted()) {
                    texCoords << x4 / (rect.width()) + 0.5 << 0.5 + z4 / zTexture * 0.5;
                    texCoords << x1 / (rect.width()) + 0.5 << 0.5 + z1 / zTexture * 0.5;
                    texCoords << x2 / (rect.width()) + 0.5 << 0.5 + z2 / zTexture * 0.5;
                    texCoords << x2 / (rect.width()) + 0.5 << 0.5 + z2 / zTexture * 0.5;
                    texCoords << x3 / (rect.width()) + 0.5 << 0.5 + z3 / zTexture * 0.5;
                    texCoords << x4 / (rect.width()) + 0.5 << 0.5 + z4 / zTexture * 0.5;
                } else {
                    texCoords << x4 / (rect.width()) + 0.5 << 0.5 - z4 / zTexture * 0.5;
                    texCoords << x1 / (rect.width()) + 0.5 << 0.5 - z1 / zTexture * 0.5;
                    texCoords << x2 / (rect.width()) + 0.5 << 0.5 - z2 / zTexture * 0.5;
                    texCoords << x2 / (rect.width()) + 0.5 << 0.5 - z2 / zTexture * 0.5;
                    texCoords << x3 / (rect.width()) + 0.5 << 0.5 - z3 / zTexture * 0.5;
                    texCoords << x4 / (rect.width()) + 0.5 << 0.5 - z4 / zTexture * 0.5;
                }
            }
            verts << x4 << yTop    << z4;
            verts << x1 << yTop    << z1;
            verts << x2 << yBottom << z2;
            verts << x2 << yBottom << z2;
            verts << x3 << yBottom << z3;
            verts << x4 << yTop    << z4;
        }
    }
    delete m_cubeCapBuffer;
    m_cubeCapBuffer = new GLVertexBuffer(GLVertexBuffer::Static);
    m_cubeCapBuffer->setData(verts.count() / 3, 3, verts.constData(), texture ? texCoords.constData() : nullptr);
}

void CubeEffect::postPaintScreen()
{
    effects->postPaintScreen();
    if (!activated)
        return;

    bool animation = (animationState != AnimationState::None || verticalAnimationState != VerticalAnimationState::None);
    if (animationState != AnimationState::None && timeLine.done()) {
        /* An animation have just finished! */
        if (animationState == AnimationState::Stop) {
            /* If the stop animation is finished, we're done */
            if (keyboard_grab)
                effects->ungrabKeyboard();
            keyboard_grab = false;
            effects->stopMouseInterception(this);
            effects->setCurrentDesktop(frontDesktop);
            effects->setActiveFullScreenEffect(nullptr);
            delete m_cubeCapBuffer;
            m_cubeCapBuffer = nullptr;
            if (desktopNameFrame)
                desktopNameFrame->free();
            activated = false;
            // User can press Esc several times, and several Stop animations can be added to queue. We don't want it
            animationState = AnimationState::None;
            animations.clear();
            verticalAnimationState = VerticalAnimationState::None;
            verticalAnimations.clear();
        } else {
            if (!animations.empty())
                startAnimation(animations.dequeue());
            else
                animationState = AnimationState::None;
        }
    }
    /* Vertical animation have finished */
    if (verticalAnimationState != VerticalAnimationState::None && verticalTimeLine.done()) {
        if (!verticalAnimations.empty()) {
            startVerticalAnimation(verticalAnimations.dequeue());
        } else {
            verticalAnimationState = VerticalAnimationState::None;
        }
    }
    /* Repaint if there is any animation */
    if (animation) {
        effects->addRepaintFull();
    }
}

void CubeEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    if (activated) {
        if (cube_painting) {
            if (mode == Cylinder || mode == Sphere) {
                int leftDesktop = frontDesktop - 1;
                int rightDesktop = frontDesktop + 1;
                if (leftDesktop == 0)
                    leftDesktop = effects->numberOfDesktops();
                if (rightDesktop > effects->numberOfDesktops())
                    rightDesktop = 1;
                if (painting_desktop == frontDesktop)
                    data.quads = data.quads.makeGrid(40);
                else if (painting_desktop == leftDesktop || painting_desktop == rightDesktop)
                    data.quads = data.quads.makeGrid(100);
                else
                    data.quads = data.quads.makeGrid(250);
            }
            if (w->isOnDesktop(painting_desktop)) {
                QRect rect = effects->clientArea(FullArea, activeScreen, painting_desktop);
                if (w->x() < rect.x()) {
                    data.quads = data.quads.splitAtX(-w->x());
                }
                if (w->x() + w->width() > rect.x() + rect.width()) {
                    data.quads = data.quads.splitAtX(rect.width() - w->x());
                }
                if (w->y() < rect.y()) {
                    data.quads = data.quads.splitAtY(-w->y());
                }
                if (w->y() + w->height() > rect.y() + rect.height()) {
                    data.quads = data.quads.splitAtY(rect.height() - w->y());
                }
                if (useZOrdering && !w->isDesktop() && !w->isDock() && !w->isOnAllDesktops())
                    data.setTransformed();
                w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
            } else {
                // check for windows belonging to the previous desktop
                int prev_desktop = painting_desktop - 1;
                if (prev_desktop == 0)
                    prev_desktop = effects->numberOfDesktops();
                if (w->isOnDesktop(prev_desktop) && mode == Cube && !useZOrdering) {
                    QRect rect = effects->clientArea(FullArea, activeScreen, prev_desktop);
                    if (w->x() + w->width() > rect.x() + rect.width()) {
                        w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
                        data.quads = data.quads.splitAtX(rect.width() - w->x());
                        if (w->y() < rect.y()) {
                            data.quads = data.quads.splitAtY(-w->y());
                        }
                        if (w->y() + w->height() > rect.y() + rect.height()) {
                            data.quads = data.quads.splitAtY(rect.height() - w->y());
                        }
                        data.setTransformed();
                        effects->prePaintWindow(w, data, time);
                        return;
                    }
                }
                // check for windows belonging to the next desktop
                int next_desktop = painting_desktop + 1;
                if (next_desktop > effects->numberOfDesktops())
                    next_desktop = 1;
                if (w->isOnDesktop(next_desktop) && mode == Cube && !useZOrdering) {
                    QRect rect = effects->clientArea(FullArea, activeScreen, next_desktop);
                    if (w->x() < rect.x()) {
                        w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
                        data.quads = data.quads.splitAtX(-w->x());
                        if (w->y() < rect.y()) {
                            data.quads = data.quads.splitAtY(-w->y());
                        }
                        if (w->y() + w->height() > rect.y() + rect.height()) {
                            data.quads = data.quads.splitAtY(rect.height() - w->y());
                        }
                        data.setTransformed();
                        effects->prePaintWindow(w, data, time);
                        return;
                    }
                }
                w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
            }
        }
    }
    effects->prePaintWindow(w, data, time);
}

void CubeEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    ShaderManager *shaderManager = ShaderManager::instance();
    if (activated && cube_painting) {
        region= infiniteRegion(); // we need to explicitly prevent any clipping, bug #325432
        //qCDebug(KWINEFFECTS) << w->caption();
        float opacity = cubeOpacity;
        if (animationState == AnimationState::Start) {
            opacity = 1.0 - (1.0 - opacity) * timeLine.value();
            if (reflectionPainting)
                opacity = 0.5 + (cubeOpacity - 0.5) * timeLine.value();
            // fade in windows belonging to different desktops
            if (painting_desktop == effects->currentDesktop() && (!w->isOnDesktop(painting_desktop)))
                opacity = timeLine.value() * cubeOpacity;
        } else if (animationState == AnimationState::Stop) {
            opacity = 1.0 - (1.0 - opacity) * (1.0 - timeLine.value());
            if (reflectionPainting)
                opacity = 0.5 + (cubeOpacity - 0.5) * (1.0 - timeLine.value());
            // fade out windows belonging to different desktops
            if (painting_desktop == effects->currentDesktop() && (!w->isOnDesktop(painting_desktop)))
                opacity = cubeOpacity * (1.0 - timeLine.value());
        }
        // z-Ordering
        if (!w->isDesktop() && !w->isDock() && useZOrdering && !w->isOnAllDesktops()) {
            float zOrdering = (effects->stackingOrder().indexOf(w) + 1) * zOrderingFactor;
            if (animationState == AnimationState::Start) {
                zOrdering *= timeLine.value();
            } else if (animationState == AnimationState::Stop) {
                zOrdering *= (1.0 - timeLine.value());
            }
            data.translate(0.0, 0.0, zOrdering);
        }
        // check for windows belonging to the previous desktop
        int prev_desktop = painting_desktop - 1;
        if (prev_desktop == 0)
            prev_desktop = effects->numberOfDesktops();
        int next_desktop = painting_desktop + 1;
        if (next_desktop > effects->numberOfDesktops())
            next_desktop = 1;
        if (w->isOnDesktop(prev_desktop) && (mask & PAINT_WINDOW_TRANSFORMED)) {
            QRect rect = effects->clientArea(FullArea, activeScreen, prev_desktop);
            WindowQuadList new_quads;
            foreach (const WindowQuad & quad, data.quads) {
                if (quad.right() > rect.width() - w->x()) {
                    new_quads.append(quad);
                }
            }
            data.quads = new_quads;
            data.setXTranslation(-rect.width());
        }
        if (w->isOnDesktop(next_desktop) && (mask & PAINT_WINDOW_TRANSFORMED)) {
            QRect rect = effects->clientArea(FullArea, activeScreen, next_desktop);
            WindowQuadList new_quads;
            foreach (const WindowQuad & quad, data.quads) {
                if (w->x() + quad.right() <= rect.x()) {
                    new_quads.append(quad);
                }
            }
            data.quads = new_quads;
            data.setXTranslation(rect.width());
        }
        QRect rect = effects->clientArea(FullArea, activeScreen, painting_desktop);

        if (animationState == AnimationState::Start || animationState == AnimationState::Stop) {
            // we have to change opacity values for fade in/out of windows which are shown on front-desktop
            if (prev_desktop == effects->currentDesktop() && w->x() < rect.x()) {
                if (animationState == AnimationState::Start) {
                    opacity = timeLine.value() * cubeOpacity;
                } else if (animationState == AnimationState::Stop) {
                    opacity = cubeOpacity * (1.0 - timeLine.value());
                }
            }
            if (next_desktop == effects->currentDesktop() && w->x() + w->width() > rect.x() + rect.width()) {
                if (animationState == AnimationState::Start) {
                    opacity = timeLine.value() * cubeOpacity;
                } else if (animationState == AnimationState::Stop) {
                    opacity = cubeOpacity * (1.0 - timeLine.value());
                }
            }
        }
        // HACK set opacity to 0.99 in case of fully opaque to ensure that windows are painted in correct sequence
        // bug #173214
        if (opacity > 0.99f)
            opacity = 0.99f;
        if (opacityDesktopOnly && !w->isDesktop())
            opacity = 0.99f;
        data.multiplyOpacity(opacity);

        if (w->isOnDesktop(painting_desktop) && w->x() < rect.x()) {
            WindowQuadList new_quads;
            foreach (const WindowQuad & quad, data.quads) {
                if (quad.right() > -w->x()) {
                    new_quads.append(quad);
                }
            }
            data.quads = new_quads;
        }
        if (w->isOnDesktop(painting_desktop) && w->x() + w->width() > rect.x() + rect.width()) {
            WindowQuadList new_quads;
            foreach (const WindowQuad & quad, data.quads) {
                if (quad.right() <= rect.width() - w->x()) {
                    new_quads.append(quad);
                }
            }
            data.quads = new_quads;
        }
        if (w->y() < rect.y()) {
            WindowQuadList new_quads;
            foreach (const WindowQuad & quad, data.quads) {
                if (quad.bottom() > -w->y()) {
                    new_quads.append(quad);
                }
            }
            data.quads = new_quads;
        }
        if (w->y() + w->height() > rect.y() + rect.height()) {
            WindowQuadList new_quads;
            foreach (const WindowQuad & quad, data.quads) {
                if (quad.bottom() <= rect.height() - w->y()) {
                    new_quads.append(quad);
                }
            }
            data.quads = new_quads;
        }
        GLShader *currentShader = nullptr;
        if (mode == Cylinder) {
            shaderManager->pushShader(cylinderShader);
            cylinderShader->setUniform("xCoord", (float)w->x());
            cylinderShader->setUniform("cubeAngle", (effects->numberOfDesktops() - 2) / (float)effects->numberOfDesktops() * 90.0f);
            float factor = 0.0f;
            if (animationState == AnimationState::Start) {
                factor = 1.0f - timeLine.value();
            } else if (animationState == AnimationState::Stop) {
                factor = timeLine.value();
            }
            cylinderShader->setUniform("timeLine", factor);
            currentShader = cylinderShader;
        }
        if (mode == Sphere) {
            shaderManager->pushShader(sphereShader);
            sphereShader->setUniform("u_offset", QVector2D(w->x(), w->y()));
            sphereShader->setUniform("cubeAngle", (effects->numberOfDesktops() - 2) / (float)effects->numberOfDesktops() * 90.0f);
            float factor = 0.0f;
            if (animationState == AnimationState::Start) {
                factor = 1.0f - timeLine.value();
            } else if (animationState == AnimationState::Stop) {
                factor = timeLine.value();
            }
            sphereShader->setUniform("timeLine", factor);
            currentShader = sphereShader;
        }
        if (currentShader) {
            data.shader = currentShader;
        }
        data.setProjectionMatrix(data.screenProjectionMatrix());
        if (reflectionPainting) {
            data.setModelViewMatrix(m_reflectionMatrix * m_rotationMatrix * m_currentFaceMatrix);
        } else {
            data.setModelViewMatrix(m_rotationMatrix * m_currentFaceMatrix);
        }
    }
    effects->paintWindow(w, mask, region, data);
    if (activated && cube_painting) {
        if (mode == Cylinder || mode == Sphere) {
            shaderManager->popShader();
        }
        if (w->isDesktop() && effects->numScreens() > 1 && paintCaps) {
            QRect rect = effects->clientArea(FullArea, activeScreen, painting_desktop);
            QRegion paint = QRegion(rect);
            for (int i = 0; i < effects->numScreens(); i++) {
                if (i == w->screen())
                    continue;
                paint = paint.subtracted(QRegion(effects->clientArea(ScreenArea, i, painting_desktop)));
            }
            paint = paint.subtracted(QRegion(w->geometry()));
            // in case of free area in multiscreen setup fill it with cap color
            if (!paint.isEmpty()) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                QVector<float> verts;
                float quadSize = 0.0f;
                int leftDesktop = frontDesktop - 1;
                int rightDesktop = frontDesktop + 1;
                if (leftDesktop == 0)
                    leftDesktop = effects->numberOfDesktops();
                if (rightDesktop > effects->numberOfDesktops())
                    rightDesktop = 1;
                if (painting_desktop == frontDesktop)
                    quadSize = 100.0f;
                else if (painting_desktop == leftDesktop || painting_desktop == rightDesktop)
                    quadSize = 150.0f;
                else
                    quadSize = 250.0f;
                for (const QRect &paintRect : paint) {
                    for (int i = 0; i <= (paintRect.height() / quadSize); i++) {
                        for (int j = 0; j <= (paintRect.width() / quadSize); j++) {
                            verts << qMin(paintRect.x() + (j + 1)*quadSize, (float)paintRect.x() + paintRect.width()) << paintRect.y() + i*quadSize;
                            verts << paintRect.x() + j*quadSize << paintRect.y() + i*quadSize;
                            verts << paintRect.x() + j*quadSize << qMin(paintRect.y() + (i + 1)*quadSize, (float)paintRect.y() + paintRect.height());
                            verts << paintRect.x() + j*quadSize << qMin(paintRect.y() + (i + 1)*quadSize, (float)paintRect.y() + paintRect.height());
                            verts << qMin(paintRect.x() + (j + 1)*quadSize, (float)paintRect.x() + paintRect.width()) << qMin(paintRect.y() + (i + 1)*quadSize, (float)paintRect.y() + paintRect.height());
                            verts << qMin(paintRect.x() + (j + 1)*quadSize, (float)paintRect.x() + paintRect.width()) << paintRect.y() + i*quadSize;
                        }
                    }
                }
                bool capShader = false;
                if (effects->compositingType() == OpenGL2Compositing && m_capShader && m_capShader->isValid()) {
                    capShader = true;
                    ShaderManager::instance()->pushShader(m_capShader);
                    m_capShader->setUniform("u_mirror", 0);
                    m_capShader->setUniform("u_untextured", 1);
                    QMatrix4x4 mvp = data.screenProjectionMatrix();
                    if (reflectionPainting) {
                        mvp = mvp * m_reflectionMatrix * m_rotationMatrix * m_currentFaceMatrix;
                    } else {
                        mvp = mvp * m_rotationMatrix * m_currentFaceMatrix;
                    }
                    m_capShader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
                }
                GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
                vbo->reset();
                QColor color = capColor;
                capColor.setAlphaF(cubeOpacity);
                vbo->setColor(color);
                vbo->setData(verts.size() / 2, 2, verts.constData(), nullptr);
                if (!capShader || mode == Cube) {
                    // TODO: use sphere and cylinder shaders
                    vbo->render(GL_TRIANGLES);
                }
                if (capShader) {
                    ShaderManager::instance()->popShader();
                }
                glDisable(GL_BLEND);
            }
        }
    }
}

bool CubeEffect::borderActivated(ElectricBorder border)
{
    if (!borderActivate.contains(border) &&
            !borderActivateCylinder.contains(border) &&
            !borderActivateSphere.contains(border))
        return false;
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return false;
    if (borderActivate.contains(border)) {
        if (!activated || (activated && mode == Cube))
            toggleCube();
        else
            return false;
    }
    if (borderActivateCylinder.contains(border)) {
        if (!activated || (activated && mode == Cylinder))
            toggleCylinder();
        else
            return false;
    }
    if (borderActivateSphere.contains(border)) {
        if (!activated || (activated && mode == Sphere))
            toggleSphere();
        else
            return false;
    }
    return true;
}

void CubeEffect::toggleCube()
{
    qCDebug(KWINEFFECTS) << "toggle cube";
    toggle(Cube);
}

void CubeEffect::toggleCylinder()
{
    qCDebug(KWINEFFECTS) << "toggle cylinder";
    if (!useShaders)
        useShaders = loadShader();
    if (useShaders)
        toggle(Cylinder);
}

void CubeEffect::toggleSphere()
{
    qCDebug(KWINEFFECTS) << "toggle sphere";
    if (!useShaders)
        useShaders = loadShader();
    if (useShaders)
        toggle(Sphere);
}

void CubeEffect::toggle(CubeMode newMode)
{
    if ((effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this) ||
            effects->numberOfDesktops() < 2)
        return;
    if (!activated) {
        mode = newMode;
        setActive(true);
    } else {
        setActive(false);
    }
}

void CubeEffect::grabbedKeyboardEvent(QKeyEvent* e)
{
    // If either stop is running or is scheduled - ignore all events
    if ((!animations.isEmpty() && animations.last() == AnimationState::Stop) || animationState == AnimationState::Stop) {
        return;
    }
    // taken from desktopgrid.cpp
    if (e->type() == QEvent::KeyPress) {
        // check for global shortcuts
        // HACK: keyboard grab disables the global shortcuts so we have to check for global shortcut (bug 156155)
        if (mode == Cube && cubeShortcut.contains(e->key() + e->modifiers())) {
            toggleCube();
            return;
        }
        if (mode == Cylinder && cylinderShortcut.contains(e->key() + e->modifiers())) {
            toggleCylinder();
            return;
        }
        if (mode == Sphere && sphereShortcut.contains(e->key() + e->modifiers())) {
            toggleSphere();
            return;
        }

        int desktop = -1;
        // switch by F<number> or just <number>
        if (e->key() >= Qt::Key_F1 && e->key() <= Qt::Key_F35)
            desktop = e->key() - Qt::Key_F1 + 1;
        else if (e->key() >= Qt::Key_0 && e->key() <= Qt::Key_9)
            desktop = e->key() == Qt::Key_0 ? 10 : e->key() - Qt::Key_0;
        if (desktop != -1) {
            if (desktop <= effects->numberOfDesktops()) {
                // we have to rotate to chosen desktop
                // and end effect when rotation finished
                rotateToDesktop(desktop);
                setActive(false);
            }
            return;
        }

        int key = e->key();
        if (invertKeys) {
            if (key == Qt::Key_Left)
                key = Qt::Key_Right;
            else if (key == Qt::Key_Right)
                key = Qt::Key_Left;
            else if (key == Qt::Key_Up)
                key = Qt::Key_Down;
            else if (key == Qt::Key_Down)
                key = Qt::Key_Up;
        }

        switch(key) {
        // wrap only on autorepeat
        case Qt::Key_Left:
            qCDebug(KWINEFFECTS) << "left";
            if (animations.count() < effects->numberOfDesktops())
                animations.enqueue(AnimationState::Left);
            break;
        case Qt::Key_Right:
            qCDebug(KWINEFFECTS) << "right";
            if (animations.count() < effects->numberOfDesktops())
                animations.enqueue(AnimationState::Right);
            break;
        case Qt::Key_Up:
            qCDebug(KWINEFFECTS) << "up";
            verticalAnimations.enqueue(VerticalAnimationState::Upwards);
            break;
        case Qt::Key_Down:
            qCDebug(KWINEFFECTS) << "down";
            verticalAnimations.enqueue(VerticalAnimationState::Downwards);
        break;
        case Qt::Key_Escape:
            rotateToDesktop(effects->currentDesktop());
            setActive(false);
            return;
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Space:
            setActive(false);
            return;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoom -= 10.0;
            zoom = qMax(-zPosition, zoom);
            rotateCube();
            break;
        case Qt::Key_Minus:
            zoom += 10.0f;
            rotateCube();
            break;
        default:
            break;
        }
    }
    effects->addRepaintFull();
}

void CubeEffect::rotateToDesktop(int desktop)
{
    // all scheduled animations will be removed as a speed up
    animations.clear();
    verticalAnimations.clear();
    // we want only startAnimation to finish gracefully
    // all the others can be interrupted
    if (animationState != AnimationState::Start) {
        animationState = AnimationState::None;
    }
    verticalAnimationState = VerticalAnimationState::None;
    // find the fastest rotation path from frontDesktop to desktop
    int rightRotations = frontDesktop - desktop;
    if (rightRotations < 0) {
        rightRotations += effects->numberOfDesktops();
    }
    int leftRotations = desktop - frontDesktop;
    if (leftRotations < 0) {
        leftRotations += effects->numberOfDesktops();
    }
    if (leftRotations <= rightRotations) {
        for (int i = 0; i < leftRotations; i++) {
            animations.enqueue(AnimationState::Left);
        }
    } else {
        for (int i = 0; i < rightRotations; i++) {
            animations.enqueue(AnimationState::Right);
        }
    }
    // we want the face of desktop to appear, it might need also vertical animation
    if (verticalCurrentAngle > 0.0f) {
        verticalAnimations.enqueue(VerticalAnimationState::Downwards);
    }
    if (verticalCurrentAngle < 0.0f) {
        verticalAnimations.enqueue(VerticalAnimationState::Upwards);
    }
    /* Start immediately, so there is no pause:
     * during that pause, actual frontDesktop might change
     * if user moves his mouse fast, leading to incorrect desktop */
    if (animationState == AnimationState::None && !animations.empty()) {
        startAnimation(animations.dequeue());
    }
    if (verticalAnimationState == VerticalAnimationState::None && !verticalAnimations.empty()) {
        startVerticalAnimation(verticalAnimations.dequeue());
    }
}

void CubeEffect::setActive(bool active)
{
    foreach (CubeInsideEffect * inside, m_cubeInsideEffects) {
        inside->setActive(true);
    }
    if (active) {
        QString capPath = CubeConfig::capPath();
        if (texturedCaps && !capTexture && !capPath.isEmpty()) {
            QFutureWatcher<QImage> *watcher = new QFutureWatcher<QImage>(this);
            connect(watcher, &QFutureWatcher<QImage>::finished, this, &CubeEffect::slotCubeCapLoaded);
            watcher->setFuture(QtConcurrent::run(this, &CubeEffect::loadCubeCap, capPath));
        }
        QString wallpaperPath = CubeConfig::wallpaper().toLocalFile();
        if (!wallpaper && !wallpaperPath.isEmpty()) {
            QFutureWatcher<QImage> *watcher = new QFutureWatcher<QImage>(this);
            connect(watcher, &QFutureWatcher<QImage>::finished, this, &CubeEffect::slotWallPaperLoaded);
            watcher->setFuture(QtConcurrent::run(this, &CubeEffect::loadWallPaper, wallpaperPath));
        }
        activated = true;
        activeScreen = effects->activeScreen();
        keyboard_grab = effects->grabKeyboard(this);
        effects->startMouseInterception(this, Qt::OpenHandCursor);
        frontDesktop = effects->currentDesktop();
        zoom = 0.0;
        zOrderingFactor = zPosition / (effects->stackingOrder().count() - 1);
        animations.enqueue(AnimationState::Start);
        animationState = AnimationState::None;
        verticalAnimationState = VerticalAnimationState::None;
        effects->setActiveFullScreenEffect(this);
        qCDebug(KWINEFFECTS) << "Cube is activated";
        currentAngle = 0.0;
        verticalCurrentAngle = 0.0;
        if (reflection) {
            QRect rect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
            float temporaryCoeff = float(rect.width()) / tan(M_PI / float(effects->numberOfDesktops()));
            mAddedHeightCoeff1 = sqrt(float(rect.height()) * float(rect.height()) + temporaryCoeff * temporaryCoeff);
            mAddedHeightCoeff2 = sqrt(float(rect.height()) * float(rect.height()) + float(rect.width()) * float(rect.width()) + temporaryCoeff * temporaryCoeff);
        }
        m_rotationMatrix.setToIdentity();
    } else {
        animations.enqueue(AnimationState::Stop);
    }
    effects->addRepaintFull();
}

void CubeEffect::windowInputMouseEvent(QEvent* e)
{
    if (!activated)
        return;
    if (tabBoxMode)
        return;
    if ((!animations.isEmpty() && animations.last() == AnimationState::Stop) || animationState == AnimationState::Stop)
        return;

    QMouseEvent *mouse = dynamic_cast< QMouseEvent* >(e);
    if (!mouse)
        return;

    static QPoint oldpos;
    static QElapsedTimer dblClckTime;
    static int dblClckCounter(0);
    if (mouse->type() == QEvent::MouseMove && mouse->buttons().testFlag(Qt::LeftButton)) {
        const QPoint pos = mouse->pos();
        QRect rect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
        bool repaint = false;
        // vertical movement only if there is not a rotation
        if (verticalAnimationState == VerticalAnimationState::None) {
            // display height corresponds to 180*
            int deltaY = pos.y() - oldpos.y();
            float deltaVerticalDegrees = (float)deltaY / rect.height() * 180.0f;
            if (invertMouse)
                verticalCurrentAngle += deltaVerticalDegrees;
            else
                verticalCurrentAngle -= deltaVerticalDegrees;
            // don't get too excited
            verticalCurrentAngle = qBound(-90.0f, verticalCurrentAngle, 90.0f);

            if (deltaVerticalDegrees != 0.0)
                repaint = true;
        }
        // horizontal movement only if there is not a rotation
        if (animationState == AnimationState::None) {
            // display width corresponds to sum of angles of the polyhedron
            int deltaX = oldpos.x() - pos.x();
            float deltaDegrees = (float)deltaX / rect.width() * 360.0f;
            if (deltaX == 0) {
                if (pos.x() == 0)
                    deltaDegrees = 5.0f;
                if (pos.x() == rect.width() - 1)
                    deltaDegrees = -5.0f;
            }
            if (invertMouse)
                currentAngle += deltaDegrees;
            else
                currentAngle -= deltaDegrees;
            if (deltaDegrees != 0.0)
                repaint = true;
        }
        if (repaint) {
            rotateCube();
            effects->addRepaintFull();
        }
        oldpos = pos;
    }

    else if (mouse->type() == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton) {
        oldpos = mouse->pos();
        if (dblClckTime.elapsed() > QApplication::doubleClickInterval())
            dblClckCounter = 0;
        if (!dblClckCounter)
            dblClckTime.start();
    }

    else if (mouse->type() == QEvent::MouseButtonRelease) {
        effects->defineCursor(Qt::OpenHandCursor);
        if (mouse->button() == Qt::LeftButton && ++dblClckCounter == 2) {
            dblClckCounter = 0;
            if (dblClckTime.elapsed() < QApplication::doubleClickInterval()) {
                setActive(false);
                return;
            }
        }
        else if (mouse->button() == Qt::XButton1) {
            if (animations.count() < effects->numberOfDesktops()) {
                if (invertMouse)
                    animations.enqueue(AnimationState::Right);
                else
                    animations.enqueue(AnimationState::Left);
            }
            effects->addRepaintFull();
        } else if (mouse->button() == Qt::XButton2) {
            if (animations.count() < effects->numberOfDesktops()) {
                if (invertMouse)
                    animations.enqueue(AnimationState::Left);
                else
                    animations.enqueue(AnimationState::Right);
            }
            effects->addRepaintFull();
        } else if (mouse->button() == Qt::RightButton || (mouse->button() == Qt::LeftButton && closeOnMouseRelease)) {
            setActive(false);
        }
    }
}

void CubeEffect::slotTabBoxAdded(int mode)
{
    if (activated)
        return;
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return;
    if (useForTabBox && mode == TabBoxDesktopListMode) {
        effects->refTabBox();
        tabBoxMode = true;
        setActive(true);
        rotateToDesktop(effects->currentTabBoxDesktop());
    }
}

void CubeEffect::slotTabBoxUpdated()
{
    if (activated) {
        rotateToDesktop(effects->currentTabBoxDesktop());
        effects->addRepaintFull();
    }
}

void CubeEffect::slotTabBoxClosed()
{
    if (activated) {
        effects->unrefTabBox();
        tabBoxMode = false;
        setActive(false);
    }
}

void CubeEffect::globalShortcutChanged(QAction *action, const QKeySequence &seq)
{
    if (action->objectName() == QStringLiteral("Cube")) {
        cubeShortcut.clear();
        cubeShortcut.append(seq);
    } else if (action->objectName() == QStringLiteral("Cylinder")) {
        cylinderShortcut.clear();
        cylinderShortcut.append(seq);
    } else if (action->objectName() == QStringLiteral("Sphere")) {
        sphereShortcut.clear();
        sphereShortcut.append(seq);
    }
}

void* CubeEffect::proxy()
{
    return &m_proxy;
}

void CubeEffect::registerCubeInsideEffect(CubeInsideEffect* effect)
{
    m_cubeInsideEffects.append(effect);
}

void CubeEffect::unregisterCubeInsideEffect(CubeInsideEffect* effect)
{
    m_cubeInsideEffects.removeAll(effect);
}

bool CubeEffect::isActive() const
{
    return activated && !effects->isScreenLocked();
}

} // namespace
