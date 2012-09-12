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
#include "cube.h"
// KConfigSkeleton
#include "cubeconfig.h"

#include "cube_inside.h"

#include <kaction.h>
#include <kactioncollection.h>
#include <klocale.h>
#include <kwinconfig.h>
#include <kdebug.h>

#include <QColor>
#include <QRect>
#include <QEvent>
#include <QFutureWatcher>
#include <QKeyEvent>
#include <QtConcurrentRun>
#include <QVector2D>
#include <QVector3D>

#include <math.h>

#include <kwinglutils.h>
#include <kwinglplatform.h>

namespace KWin
{

KWIN_EFFECT(cube, CubeEffect)
KWIN_EFFECT_SUPPORTED(cube, CubeEffect::supported())

CubeEffect::CubeEffect()
    : activated(false)
    , mousePolling(false)
    , cube_painting(false)
    , keyboard_grab(false)
    , schedule_close(false)
    , painting_desktop(1)
    , frontDesktop(0)
    , cubeOpacity(1.0)
    , opacityDesktopOnly(true)
    , displayDesktopName(false)
    , desktopNameFrame(NULL)
    , reflection(true)
    , rotating(false)
    , desktopChangedWhileRotating(false)
    , paintCaps(true)
    , rotationDirection(Left)
    , verticalRotationDirection(Upwards)
    , verticalPosition(Normal)
    , wallpaper(NULL)
    , texturedCaps(true)
    , capTexture(NULL)
    , manualAngle(0.0)
    , manualVerticalAngle(0.0)
    , currentShape(QTimeLine::EaseInOutCurve)
    , start(false)
    , stop(false)
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
    , cylinderShader(0)
    , sphereShader(0)
    , zOrderingFactor(0.0f)
    , mAddedHeightCoeff1(0.0f)
    , mAddedHeightCoeff2(0.0f)
    , m_cubeCapBuffer(NULL)
    , m_proxy(this)
{
    desktopNameFont.setBold(true);
    desktopNameFont.setPointSize(14);

    const QString fragmentshader = KGlobal::dirs()->findResource("data", "kwin/cube-reflection.glsl");
    m_reflectionShader = ShaderManager::instance()->loadFragmentShader(ShaderManager::GenericShader, fragmentshader);
    const QString capshader = KGlobal::dirs()->findResource("data", "kwin/cube-cap.glsl");
    m_capShader = ShaderManager::instance()->loadFragmentShader(ShaderManager::GenericShader, capshader);
    m_textureMirrorMatrix.scale(1.0, -1.0, 1.0);
    m_textureMirrorMatrix.translate(0.0, -1.0, 0.0);
    connect(effects, SIGNAL(tabBoxAdded(int)), this, SLOT(slotTabBoxAdded(int)));
    connect(effects, SIGNAL(tabBoxClosed()), this, SLOT(slotTabBoxClosed()));
    connect(effects, SIGNAL(tabBoxUpdated()), this, SLOT(slotTabBoxUpdated()));
    connect(effects, SIGNAL(mouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)),
            this, SLOT(slotMouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)));

    reconfigure(ReconfigureAll);
}

bool CubeEffect::supported()
{
    return effects->compositingType() == OpenGLCompositing;
}

void CubeEffect::reconfigure(ReconfigureFlags)
{
    loadConfig("Cube");
}

void CubeEffect::loadConfig(QString config)
{
    CubeConfig::self()->readConfig();
    foreach (ElectricBorder border, borderActivate) {
        effects->unreserveElectricBorder(border);
    }
    foreach (ElectricBorder border, borderActivateCylinder) {
        effects->unreserveElectricBorder(border);
    }
    foreach (ElectricBorder border, borderActivateSphere) {
        effects->unreserveElectricBorder(border);
    }
    borderActivate.clear();
    borderActivateCylinder.clear();
    borderActivateSphere.clear();
    QList<int> borderList = QList<int>();
    borderList.append(int(ElectricNone));
    borderList = CubeConfig::borderActivate();
    foreach (int i, borderList) {
        borderActivate.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i));
    }
    borderList.clear();
    borderList.append(int(ElectricNone));
    borderList = CubeConfig::borderActivateCylinder();
    foreach (int i, borderList) {
        borderActivateCylinder.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i));
    }
    borderList.clear();
    borderList.append(int(ElectricNone));
    borderList = CubeConfig::borderActivateSphere();
    foreach (int i, borderList) {
        borderActivateSphere.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i));
    }

    cubeOpacity = (float)CubeConfig::opacity() / 100.0f;
    opacityDesktopOnly = CubeConfig::opacityDesktopOnly();
    displayDesktopName = CubeConfig::displayDesktopName();
    reflection = CubeConfig::reflection();
    rotationDuration = animationTime(CubeConfig::rotationDuration() != 0 ? CubeConfig::rotationDuration() : 500);
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
    wallpaper = NULL;
    delete capTexture;
    capTexture = NULL;
    texturedCaps = CubeConfig::texturedCaps();

    timeLine.setCurveShape(QTimeLine::EaseInOutCurve);
    timeLine.setDuration(rotationDuration);

    verticalTimeLine.setCurveShape(QTimeLine::EaseInOutCurve);
    verticalTimeLine.setDuration(rotationDuration);

    // do not connect the shortcut if we use cylinder or sphere
    if (!shortcutsRegistered) {
        KActionCollection* actionCollection = new KActionCollection(this);
        KAction* cubeAction = static_cast< KAction* >(actionCollection->addAction("Cube"));
        cubeAction->setText(i18n("Desktop Cube"));
        cubeAction->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F11));
        cubeShortcut = cubeAction->globalShortcut();
        KAction* cylinderAction = static_cast< KAction* >(actionCollection->addAction("Cylinder"));
        cylinderAction->setText(i18n("Desktop Cylinder"));
        cylinderAction->setGlobalShortcut(KShortcut(), KAction::ActiveShortcut);
        cylinderShortcut = cylinderAction->globalShortcut();
        KAction* sphereAction = static_cast< KAction* >(actionCollection->addAction("Sphere"));
        sphereAction->setText(i18n("Desktop Sphere"));
        sphereAction->setGlobalShortcut(KShortcut(), KAction::ActiveShortcut);
        sphereShortcut = sphereAction->globalShortcut();
        connect(cubeAction, SIGNAL(triggered(bool)), this, SLOT(toggleCube()));
        connect(cylinderAction, SIGNAL(triggered(bool)), this, SLOT(toggleCylinder()));
        connect(sphereAction, SIGNAL(triggered(bool)), this, SLOT(toggleSphere()));
        connect(cubeAction, SIGNAL(globalShortcutChanged(QKeySequence)), this, SLOT(cubeShortcutChanged(QKeySequence)));
        connect(cylinderAction, SIGNAL(globalShortcutChanged(QKeySequence)), this, SLOT(cylinderShortcutChanged(QKeySequence)));
        connect(sphereAction, SIGNAL(globalShortcutChanged(QKeySequence)), this, SLOT(sphereShortcutChanged(QKeySequence)));
        shortcutsRegistered = true;
    }

    // set the cap color on the shader
    if (ShaderManager::instance()->isValid() && m_capShader->isValid()) {
        ShaderManager::instance()->pushShader(m_capShader);
        m_capShader->setUniform("u_capColor", capColor);
        ShaderManager::instance()->popShader();
    }
}

