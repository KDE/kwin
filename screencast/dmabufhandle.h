/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtGlobal>

#include <epoxy/egl.h>
#include <fixx11h.h>

namespace KWin
{

class GLTexture;

/**
 * The DmaBufHandle class represents a dmabuf handle for an exported OpenGL texture.
 */
class DmaBufHandle
{
public:
    DmaBufHandle();
    ~DmaBufHandle();

    /**
     * Returns the exported OpenGL texture.
     */
    GLTexture *texture() const;

    /**
     * Sets the @a texture to be exported with this handle. This function doesn't have
     * any effect after create() has been called.
     */
    void setTexture(GLTexture *texture);

    /**
     * Returns the EGL context associated with this dma-buf handle.
     */
    EGLContext context() const;

    /**
     * Sets the EGL context associated with this dma-buf handle. This function doesn't
     * have any effect after create() has been called.
     */
    void setContext(EGLContext context);

    /**
     * Attempts to export the texture. Returns @c true if the texture was successfully
     * exported; otherwise returns @c false.
     */
    bool create();

    /**
     * Destroys the dmabuf handle.
     */
    void destroy();

    /**
     * Returns the total number of planes. This function returns @c 0 if the handle is
     * invalid.
     */
    int planeCount() const;

    /**
     * Returns the pixel format of the exported buffer. This function returns @c 0 if
     * the handle is invalid.
     */
    int fourcc() const;

    /**
     * Returns the file descriptor for this handle. This function returns @c -1 if the
     * handle is invalid.
     */
    int fileDescriptor(int plane) const;

    /**
     * Returns the stride value for the specified plane @a plane. This function returns
     * @c 0 if the handle is invalid.
     */
    int stride(int plane) const;

    /**
     * Returns the offset value for the specified plane @a plane. This function returns
     * @c 0 if the handle is invalid.
     */
    int offset(int plane) const;

private:
    EGLDisplay m_eglDisplay;
    EGLImageKHR m_eglImage = EGL_NO_IMAGE_KHR;
    EGLContext m_eglContext = EGL_NO_CONTEXT;
    GLTexture *m_texture = nullptr;
    int m_planeCount = 0;
    int m_fourcc = 0;
    std::array<int, 4> m_fileDescriptors;
    std::array<int, 4> m_strides;
    std::array<int, 4> m_offsets;
    bool m_isCreated = false;
};

} // namespace KWin
