/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "libkwineffects/kwinglutils_export.h"

#include <QRect>
#include <QStack>
#include <epoxy/gl.h>

namespace KWin
{

class GLTexture;
class RenderTarget;
class RenderViewport;

// Cleans up all resources hold by the GL Context
void KWINGLUTILS_EXPORT cleanupGL();

/**
 * @short OpenGL framebuffer object
 *
 * Framebuffer object enables you to render onto a texture. This texture can
 * later be used to e.g. do post-processing of the scene.
 *
 * @author Rivo Laks <rivolaks@hot.ee>
 */
class KWINGLUTILS_EXPORT GLFramebuffer
{
public:
    enum Attachment {
        NoAttachment,
        CombinedDepthStencil,
    };

    /**
     * Constructs a GLFramebuffer
     * @since 5.13
     */
    explicit GLFramebuffer();

    /**
     * Constructs a GLFramebuffer. Note that ensuring the color attachment outlives
     * the framebuffer is the responsibility of the caller.
     *
     * @param colorAttachment texture where the scene will be rendered onto
     */
    explicit GLFramebuffer(GLTexture *colorAttachment, Attachment attachment = NoAttachment);

    /**
     * Constructs a wrapper for an already created framebuffer object. The GLFramebuffer
     * does not take the ownership of the framebuffer object handle.
     */
    GLFramebuffer(GLuint handle, const QSize &size);
    ~GLFramebuffer();

    /**
     * Returns the framebuffer object handle to this framebuffer object.
     */
    GLuint handle() const
    {
        return mFramebuffer;
    }
    /**
     * Returns the size of the color attachment to this framebuffer object.
     */
    QSize size() const
    {
        return mSize;
    }
    bool valid() const
    {
        return mValid;
    }

    static void initStatic();
    static bool supported()
    {
        return sSupported;
    }

    /**
     * Returns the last bound framebuffer, or @c null if no framebuffer is current.
     */
    static GLFramebuffer *currentFramebuffer();

    static void pushFramebuffer(GLFramebuffer *fbo);
    static GLFramebuffer *popFramebuffer();
    /**
     * Whether the GL_EXT_framebuffer_blit extension is supported.
     * This functionality is not available in OpenGL ES 2.0.
     *
     * @returns whether framebuffer blitting is supported.
     * @since 4.8
     */
    static bool blitSupported();

    /**
     * Blits from @a source rectangle in the current framebuffer to the @a destination rectangle in
     * this framebuffer.
     *
     * Be aware that framebuffer blitting may not be supported on all hardware. Use blitSupported()
     * to check whether it is supported.
     *
     * The @a source and the @a destination rectangles can have different sizes. The @a filter indicates
     * what filter will be used in case scaling needs to be performed.
     *
     * @see blitSupported
     * @since 4.8
     */
    void blitFromFramebuffer(const QRect &source = QRect(), const QRect &destination = QRect(), GLenum filter = GL_LINEAR, bool flipX = false, bool flipY = false);

    /**
     * Blits from @a source rectangle in logical coordinates in the current framebuffer to the @a destination rectangle in texture-local coordinates
     * in this framebuffer, taking into account any transformations the source render target may have
     */
    bool blitFromRenderTarget(const RenderTarget &sourceRenderTarget, const RenderViewport &sourceViewport, const QRect &source, const QRect &destination);

    /**
     * @returns the color attachment of this fbo. May be nullptr
     */
    GLTexture *colorAttachment() const;

protected:
    void initColorAttachment(GLTexture *colorAttachment);
    void initDepthStencilAttachment();

private:
    bool bind();

    friend void KWin::cleanupGL();
    static void cleanup();
    static bool sSupported;
    static bool sSupportsPackedDepthStencil;
    static bool sSupportsDepth24;
    static bool s_blitSupported;
    static QStack<GLFramebuffer *> s_fbos;

    GLuint mFramebuffer = 0;
    GLuint mDepthBuffer = 0;
    GLuint mStencilBuffer = 0;
    QSize mSize;
    bool mValid = false;
    bool mForeign = false;
    GLTexture *const m_colorAttachment;
};

}