CubeEffect::~CubeEffect()
{
    foreach (ElectricBorder border, borderActivate) {
        effects->unreserveElectricBorder(border);
    }
    foreach (ElectricBorder border, borderActivateCylinder) {
        effects->unreserveElectricBorder(border);
    }
    foreach (ElectricBorder border, borderActivateSphere) {
        effects->unreserveElectricBorder(border);
    }
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
        capTexture = new GLTexture(img);
        capTexture->setFilter(GL_LINEAR);
#ifndef KWIN_HAVE_OPENGLES
        capTexture->setWrapMode(GL_CLAMP_TO_BORDER);
#endif
        // need to recreate the VBO for the cube cap
        delete m_cubeCapBuffer;
        m_cubeCapBuffer = NULL;
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
        wallpaper = new GLTexture(img);
        effects->addRepaintFull();
    }
    watcher->deleteLater();
}

bool CubeEffect::loadShader()
{
    if (!(GLPlatform::instance()->supports(GLSL) &&
            (effects->compositingType() == OpenGLCompositing)))
        return false;
    QString fragmentshader       =  KGlobal::dirs()->findResource("data", "kwin/cylinder.frag");
    QString cylinderVertexshader =  KGlobal::dirs()->findResource("data", "kwin/cylinder.vert");
    QString sphereVertexshader   = KGlobal::dirs()->findResource("data", "kwin/sphere.vert");
    if (fragmentshader.isEmpty() || cylinderVertexshader.isEmpty() || sphereVertexshader.isEmpty()) {
        kError(1212) << "Couldn't locate shader files" << endl;
        return false;
    }

    ShaderManager *shaderManager = ShaderManager::instance();
    // TODO: use generic shader - currently it is failing in alpha/brightness manipulation
    cylinderShader = new GLShader(cylinderVertexshader, fragmentshader);
    if (!cylinderShader->isValid()) {
        kError(1212) << "The cylinder shader failed to load!" << endl;
        return false;
    } else {
        shaderManager->pushShader(cylinderShader);
        cylinderShader->setUniform("sampler", 0);
        QMatrix4x4 projection;
        float fovy = 60.0f;
        float aspect = 1.0f;
        float zNear = 0.1f;
        float zFar = 100.0f;
        float ymax = zNear * tan(fovy  * M_PI / 360.0f);
        float ymin = -ymax;
        float xmin =  ymin * aspect;
        float xmax = ymax * aspect;
        projection.frustum(xmin, xmax, ymin, ymax, zNear, zFar);
        cylinderShader->setUniform(GLShader::ProjectionMatrix, projection);
        QMatrix4x4 modelview;
        float scaleFactor = 1.1 * tan(fovy * M_PI / 360.0f) / ymax;
        modelview.translate(xmin * scaleFactor, ymax * scaleFactor, -1.1);
        modelview.scale((xmax - xmin)*scaleFactor / displayWidth(), -(ymax - ymin)*scaleFactor / displayHeight(), 0.001);
        cylinderShader->setUniform(GLShader::ModelViewMatrix, modelview);
        const QMatrix4x4 identity;
        cylinderShader->setUniform(GLShader::ScreenTransformation, identity);
        cylinderShader->setUniform(GLShader::WindowTransformation, identity);
        QRect rect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
        cylinderShader->setUniform("width", (float)rect.width() * 0.5f);
        shaderManager->popShader();
    }
    // TODO: use generic shader - currently it is failing in alpha/brightness manipulation
    sphereShader = new GLShader(sphereVertexshader, fragmentshader);
    if (!sphereShader->isValid()) {
        kError(1212) << "The sphere shader failed to load!" << endl;
        return false;
    } else {
        shaderManager->pushShader(sphereShader);
        sphereShader->setUniform("sampler", 0);
        QMatrix4x4 projection;
        float fovy = 60.0f;
        float aspect = 1.0f;
        float zNear = 0.1f;
        float zFar = 100.0f;
        float ymax = zNear * tan(fovy  * M_PI / 360.0f);
        float ymin = -ymax;
        float xmin =  ymin * aspect;
        float xmax = ymax * aspect;
        projection.frustum(xmin, xmax, ymin, ymax, zNear, zFar);
        sphereShader->setUniform(GLShader::ProjectionMatrix, projection);
        QMatrix4x4 modelview;
        float scaleFactor = 1.1 * tan(fovy * M_PI / 360.0f) / ymax;
        modelview.translate(xmin * scaleFactor, ymax * scaleFactor, -1.1);
        modelview.scale((xmax - xmin)*scaleFactor / displayWidth(), -(ymax - ymin)*scaleFactor / displayHeight(), 0.001);
        sphereShader->setUniform(GLShader::ModelViewMatrix, modelview);
        const QMatrix4x4 identity;
        sphereShader->setUniform(GLShader::ScreenTransformation, identity);
        sphereShader->setUniform(GLShader::WindowTransformation, identity);
        QRect rect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
        sphereShader->setUniform("width", (float)rect.width() * 0.5f);
        sphereShader->setUniform("height", (float)rect.height() * 0.5f);
        sphereShader->setUniform("u_offset", QVector2D(0, 0));
        shaderManager->popShader();
        checkGLError("Loading Sphere Shader");
    }
    return true;
}

void CubeEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (activated) {
        data.mask |= PAINT_SCREEN_TRANSFORMED | Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS | PAINT_SCREEN_BACKGROUND_FIRST;

        if (rotating || start || stop) {
            timeLine.setCurrentTime(timeLine.currentTime() + time);
            rotateCube();
        }
        if (verticalRotating) {
            verticalTimeLine.setCurrentTime(verticalTimeLine.currentTime() + time);
            rotateCube();
        }
    }
    effects->prePaintScreen(data, time);
}

