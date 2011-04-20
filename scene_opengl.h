/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

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

#ifndef KWIN_SCENE_OPENGL_H
#define KWIN_SCENE_OPENGL_H

#include "scene.h"
#include "shadow.h"

#include "kwinglutils.h"

#ifdef KWIN_HAVE_OPENGL_COMPOSITING

#ifdef HAVE_XSHM
#include <X11/extensions/XShm.h>
#endif

namespace KWin
{

class SceneOpenGL
    : public Scene
{
public:
    class EffectFrame;
    class Texture;
    class Window;
    SceneOpenGL(Workspace* ws);
    virtual ~SceneOpenGL();
    virtual bool initFailed() const;
    virtual CompositingType compositingType() const {
        return OpenGLCompositing;
    }
    virtual void paint(QRegion damage, ToplevelList windows);
    virtual void windowGeometryShapeChanged(Toplevel*);
    virtual void windowOpacityChanged(Toplevel*);
    virtual void windowAdded(Toplevel*);
    virtual void windowClosed(Toplevel*, Deleted*);
    virtual void windowDeleted(Deleted*);

protected:
    virtual void paintGenericScreen(int mask, ScreenPaintData data);
    virtual void paintBackground(QRegion region);
    QMatrix4x4 transformation(int mask, const ScreenPaintData &data) const;

private:
    bool selectMode();
    bool initTfp();
    bool initBuffer();
    bool initRenderingContext();
    bool initBufferConfigs();
    bool initDrawableConfigs();
    void waitSync();
    void flushBuffer(int mask, QRegion damage);
    bool selfCheck();
    void selfCheckSetup();
    bool selfCheckFinish();
    GC gcroot;
    class FBConfigInfo
    {
    public:
#ifndef KWIN_HAVE_OPENGLES
        GLXFBConfig fbconfig;
#endif
        int bind_texture_format;
        int texture_targets;
        int y_inverted;
        int mipmap;
    };
#ifndef KWIN_HAVE_OPENGLES
    Drawable buffer;
    GLXFBConfig fbcbuffer;
#endif
    static bool db;
#ifndef KWIN_HAVE_OPENGLES
    static GLXFBConfig fbcbuffer_db;
    static GLXFBConfig fbcbuffer_nondb;
    static FBConfigInfo fbcdrawableinfo[ 32 + 1 ];
    static GLXDrawable glxbuffer;
    static GLXContext ctxbuffer;
    static GLXContext ctxdrawable;
    static GLXDrawable last_pixmap; // for a workaround in bindTexture()
#endif
    QHash< Toplevel*, Window* > windows;
    bool init_ok;
    bool selfCheckDone;
    bool debug;
};

class SceneOpenGL::Texture
    : public GLTexture
{
public:
    Texture();
    Texture(const Pixmap& pix, const QSize& size, int depth);
    virtual ~Texture();

    using GLTexture::load;
    virtual bool load(const Pixmap& pix, const QSize& size, int depth,
                      QRegion region);
    virtual bool load(const Pixmap& pix, const QSize& size, int depth);
    virtual bool load(const QImage& image, GLenum target = GL_TEXTURE_2D);
    virtual bool load(const QPixmap& pixmap, GLenum target = GL_TEXTURE_2D);
    virtual void discard();
    virtual void release(); // undo the tfp_mode binding
    virtual void bind();
    virtual void unbind();
    void setYInverted(bool inverted) {
        y_inverted = inverted;
    }

protected:
    void findTarget();
    QRegion optimizeBindDamage(const QRegion& reg, int limit);
    void createTexture();

private:
    void init();

#ifndef KWIN_HAVE_OPENGLES
    GLXPixmap glxpixmap; // the glx pixmap the texture is bound to, only for tfp_mode
#endif
};

class SceneOpenGL::Window
    : public Scene::Window
{
public:
    Window(Toplevel* c);
    virtual ~Window();
    virtual void performPaint(int mask, QRegion region, WindowPaintData data);
    virtual void pixmapDiscarded();
    bool bindTexture();
    void discardTexture();
    void checkTextureSize();

protected:
    enum TextureType {
        Content,
        DecorationTop,
        DecorationLeft,
        DecorationRight,
        DecorationBottom,
        Shadow
    };

    QMatrix4x4 transformation(int mask, const WindowPaintData &data) const;
    void paintDecoration(const QPixmap* decoration, TextureType decorationType, const QRegion& region, const QRect& rect, const WindowPaintData& data, const WindowQuadList& quads, bool updateDeco);
    void paintShadow(WindowQuadType type, const QRegion &region, const WindowPaintData &data);
    void makeDecorationArrays(const WindowQuadList& quads, const QRect& rect) const;
    void renderQuads(int mask, const QRegion& region, const WindowQuadList& quads);
    void prepareStates(TextureType type, double opacity, double brightness, double saturation, GLShader* shader);
    void prepareStates(TextureType type, double opacity, double brightness, double saturation, GLShader* shader, Texture *texture);
    void prepareRenderStates(TextureType type, double opacity, double brightness, double saturation, Texture *tex);
    void prepareShaderRenderStates(TextureType type, double opacity, double brightness, double saturation, GLShader* shader);
    void restoreStates(TextureType type, double opacity, double brightness, double saturation, GLShader* shader);
    void restoreStates(TextureType type, double opacity, double brightness, double saturation, GLShader* shader, Texture *texture);
    void restoreRenderStates(TextureType type, double opacity, double brightness, double saturation, Texture *tex);
    void restoreShaderRenderStates(TextureType type, double opacity, double brightness, double saturation, GLShader* shader);

private:
    Texture texture;
    Texture topTexture;
    Texture leftTexture;
    Texture rightTexture;
    Texture bottomTexture;
};

class SceneOpenGL::EffectFrame
    : public Scene::EffectFrame
{
public:
    EffectFrame(EffectFrameImpl* frame);
    virtual ~EffectFrame();

    virtual void free();
    virtual void freeIconFrame();
    virtual void freeTextFrame();
    virtual void freeSelection();

    virtual void render(QRegion region, double opacity, double frameOpacity);

    virtual void crossFadeIcon();
    virtual void crossFadeText();

    static void cleanup();

private:
    void updateTexture();
    void updateTextTexture();

    Texture* m_texture;
    Texture* m_textTexture;
    Texture* m_oldTextTexture;
    QPixmap* m_textPixmap; // need to keep the pixmap around to workaround some driver problems
    Texture* m_iconTexture;
    Texture* m_oldIconTexture;
    Texture* m_selectionTexture;
    GLVertexBuffer* m_unstyledVBO;

    static Texture* m_unstyledTexture;
    static QPixmap* m_unstyledPixmap; // need to keep the pixmap around to workaround some driver problems
    static void updateUnstyledTexture(); // Update OpenGL unstyled frame texture
};

/**
 * @short OpenGL implementation of Shadow.
 *
 * This class extends Shadow by the Elements required for OpenGL rendering.
 * @author Martin Gräßlin <mgraesslin@kde.org>
 **/
class SceneOpenGLShadow
    : public Shadow
{
public:
    SceneOpenGLShadow(Toplevel *toplevel);
    virtual ~SceneOpenGLShadow();

    /**
     * Returns the Texture for a specific ShadowQuad. The method takes care of performing
     * the Texture from Pixmap operation. The calling method can use the returned Texture
     * directly.
     * In error case the method returns @c NULL.
     * @return OpenGL Texture for the Shadow Quad. May be @c NULL.
     **/
    SceneOpenGL::Texture *textureForQuadType(WindowQuadType type);
private:
    SceneOpenGL::Texture m_shadowTextures[ShadowElementsCount];
};

} // namespace

#endif

#endif
