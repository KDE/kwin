/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwineffects.h"
#include "libkwineffects/kwinglutils_export.h"

#include <QColor>
#include <QRegion>
#include <epoxy/gl.h>
#include <optional>
#include <ranges>
#include <span>

namespace KWin
{

enum VertexAttributeType {
    VA_Position = 0,
    VA_TexCoord = 1,
    VertexAttributeCount = 2,
};

/**
 * Describes the format of a vertex attribute stored in a buffer object.
 *
 * The attribute format consists of the attribute index, the number of
 * vector components, the data type, and the offset of the first element
 * relative to the start of the vertex data.
 */
struct GLVertexAttrib
{
    size_t attributeIndex;
    size_t componentCount;
    GLenum type; /** The type (e.g. GL_FLOAT) */
    size_t relativeOffset; /** The relative offset of the attribute */
};

class GLVertexBufferPrivate;

/**
 * @short Vertex Buffer Object
 *
 * This is a short helper class to use vertex buffer objects (VBO). A VBO can be used to buffer
 * vertex data and to store them on graphics memory.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
 * @since 4.6
 */
class KWINGLUTILS_EXPORT GLVertexBuffer
{
public:
    /**
     * Enum to define how often the vertex data in the buffer object changes.
     */
    enum UsageHint {
        Dynamic, ///< frequent changes, but used several times for rendering
        Static, ///< No changes to data
        Stream ///< Data only used once for rendering, updated very frequently
    };

    explicit GLVertexBuffer(UsageHint hint);
    ~GLVertexBuffer();

    /**
     * Specifies how interleaved vertex attributes are laid out in
     * the buffer object.
     *
     * Note that the attributes and the stride should be 32 bit aligned
     * or a performance penalty may be incurred.
     *
     * For some hardware the optimal stride is a multiple of 32 bytes.
     *
     * Example:
     *
     *     struct Vertex {
     *         QVector3D position;
     *         QVector2D texcoord;
     *     };
     *
     *     const std::array attribs = {
     *         GLVertexAttrib{ VA_Position, 3, GL_FLOAT, offsetof(Vertex, position) },
     *         GLVertexAttrib{ VA_TexCoord, 2, GL_FLOAT, offsetof(Vertex, texcoord) },
     *     };
     *
     *     Vertex vertices[6];
     *     vbo->setAttribLayout(std::span(attribs), sizeof(Vertex));
     *     vbo->setData(vertices, sizeof(vertices));
     */
    void setAttribLayout(std::span<const GLVertexAttrib> attribs, size_t stride);

    /**
     * Uploads data into the buffer object's data store.
     */
    void setData(const void *data, size_t sizeInBytes);

    /**
     * Sets the number of vertices that will be drawn by the render() method.
     */
    void setVertexCount(int count);

    // clang-format off
    template<std::ranges::contiguous_range T>
        requires std::is_same<std::ranges::range_value_t<T>, GLVertex2D>::value
    void setVertices(const T &range)
    {
        setData(range.data(), range.size() * sizeof(GLVertex2D));
        setVertexCount(range.size());
        setAttribLayout(std::span(GLVertex2DLayout), sizeof(GLVertex2D));
    }

    template<std::ranges::contiguous_range T>
        requires std::is_same<std::ranges::range_value_t<T>, GLVertex3D>::value
    void setVertices(const T &range)
    {
        setData(range.data(), range.size() * sizeof(GLVertex3D));
        setVertexCount(range.size());
        setAttribLayout(std::span(GLVertex3DLayout), sizeof(GLVertex3D));
    }

    template<std::ranges::contiguous_range T>
        requires std::is_same<std::ranges::range_value_t<T>, QVector2D>::value
    void setVertices(const T &range)
    {
        setData(range.data(), range.size() * sizeof(QVector2D));
        setVertexCount(range.size());
        static constexpr GLVertexAttrib layout{
            .attributeIndex = VA_Position,
            .componentCount = 2,
            .type = GL_FLOAT,
            .relativeOffset = 0,
        };
        setAttribLayout(std::span(&layout, 1), sizeof(QVector2D));
    }
    // clang-format on

    /**
     * Binds the vertex arrays to the context.
     */
    void bindArrays();

    /**
     * Disables the vertex arrays.
     */
    void unbindArrays();

    /**
     * Draws count vertices beginning with first.
     */
    void draw(GLenum primitiveMode, int first, int count);

    /**
     * Draws count vertices beginning with first.
     */
    void draw(const QRegion &region, GLenum primitiveMode, int first, int count, bool hardwareClipping = false);

    /**
     * Renders the vertex data in given @a primitiveMode.
     * Please refer to OpenGL documentation of glDrawArrays or glDrawElements for allowed
     * values for @a primitiveMode. Best is to use GL_TRIANGLES or similar to be future
     * compatible.
     */
    void render(GLenum primitiveMode);
    /**
     * Same as above restricting painting to @a region if @a hardwareClipping is true.
     * It's within the caller's responsibility to enable GL_SCISSOR_TEST.
     */
    void render(const QRegion &region, GLenum primitiveMode, bool hardwareClipping = false);

    /**
     * @internal
     */
    static void initStatic();

    /**
     * @internal
     */
    static void cleanup();

    static constexpr std::array GLVertex2DLayout{
        GLVertexAttrib{
            .attributeIndex = VA_Position,
            .componentCount = 2,
            .type = GL_FLOAT,
            .relativeOffset = offsetof(GLVertex2D, position),
        },
        GLVertexAttrib{
            .attributeIndex = VA_TexCoord,
            .componentCount = 2,
            .type = GL_FLOAT,
            .relativeOffset = offsetof(GLVertex2D, texcoord),
        },
    };
    static constexpr std::array GLVertex3DLayout{
        GLVertexAttrib{
            .attributeIndex = VA_Position,
            .componentCount = 3,
            .type = GL_FLOAT,
            .relativeOffset = offsetof(GLVertex3D, position),
        },
        GLVertexAttrib{
            .attributeIndex = VA_TexCoord,
            .componentCount = 2,
            .type = GL_FLOAT,
            .relativeOffset = offsetof(GLVertex3D, texcoord),
        },
    };

private:
    const std::unique_ptr<GLVertexBufferPrivate> d;
};

}