void CubeEffect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
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
            if (ShaderManager::instance()->isValid()) {
                ShaderManager::instance()->pushShader(ShaderManager::SimpleShader);
            }
            wallpaper->bind();
            wallpaper->render(region, rect);
            wallpaper->unbind();
            if (ShaderManager::instance()->isValid()) {
                ShaderManager::instance()->popShader();
            }
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // some veriables needed for painting the caps
        float cubeAngle = (float)((float)(effects->numberOfDesktops() - 2) / (float)effects->numberOfDesktops() * 180.0f);
        float point = rect.width() / 2 * tan(cubeAngle * 0.5f * M_PI / 180.0f);
        float zTranslate = zPosition + zoom;
        if (start)
            zTranslate *= timeLine.currentValue();
        if (stop)
            zTranslate *= (1.0 - timeLine.currentValue());
        // reflection
        if (reflection && mode != Sphere) {
            // we can use a huge scale factor (needed to calculate the rearground vertices)
            float scaleFactor = 1000000 * tan(60.0 * M_PI / 360.0f) / rect.height();
            m_reflectionMatrix.setToIdentity();
            m_reflectionMatrix.scale(1.0, -1.0, 1.0);

            // TODO reflection is not correct when mixing manual (mouse) rotating with rotation by cursor keys
            // there's also a small bug when zooming
            float addedHeight1 = -sin(asin(float(rect.height()) / mAddedHeightCoeff1) + fabs(manualVerticalAngle) * M_PI / 180.0f) * mAddedHeightCoeff1;
            float addedHeight2 = -sin(asin(float(rect.height()) / mAddedHeightCoeff2) + fabs(manualVerticalAngle) * M_PI / 180.0f) * mAddedHeightCoeff2 - addedHeight1;
            if (manualVerticalAngle > 0.0f && effects->numberOfDesktops() & 1) {
                m_reflectionMatrix.translate(0.0, cos(fabs(manualAngle) * M_PI / 360.0f * float(effects->numberOfDesktops())) * addedHeight2 + addedHeight1 - float(rect.height()), 0.0);
            } else {
                m_reflectionMatrix.translate(0.0, sin(fabs(manualAngle) * M_PI / 360.0f * float(effects->numberOfDesktops())) * addedHeight2 + addedHeight1 - float(rect.height()), 0.0);
            }
            pushMatrix(m_reflectionMatrix);

#ifndef KWIN_HAVE_OPENGLES
            // TODO: find a solution for GLES
            glEnable(GL_CLIP_PLANE0);
#endif
            reflectionPainting = true;
            glEnable(GL_CULL_FACE);
            paintCap(true, -point - zTranslate);

            // cube
            glCullFace(GL_BACK);
            pushMatrix(m_rotationMatrix);
            paintCube(mask, region, data);
            popMatrix();

            // call the inside cube effects
#ifndef KWIN_HAVE_OPENGLES
            foreach (CubeInsideEffect * inside, m_cubeInsideEffects) {
                pushMatrix(m_rotationMatrix);
                glTranslatef(rect.width() / 2, rect.height() / 2, -point - zTranslate);
                glRotatef((1 - frontDesktop) * 360.0f / effects->numberOfDesktops(), 0.0, 1.0, 0.0);
                inside->paint();
                popMatrix();
            }
#endif

            glCullFace(GL_FRONT);
            pushMatrix(m_rotationMatrix);
            paintCube(mask, region, data);
            popMatrix();

            paintCap(false, -point - zTranslate);
            glDisable(GL_CULL_FACE);
            reflectionPainting = false;
#ifndef KWIN_HAVE_OPENGLES
            // TODO: find a solution for GLES
            glDisable(GL_CLIP_PLANE0);
#endif
            popMatrix();

            float vertices[] = {
                -rect.width() * 0.5f, rect.height(), 0.0,
                rect.width() * 0.5f, rect.height(), 0.0,
                (float)rect.width()*scaleFactor, rect.height(), -5000,
                -(float)rect.width()*scaleFactor, rect.height(), -5000
            };
            // foreground
            float alpha = 0.7;
            if (start)
                alpha = 0.3 + 0.4 * timeLine.currentValue();
            if (stop)
                alpha = 0.3 + 0.4 * (1.0 - timeLine.currentValue());
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            ShaderManager *shaderManager = ShaderManager::instance();
            if (shaderManager->isValid() && m_reflectionShader->isValid()) {
                // ensure blending is enabled - no attribute stack
                shaderManager->pushShader(m_reflectionShader);
                QMatrix4x4 windowTransformation;
                windowTransformation.translate(rect.x() + rect.width() * 0.5f, 0.0, 0.0);
                m_reflectionShader->setUniform("windowTransformation", windowTransformation);
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

                shaderManager->popShader();
            } else {
#ifndef KWIN_HAVE_OPENGLES
                glColor4f(0.0, 0.0, 0.0, alpha);
                glPushMatrix();
                glTranslatef(rect.x() + rect.width() * 0.5f, 0.0, 0.0);
                glBegin(GL_POLYGON);
                glVertex3f(vertices[0], vertices[1], vertices[2]);
                glVertex3f(vertices[3], vertices[4], vertices[5]);
                // rearground
                alpha = -1.0;
                glColor4f(0.0, 0.0, 0.0, alpha);
                glVertex3f(vertices[6], vertices[7], vertices[8]);
                glVertex3f(vertices[9], vertices[10], vertices[11]);
                glEnd();
                glPopMatrix();
#endif
            }
            glDisable(GL_BLEND);
        }
        glEnable(GL_CULL_FACE);
        // caps
        paintCap(false, -point - zTranslate);

        // cube
        glCullFace(GL_FRONT);
        pushMatrix(m_rotationMatrix);
        paintCube(mask, region, data);
        popMatrix();


        // call the inside cube effects
#ifndef KWIN_HAVE_OPENGLES
        foreach (CubeInsideEffect * inside, m_cubeInsideEffects) {
            pushMatrix(m_rotationMatrix);
            glTranslatef(rect.width() / 2, rect.height() / 2, -point - zTranslate);
            glRotatef((1 - frontDesktop) * 360.0f / effects->numberOfDesktops(), 0.0, 1.0, 0.0);
            inside->paint();
            popMatrix();
        }
#endif

        glCullFace(GL_BACK);
        pushMatrix(m_rotationMatrix);
        paintCube(mask, region, data);
        popMatrix();

        // cap
        paintCap(true, -point - zTranslate);
        glDisable(GL_CULL_FACE);

        glDisable(GL_BLEND);

        // desktop name box - inspired from coverswitch
        if (displayDesktopName) {
            double opacity = 1.0;
            if (start)
                opacity = timeLine.currentValue();
            if (stop)
                opacity = 1.0 - timeLine.currentValue();
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
        // restore the ScreenTransformation after all desktops are painted
        // if not done GenericShader keeps the rotation data and transforms windows incorrectly in other rendering calls
        if (ShaderManager::instance()->isValid()) {
            GLShader *shader = ShaderManager::instance()->pushShader(KWin::ShaderManager::GenericShader);
            shader->setUniform(GLShader::ScreenTransformation, QMatrix4x4());
            ShaderManager::instance()->popShader();
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
    if (start)
        zTranslate *= timeLine.currentValue();
    if (stop)
        zTranslate *= (1.0 - timeLine.currentValue());
    // Rotation of the cube
    float cubeAngle = (float)((float)(effects->numberOfDesktops() - 2) / (float)effects->numberOfDesktops() * 180.0f);
    float point = rect.width() / 2 * tan(cubeAngle * 0.5f * M_PI / 180.0f);
    if (verticalRotating || verticalPosition != Normal || manualVerticalAngle != 0.0) {
        // change the verticalPosition if manualVerticalAngle > 90 or < -90 degrees
        if (manualVerticalAngle <= -90.0) {
            manualVerticalAngle += 90.0;
            if (verticalPosition == Normal)
                verticalPosition = Down;
            if (verticalPosition == Up)
                verticalPosition = Normal;
        }
        if (manualVerticalAngle >= 90.0) {
            manualVerticalAngle -= 90.0;
            if (verticalPosition == Normal)
                verticalPosition = Up;
            if (verticalPosition == Down)
                verticalPosition = Normal;
        }
        float angle = 0.0;
        if (verticalPosition == Up) {
            angle = 90.0;
            if (!verticalRotating) {
                if (manualVerticalAngle < 0.0)
                    angle += manualVerticalAngle;
                else
                    manualVerticalAngle = 0.0;
            }
        } else if (verticalPosition == Down) {
            angle = -90.0;
            if (!verticalRotating) {
                if (manualVerticalAngle > 0.0)
                    angle += manualVerticalAngle;
                else
                    manualVerticalAngle = 0.0;
            }
        } else {
            angle = manualVerticalAngle;
        }
        if (verticalRotating) {
            angle *= verticalTimeLine.currentValue();
            if (verticalPosition == Normal && verticalRotationDirection == Upwards)
                angle = -90.0 + 90 * verticalTimeLine.currentValue();
            if (verticalPosition == Normal && verticalRotationDirection == Downwards)
                angle = 90.0 - 90 * verticalTimeLine.currentValue();
            angle += manualVerticalAngle * (1.0 - verticalTimeLine.currentValue());
        }
        if (stop)
            angle *= (1.0 - timeLine.currentValue());
        m_rotationMatrix.translate(rect.width() / 2, rect.height() / 2, -point - zTranslate);
        m_rotationMatrix.rotate(angle, 1.0, 0.0, 0.0);
        m_rotationMatrix.translate(-rect.width() / 2, -rect.height() / 2, point + zTranslate);
    }
    if (rotating || (manualAngle != 0.0)) {
        int tempFrontDesktop = frontDesktop;
        if (manualAngle > internalCubeAngle * 0.5f) {
            manualAngle -= internalCubeAngle;
            tempFrontDesktop--;
            if (tempFrontDesktop == 0)
                tempFrontDesktop = effects->numberOfDesktops();
        }
        if (manualAngle < -internalCubeAngle * 0.5f) {
            manualAngle += internalCubeAngle;
            tempFrontDesktop++;
            if (tempFrontDesktop > effects->numberOfDesktops())
                tempFrontDesktop = 1;
        }
        float rotationAngle = internalCubeAngle * timeLine.currentValue();
        if (rotationAngle > internalCubeAngle * 0.5f) {
            rotationAngle -= internalCubeAngle;
            if (!desktopChangedWhileRotating) {
                desktopChangedWhileRotating = true;
                if (rotationDirection == Left) {
                    tempFrontDesktop++;
                } else if (rotationDirection == Right) {
                    tempFrontDesktop--;
                }
                if (tempFrontDesktop > effects->numberOfDesktops())
                    tempFrontDesktop = 1;
                else if (tempFrontDesktop == 0)
                    tempFrontDesktop = effects->numberOfDesktops();
            }
        }
        // don't change front desktop during stop animation as this would break some logic
        if (!stop)
            frontDesktop = tempFrontDesktop;
        if (rotationDirection == Left) {
            rotationAngle *= -1;
        }
        if (stop)
            rotationAngle = manualAngle * (1.0 - timeLine.currentValue());
        else
            rotationAngle += manualAngle * (1.0 - timeLine.currentValue());
        m_rotationMatrix.translate(rect.width() / 2, rect.height() / 2, -point - zTranslate);
        m_rotationMatrix.rotate(rotationAngle, 0.0, 1.0, 0.0);
        m_rotationMatrix.translate(-rect.width() / 2, -rect.height() / 2, point + zTranslate);
    }
}

void CubeEffect::paintCube(int mask, QRegion region, ScreenPaintData& data)
{
    QRect rect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
    float internalCubeAngle = 360.0f / effects->numberOfDesktops();
    cube_painting = true;
    float zTranslate = zPosition + zoom;
    if (start)
        zTranslate *= timeLine.currentValue();
    if (stop)
        zTranslate *= (1.0 - timeLine.currentValue());

    // Rotation of the cube
    float cubeAngle = (float)((float)(effects->numberOfDesktops() - 2) / (float)effects->numberOfDesktops() * 180.0f);
    float point = rect.width() / 2 * tan(cubeAngle * 0.5f * M_PI / 180.0f);

    for (int i = 0; i < effects->numberOfDesktops(); i++) {
        // start painting the cube
        painting_desktop = (i + frontDesktop) % effects->numberOfDesktops();
        if (painting_desktop == 0) {
            painting_desktop = effects->numberOfDesktops();
        }
        ScreenPaintData newData = data;
        newData.setRotationAxis(Qt::YAxis);
        newData.setRotationAngle(internalCubeAngle * i);
        newData.setRotationOrigin(QVector3D(rect.width() / 2, 0.0, -point));
        newData.setZTranslation(-zTranslate);
        effects->paintScreen(mask, region, newData);
    }
    cube_painting = false;
    painting_desktop = effects->currentDesktop();
}

void CubeEffect::paintCap(bool frontFirst, float zOffset)
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

    QMatrix4x4 capMatrix;
    capMatrix.translate(rect.width() / 2, 0.0, zOffset);
    capMatrix.rotate((1 - frontDesktop) * 360.0f / effects->numberOfDesktops(), 0.0, 1.0, 0.0);
    capMatrix.translate(0.0, rect.height(), 0.0);
    if (mode == Sphere) {
        capMatrix.scale(1.0, -1.0, 1.0);
    }

    bool capShader = false;
    if (ShaderManager::instance()->isValid() && m_capShader->isValid()) {
        capShader = true;
        ShaderManager::instance()->pushShader(m_capShader);
        float opacity = cubeOpacity;
        if (start) {
            opacity *= timeLine.currentValue();
        } else if (stop) {
            opacity *= (1.0 - timeLine.currentValue());
        }
        m_capShader->setUniform("u_opacity", opacity);
        m_capShader->setUniform("u_mirror", 1);
        if (reflectionPainting) {
            m_capShader->setUniform(GLShader::ScreenTransformation, m_reflectionMatrix * m_rotationMatrix);
        } else {
            m_capShader->setUniform(GLShader::ScreenTransformation, m_rotationMatrix);
        }
        m_capShader->setUniform(GLShader::WindowTransformation, capMatrix);
        m_capShader->setUniform("u_untextured", texturedCaps ? 0 : 1);
        if (texturedCaps && effects->numberOfDesktops() > 3 && capTexture) {
            capTexture->bind();
        }
    } else {
        pushMatrix(m_rotationMatrix * capMatrix);

#ifndef KWIN_HAVE_OPENGLES
        glMatrixMode(GL_TEXTURE);
#endif
        pushMatrix();
        loadMatrix(m_textureMirrorMatrix);
#ifndef KWIN_HAVE_OPENGLES
        glMatrixMode(GL_MODELVIEW);

        glColor4f(capColor.redF(), capColor.greenF(), capColor.blueF(), cubeOpacity);
        if (texturedCaps && effects->numberOfDesktops() > 3 && capTexture) {
            // modulate the cap texture: cap color should be background for translucent pixels
            // cube opacity should be used for all pixels
            // blend with cap color
            float color[4] = { capColor.redF(), capColor.greenF(), capColor.blueF(), cubeOpacity };
            glActiveTexture(GL_TEXTURE0);
            capTexture->bind();
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
            glColor4fv(color);

            // set Opacity to cube opacity
            // TODO: change opacity during start/stop animation
            glActiveTexture(GL_TEXTURE1);
            capTexture->bind();
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_CONSTANT);
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
            glActiveTexture(GL_TEXTURE0);
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);
        }
#endif
    }

    glEnable(GL_BLEND);
    glCullFace(firstCull);
    m_cubeCapBuffer->render(GL_TRIANGLES);

    if (mode == Sphere) {
        capMatrix.scale(1.0, -1.0, 1.0);
    }
    capMatrix.translate(0.0, -rect.height(), 0.0);
    if (capShader) {
        m_capShader->setUniform("windowTransformation", capMatrix);
        m_capShader->setUniform("u_mirror", 0);
    } else {
#ifndef KWIN_HAVE_OPENGLES
        glMatrixMode(GL_TEXTURE);
        popMatrix();
        glMatrixMode(GL_MODELVIEW);
#endif
        popMatrix();
        pushMatrix(m_rotationMatrix * capMatrix);
    }
    glCullFace(secondCull);
    m_cubeCapBuffer->render(GL_TRIANGLES);
    glDisable(GL_BLEND);

    if (capShader) {
        ShaderManager::instance()->popShader();
        if (texturedCaps && effects->numberOfDesktops() > 3 && capTexture) {
            capTexture->unbind();
        }
    } else {
        popMatrix();
        if (texturedCaps && effects->numberOfDesktops() > 3 && capTexture) {
#ifndef KWIN_HAVE_OPENGLES
            glActiveTexture(GL_TEXTURE1);
            glDisable(capTexture->target());
            glActiveTexture(GL_TEXTURE0);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
            capTexture->unbind();
#endif
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
    m_cubeCapBuffer->setData(verts.count() / 3, 3, verts.constData(), texture ? texCoords.constData() : NULL);
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
    m_cubeCapBuffer->setData(verts.count() / 3, 3, verts.constData(), texture ? texCoords.constData() : NULL);
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
        float yTop = rect.height() * 0.5 - radius * cos(topAngle);
        yTop -= (yTop - rect.height() * 0.5) * capDeformationFactor;
        float yBottom = rect.height() * 0.5 - radius * cos(bottomAngle);
        yBottom -= (yBottom - rect.height() * 0.5) * capDeformationFactor;
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
    m_cubeCapBuffer->setData(verts.count() / 3, 3, verts.constData(), texture ? texCoords.constData() : NULL);
}

void CubeEffect::postPaintScreen()
{
    effects->postPaintScreen();
    if (activated) {
        if (start) {
            if (timeLine.currentValue() == 1.0) {
                start = false;
                timeLine.setCurrentTime(0);
                // more rotations?
                if (!rotations.empty()) {
                    rotationDirection = rotations.dequeue();
                    rotating = true;
                    // change the curve shape if current shape is not easeInOut
                    if (currentShape != QTimeLine::EaseInOutCurve) {
                        // more rotations follow -> linear curve
                        if (!rotations.empty()) {
                            currentShape = QTimeLine::LinearCurve;
                        }
                        // last rotation step -> easeOut curve
                        else {
                            currentShape = QTimeLine::EaseOutCurve;
                        }
                        timeLine.setCurveShape(currentShape);
                    } else {
                        // if there is at least one more rotation, we can change to easeIn
                        if (!rotations.empty()) {
                            currentShape = QTimeLine::EaseInCurve;
                            timeLine.setCurveShape(currentShape);
                        }
                    }
                }
            }
            effects->addRepaintFull();
            return; // schedule_close could have been called, start has to finish first
        }
        if (stop) {
            if (timeLine.currentValue() == 1.0) {
                effects->setCurrentDesktop(frontDesktop);
                stop = false;
                timeLine.setCurrentTime(0);
                activated = false;
                // set the new desktop
                if (keyboard_grab)
                    effects->ungrabKeyboard();
                keyboard_grab = false;
                effects->destroyInputWindow(input);

                effects->setActiveFullScreenEffect(0);

                delete m_cubeCapBuffer;
                m_cubeCapBuffer = NULL;
                if (desktopNameFrame)
                    desktopNameFrame->free();
            }
            effects->addRepaintFull();
        }
        if (rotating || verticalRotating) {
            if (rotating && timeLine.currentValue() == 1.0) {
                timeLine.setCurrentTime(0.0);
                rotating = false;
                desktopChangedWhileRotating = false;
                manualAngle = 0.0;
                // more rotations?
                if (!rotations.empty()) {
                    rotationDirection = rotations.dequeue();
                    rotating = true;
                    // change the curve shape if current shape is not easeInOut
                    if (currentShape != QTimeLine::EaseInOutCurve) {
                        // more rotations follow -> linear curve
                        if (!rotations.empty()) {
                            currentShape = QTimeLine::LinearCurve;
                        }
                        // last rotation step -> easeOut curve
                        else {
                            currentShape = QTimeLine::EaseOutCurve;
                        }
                        timeLine.setCurveShape(currentShape);
                    } else {
                        // if there is at least one more rotation, we can change to easeIn
                        if (!rotations.empty()) {
                            currentShape = QTimeLine::EaseInCurve;
                            timeLine.setCurveShape(currentShape);
                        }
                    }
                } else {
                    // reset curve shape if there are no more rotations
                    if (currentShape != QTimeLine::EaseInOutCurve) {
                        currentShape = QTimeLine::EaseInOutCurve;
                        timeLine.setCurveShape(currentShape);
                    }
                }
            }
            if (verticalRotating && verticalTimeLine.currentValue() == 1.0) {
                verticalTimeLine.setCurrentTime(0);
                verticalRotating = false;
                manualVerticalAngle = 0.0;
                // more rotations?
                if (!verticalRotations.empty()) {
                    verticalRotationDirection = verticalRotations.dequeue();
                    verticalRotating = true;
                    if (verticalRotationDirection == Upwards) {
                        if (verticalPosition == Normal)
                            verticalPosition = Up;
                        if (verticalPosition == Down)
                            verticalPosition = Normal;
                    }
                    if (verticalRotationDirection == Downwards) {
                        if (verticalPosition == Normal)
                            verticalPosition = Down;
                        if (verticalPosition == Up)
                            verticalPosition = Normal;
                    }
                }
            }
            effects->addRepaintFull();
            return; // rotation has to end before cube is closed
        }
        if (schedule_close) {
            schedule_close = false;
            stop = true;
            effects->addRepaintFull();
        }
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
    GLShader *shader = NULL;
    QMatrix4x4 origMatrix;
    if (activated && cube_painting) {
        shader = shaderManager->pushShader(ShaderManager::GenericShader);
        //kDebug(1212) << w->caption();
        float opacity = cubeOpacity;
        if (start) {
            opacity = 1.0 - (1.0 - opacity) * timeLine.currentValue();
            if (reflectionPainting)
                opacity = 0.5 + (cubeOpacity - 0.5) * timeLine.currentValue();
            // fade in windows belonging to different desktops
            if (painting_desktop == effects->currentDesktop() && (!w->isOnDesktop(painting_desktop)))
                opacity = timeLine.currentValue() * cubeOpacity;
        }
        if (stop) {
            opacity = 1.0 - (1.0 - opacity) * (1.0 - timeLine.currentValue());
            if (reflectionPainting)
                opacity = 0.5 + (cubeOpacity - 0.5) * (1.0 - timeLine.currentValue());
            // fade out windows belonging to different desktops
            if (painting_desktop == effects->currentDesktop() && (!w->isOnDesktop(painting_desktop)))
                opacity = cubeOpacity * (1.0 - timeLine.currentValue());
        }
        // z-Ordering
        if (!w->isDesktop() && !w->isDock() && useZOrdering && !w->isOnAllDesktops()) {
            float zOrdering = (effects->stackingOrder().indexOf(w) + 1) * zOrderingFactor;
            if (start)
                zOrdering *= timeLine.currentValue();
            if (stop)
                zOrdering *= (1.0 - timeLine.currentValue());
            data.translate(0.0, 0.0, zOrdering);
        }
        // check for windows belonging to the previous desktop
        int prev_desktop = painting_desktop - 1;
        if (prev_desktop == 0)
            prev_desktop = effects->numberOfDesktops();
        int next_desktop = painting_desktop + 1;
        if (next_desktop > effects->numberOfDesktops())
            next_desktop = 1;
        if (!shader) {
            pushMatrix();
        }
        if (w->isOnDesktop(prev_desktop) && (mask & PAINT_WINDOW_TRANSFORMED)) {
            QRect rect = effects->clientArea(FullArea, activeScreen, prev_desktop);
            WindowQuadList new_quads;
            foreach (const WindowQuad & quad, data.quads) {
                if (quad.right() > rect.width() - w->x()) {
                    new_quads.append(quad);
                }
            }
            data.quads = new_quads;
            if (shader) {
                data.setXTranslation(-rect.width());
            } else {
                data.setRotationAxis(Qt::YAxis);
                data.setRotationOrigin(QVector3D(rect.width() - w->x(), 0.0, 0.0));
                data.setRotationAngle(-360.0f / effects->numberOfDesktops());
                float cubeAngle = (float)((float)(effects->numberOfDesktops() - 2) / (float)effects->numberOfDesktops() * 180.0f);
                float point = rect.width() / 2 * tan(cubeAngle * 0.5f * M_PI / 180.0f);
                QMatrix4x4 matrix;
                matrix.translate(rect.width() / 2, 0.0, -point);
                matrix.rotate(-360.0f / effects->numberOfDesktops(), 0.0, 1.0, 0.0);
                matrix.translate(-rect.width() / 2, 0.0, point);
                multiplyMatrix(matrix);
            }
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
            if (shader) {
                data.setXTranslation(rect.width());
            } else {
                data.setRotationAxis(Qt::YAxis);
                data.setRotationOrigin(QVector3D(-w->x(), 0.0, 0.0));
                data.setRotationAngle(-360.0f / effects->numberOfDesktops());
                float cubeAngle = (float)((float)(effects->numberOfDesktops() - 2) / (float)effects->numberOfDesktops() * 180.0f);
                float point = rect.width() / 2 * tan(cubeAngle * 0.5f * M_PI / 180.0f);
                QMatrix4x4 matrix;
                matrix.translate(rect.width() / 2, 0.0, -point);
                matrix.rotate(360.0f / effects->numberOfDesktops(), 0.0, 1.0, 0.0);
                matrix.translate(-rect.width() / 2, 0.0, point);
                multiplyMatrix(matrix);
            }
        }
        QRect rect = effects->clientArea(FullArea, activeScreen, painting_desktop);

        if (start || stop) {
            // we have to change opacity values for fade in/out of windows which are shown on front-desktop
            if (prev_desktop == effects->currentDesktop() && w->x() < rect.x()) {
                if (start)
                    opacity = timeLine.currentValue() * cubeOpacity;
                if (stop)
                    opacity = cubeOpacity * (1.0 - timeLine.currentValue());
            }
            if (next_desktop == effects->currentDesktop() && w->x() + w->width() > rect.x() + rect.width()) {
                if (start)
                    opacity = timeLine.currentValue() * cubeOpacity;
                if (stop)
                    opacity = cubeOpacity * (1.0 - timeLine.currentValue());
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
        if (shader) {
            origMatrix = shader->getUniformMatrix4x4("screenTransformation");
            GLShader *currentShader = shader;
            if (mode == Cylinder) {
                shaderManager->pushShader(cylinderShader);
                cylinderShader->setUniform("xCoord", (float)w->x());
                cylinderShader->setUniform("cubeAngle", (effects->numberOfDesktops() - 2) / (float)effects->numberOfDesktops() * 90.0f);
                float factor = 0.0f;
                if (start)
                    factor = 1.0f - timeLine.currentValue();
                if (stop)
                    factor = timeLine.currentValue();
                cylinderShader->setUniform("timeLine", factor);
                data.shader = cylinderShader;
                currentShader = cylinderShader;
            }
            if (mode == Sphere) {
                shaderManager->pushShader(sphereShader);
                sphereShader->setUniform("u_offset", QVector2D(w->x(), w->y()));
                sphereShader->setUniform("cubeAngle", (effects->numberOfDesktops() - 2) / (float)effects->numberOfDesktops() * 90.0f);
                float factor = 0.0f;
                if (start)
                    factor = 1.0f - timeLine.currentValue();
                if (stop)
                    factor = timeLine.currentValue();
                sphereShader->setUniform("timeLine", factor);
                data.shader = sphereShader;
                currentShader = sphereShader;
            }
            if (reflectionPainting) {
                currentShader->setUniform(GLShader::ScreenTransformation, m_reflectionMatrix * m_rotationMatrix * origMatrix);
            } else {
                currentShader->setUniform(GLShader::ScreenTransformation, m_rotationMatrix*origMatrix);
            }
        }
    }
    effects->paintWindow(w, mask, region, data);
    if (activated && cube_painting) {
        if (shader) {
            if (mode == Cylinder || mode == Sphere) {
                shaderManager->popShader();
            } else {
                shader->setUniform(GLShader::ScreenTransformation, origMatrix);
            }
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
                foreach (const QRect & paintRect, paint.rects()) {
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
                if (ShaderManager::instance()->isValid() && m_capShader->isValid()) {
                    capShader = true;
                    ShaderManager::instance()->pushShader(m_capShader);
                    m_capShader->setUniform("u_mirror", 0);
                    m_capShader->setUniform("u_untextured", 1);
                    if (reflectionPainting) {
                        m_capShader->setUniform(GLShader::ScreenTransformation, m_reflectionMatrix * m_rotationMatrix * origMatrix);
                    } else {
                        m_capShader->setUniform(GLShader::ScreenTransformation, m_rotationMatrix * origMatrix);
                    }
                    m_capShader->setUniform(GLShader::WindowTransformation, QMatrix4x4());
                }
                GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
                vbo->reset();
                QColor color = capColor;
                capColor.setAlphaF(cubeOpacity);
                vbo->setColor(color);
                vbo->setData(verts.size() / 2, 2, verts.constData(), NULL);
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
        if (!shader) {
            popMatrix();
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
    kDebug(1212) << "toggle cube";
    toggle(Cube);
}

void CubeEffect::toggleCylinder()
{
    kDebug(1212) << "toggle cylinder";
    if (!useShaders)
        useShaders = loadShader();
    if (useShaders)
        toggle(Cylinder);
    else
        kError(1212) << "Sorry shaders are not available - cannot activate Cylinder";
}

void CubeEffect::toggleSphere()
{
    kDebug(1212) << "toggle sphere";
    if (!useShaders)
        useShaders = loadShader();
    if (useShaders)
        toggle(Sphere);
    else
        kError(1212) << "Sorry shaders are not available - cannot activate Sphere";
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
    if (stop)
        return;
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
        switch(e->key()) {
            // wrap only on autorepeat
        case Qt::Key_Left:
            // rotate to previous desktop
            kDebug(1212) << "left";
            if (!rotating && !start) {
                rotating = true;
                if (invertKeys)
                    rotationDirection = Right;
                else
                    rotationDirection = Left;
            } else {
                if (rotations.count() < effects->numberOfDesktops()) {
                    if (invertKeys)
                        rotations.enqueue(Right);
                    else
                        rotations.enqueue(Left);
                }
            }
            break;
        case Qt::Key_Right:
            // rotate to next desktop
            kDebug(1212) << "right";
            if (!rotating && !start) {
                rotating = true;
                if (invertKeys)
                    rotationDirection = Left;
                else
                    rotationDirection = Right;
            } else {
                if (rotations.count() < effects->numberOfDesktops()) {
                    if (invertKeys)
                        rotations.enqueue(Left);
                    else
                        rotations.enqueue(Right);
                }
            }
            break;
        case Qt::Key_Up:
            kDebug(1212) << "up";
            if (invertKeys) {
                if (verticalPosition != Down) {
                    if (!verticalRotating) {
                        verticalRotating = true;
                        verticalRotationDirection = Downwards;
                        if (verticalPosition == Normal)
                            verticalPosition = Down;
                        if (verticalPosition == Up)
                            verticalPosition = Normal;
                    } else {
                        verticalRotations.enqueue(Downwards);
                    }
                } else if (manualVerticalAngle > 0.0 && !verticalRotating) {
                    // rotate to down position from the manual position
                    verticalRotating = true;
                    verticalRotationDirection = Downwards;
                    verticalPosition = Down;
                    manualVerticalAngle -= 90.0;
                }
            } else {
                if (verticalPosition != Up) {
                    if (!verticalRotating) {
                        verticalRotating = true;
                        verticalRotationDirection = Upwards;
                        if (verticalPosition == Normal)
                            verticalPosition = Up;
                        if (verticalPosition == Down)
                            verticalPosition = Normal;
                    } else {
                        verticalRotations.enqueue(Upwards);
                    }
                } else if (manualVerticalAngle < 0.0 && !verticalRotating) {
                    // rotate to up position from the manual position
                    verticalRotating = true;
                    verticalRotationDirection = Upwards;
                    verticalPosition = Up;
                    manualVerticalAngle += 90.0;
                }
            }
            break;
        case Qt::Key_Down:
            kDebug(1212) << "down";
            if (invertKeys) {
                if (verticalPosition != Up) {
                    if (!verticalRotating) {
                        verticalRotating = true;
                        verticalRotationDirection = Upwards;
                        if (verticalPosition == Normal)
                            verticalPosition = Up;
                        if (verticalPosition == Down)
                            verticalPosition = Normal;
                    } else {
                        verticalRotations.enqueue(Upwards);
                    }
                } else if (manualVerticalAngle < 0.0 && !verticalRotating) {
                    // rotate to up position from the manual position
                    verticalRotating = true;
                    verticalRotationDirection = Upwards;
                    verticalPosition = Up;
                    manualVerticalAngle += 90.0;
                }
            } else {
                if (verticalPosition != Down) {
                    if (!verticalRotating) {
                        verticalRotating = true;
                        verticalRotationDirection = Downwards;
                        if (verticalPosition == Normal)
                            verticalPosition = Down;
                        if (verticalPosition == Up)
                            verticalPosition = Normal;
                    } else {
                        verticalRotations.enqueue(Downwards);
                    }
                } else if (manualVerticalAngle > 0.0 && !verticalRotating) {
                    // rotate to down position from the manual position
                    verticalRotating = true;
                    verticalRotationDirection = Downwards;
                    verticalPosition = Down;
                    manualVerticalAngle -= 90.0;
                }
            }
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
        effects->addRepaintFull();
    }
}

void CubeEffect::rotateToDesktop(int desktop)
{
    int tempFrontDesktop = frontDesktop;
    if (!rotations.empty()) {
        // all scheduled rotations will be removed as a speed up
        rotations.clear();
    }
    if (rotating && !desktopChangedWhileRotating) {
        // front desktop will change during the actual rotation - this has to be considered
        if (rotationDirection == Left) {
            tempFrontDesktop++;
        } else if (rotationDirection == Right) {
            tempFrontDesktop--;
        }
        if (tempFrontDesktop > effects->numberOfDesktops())
            tempFrontDesktop = 1;
        else if (tempFrontDesktop == 0)
            tempFrontDesktop = effects->numberOfDesktops();
    }
    // find the fastest rotation path from tempFrontDesktop to desktop
    int rightRotations = tempFrontDesktop - desktop;
    if (rightRotations < 0)
        rightRotations += effects->numberOfDesktops();
    int leftRotations = desktop - tempFrontDesktop;
    if (leftRotations < 0)
        leftRotations += effects->numberOfDesktops();
    if (leftRotations <= rightRotations) {
        for (int i = 0; i < leftRotations; i++) {
            rotations.enqueue(Left);
        }
    } else {
        for (int i = 0; i < rightRotations; i++) {
            rotations.enqueue(Right);
        }
    }
    if (!start && !rotating && !rotations.empty()) {
        rotating = true;
        rotationDirection = rotations.dequeue();
    }
    // change timeline curve if more rotations are following
    if (!rotations.empty()) {
        currentShape = QTimeLine::EaseInCurve;
        timeLine.setCurveShape(currentShape);
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
            connect(watcher, SIGNAL(finished()), SLOT(slotCubeCapLoaded()));
            watcher->setFuture(QtConcurrent::run(this, &CubeEffect::loadCubeCap, capPath));
        }
        QString wallpaperPath = CubeConfig::wallpaper().toLocalFile();
        if (!wallpaper && !wallpaperPath.isEmpty()) {
            QFutureWatcher<QImage> *watcher = new QFutureWatcher<QImage>(this);
            connect(watcher, SIGNAL(finished()), SLOT(slotWallPaperLoaded()));
            watcher->setFuture(QtConcurrent::run(this, &CubeEffect::loadWallPaper, wallpaperPath));
        }
        if (!mousePolling) {
            effects->startMousePolling();
            mousePolling = true;
        }
        activated = true;
        activeScreen = effects->activeScreen();
        keyboard_grab = effects->grabKeyboard(this);
        input = effects->createInputWindow(this, 0, 0, displayWidth(), displayHeight(),
                                           Qt::OpenHandCursor);
        frontDesktop = effects->currentDesktop();
        zoom = 0.0;
        zOrderingFactor = zPosition / (effects->stackingOrder().count() - 1);
        start = true;
        effects->setActiveFullScreenEffect(this);
        kDebug(1212) << "Cube is activated";
        verticalPosition = Normal;
        verticalRotating = false;
        manualAngle = 0.0;
        manualVerticalAngle = 0.0;
        if (reflection) {
            QRect rect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
#ifndef KWIN_HAVE_OPENGLES
            // clip parts above the reflection area
            double eqn[4] = {0.0, 1.0, 0.0, 0.0};
            glPushMatrix();
            glTranslatef(0.0, rect.height(), 0.0);
            glClipPlane(GL_CLIP_PLANE0, eqn);
            glPopMatrix();
#endif
            float temporaryCoeff = float(rect.width()) / tan(M_PI / float(effects->numberOfDesktops()));
            mAddedHeightCoeff1 = sqrt(float(rect.height()) * float(rect.height()) + temporaryCoeff * temporaryCoeff);
            mAddedHeightCoeff2 = sqrt(float(rect.height()) * float(rect.height()) + float(rect.width()) * float(rect.width()) + temporaryCoeff * temporaryCoeff);
        }
        m_rotationMatrix.setToIdentity();
        effects->addRepaintFull();
    } else {
        if (mousePolling) {
            effects->stopMousePolling();
            mousePolling = false;
        }
        schedule_close = true;
        // we have to add a repaint, to start the deactivating
        effects->addRepaintFull();
    }
}

void CubeEffect::slotMouseChanged(const QPoint& pos, const QPoint& oldpos, Qt::MouseButtons buttons,
                              Qt::MouseButtons oldbuttons, Qt::KeyboardModifiers, Qt::KeyboardModifiers)
{
    if (!activated)
        return;
    if (tabBoxMode)
        return;
    if (stop)
        return;
    QRect rect = effects->clientArea(FullArea, activeScreen, effects->currentDesktop());
    if (buttons.testFlag(Qt::LeftButton)) {
        bool repaint = false;
        // vertical movement only if there is not a rotation
        if (!verticalRotating) {
            // display height corresponds to 180*
            int deltaY = pos.y() - oldpos.y();
            float deltaVerticalDegrees = (float)deltaY / rect.height() * 180.0f;
            if (invertMouse)
                manualVerticalAngle += deltaVerticalDegrees;
            else
                manualVerticalAngle -= deltaVerticalDegrees;
            if (deltaVerticalDegrees != 0.0)
                repaint = true;
        }
        // horizontal movement only if there is not a rotation
        if (!rotating) {
            // display width corresponds to sum of angles of the polyhedron
            int deltaX = oldpos.x() - pos.x();
            float deltaDegrees = (float)deltaX / rect.width() * 360.0f;
            if (deltaX == 0) {
                if (pos.x() == 0)
                    deltaDegrees = 5.0f;
                if (pos.x() == displayWidth() - 1)
                    deltaDegrees = -5.0f;
            }
            if (invertMouse)
                manualAngle += deltaDegrees;
            else
                manualAngle -= deltaDegrees;
            if (deltaDegrees != 0.0)
                repaint = true;
        }
        if (repaint) {
            rotateCube();
            effects->addRepaintFull();
        }
    }
    if (!oldbuttons.testFlag(Qt::LeftButton) && buttons.testFlag(Qt::LeftButton)) {
        XDefineCursor(display(), input, QCursor(Qt::ClosedHandCursor).handle());
    }
    if (oldbuttons.testFlag(Qt::LeftButton) && !buttons.testFlag(Qt::LeftButton)) {
        XDefineCursor(display(), input, QCursor(Qt::OpenHandCursor).handle());
        if (closeOnMouseRelease)
            setActive(false);
    }
    if (oldbuttons.testFlag(Qt::RightButton) && !buttons.testFlag(Qt::RightButton)) {
        // end effect on right mouse button
        setActive(false);
    }
}

void CubeEffect::windowInputMouseEvent(Window w, QEvent* e)
{
    assert(w == input);
    Q_UNUSED(w);
    QMouseEvent *mouse = dynamic_cast< QMouseEvent* >(e);
    if (mouse && mouse->type() == QEvent::MouseButtonRelease) {
        if (mouse->button() == Qt::XButton1) {
            if (!rotating && !start) {
                rotating = true;
                if (invertMouse)
                    rotationDirection = Right;
                else
                    rotationDirection = Left;
            } else {
                if (rotations.count() < effects->numberOfDesktops()) {
                    if (invertMouse)
                        rotations.enqueue(Right);
                    else
                        rotations.enqueue(Left);
                }
            }
            effects->addRepaintFull();
        }
        if (mouse->button() == Qt::XButton2) {
            if (!rotating && !start) {
                rotating = true;
                if (invertMouse)
                    rotationDirection = Left;
                else
                    rotationDirection = Right;
            } else {
                if (rotations.count() < effects->numberOfDesktops()) {
                    if (invertMouse)
                        rotations.enqueue(Left);
                    else
                        rotations.enqueue(Right);
                }
            }
            effects->addRepaintFull();
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

void CubeEffect::cubeShortcutChanged(const QKeySequence& seq)
{
    cubeShortcut = KShortcut(seq);
}

void CubeEffect::cylinderShortcutChanged(const QKeySequence& seq)
{
    cylinderShortcut = KShortcut(seq);
}

void CubeEffect::sphereShortcutChanged(const QKeySequence& seq)
{
    sphereShortcut = KShortcut(seq);
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
    return activated;
}

} // namespace
