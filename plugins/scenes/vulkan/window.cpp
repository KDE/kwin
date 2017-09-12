/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright © 2017-2018 Fredrik Höglund <fredrik@kde.org>

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

#include "window.h"
#include "windowpixmap.h"
#include "descriptorset.h"
#include "abstract_client.h"
#include "client.h"
#include "deleted.h"

#include <QGraphicsScale>

#define GL_QUADS 0x0007


namespace KWin
{


VulkanWindow::VulkanWindow(Toplevel *toplevel, VulkanScene *scene)
    : Scene::Window(toplevel),
      m_scene(scene)
{
}


VulkanWindow::~VulkanWindow()
{
}


QMatrix4x4 VulkanWindow::windowMatrix(int mask, const WindowPaintData &data) const
{
    QMatrix4x4 matrix;
    matrix.translate(x(), y());

    if (!(mask & Scene::PAINT_WINDOW_TRANSFORMED))
        return matrix;

    matrix.translate(data.translation());
    data.scale().applyTo(&matrix);

    if (data.rotationAngle() == 0.0)
        return matrix;

    // Apply the rotation
    // We cannot use data.rotation.applyTo(&matrix) as QGraphicsRotation uses projectedRotate to map back to 2D
    matrix.translate(data.rotationOrigin());
    const QVector3D axis = data.rotationAxis();
    matrix.rotate(data.rotationAngle(), axis.x(), axis.y(), axis.z());
    matrix.translate(-data.rotationOrigin());

    return matrix;
}


QMatrix4x4 VulkanWindow::modelViewProjectionMatrix(int mask, const WindowPaintData &data) const
{
    const QMatrix4x4 pMatrix = data.projectionMatrix();
    const QMatrix4x4 mvMatrix = data.modelViewMatrix();

    // An effect may want to override the default projection matrix in some cases,
    // such as when it is rendering a window on a render target that doesn't have
    // the same dimensions as the default framebuffer.
    //
    // Note that the screen transformation is not applied here.
    if (!pMatrix.isIdentity())
        return pMatrix * mvMatrix;

    // If an effect has specified a model-view matrix, we multiply that matrix
    // with the default projection matrix.  If the effect hasn't specified a
    // model-view matrix, mvMatrix will be the identity matrix.
    if (mask & Scene::PAINT_SCREEN_TRANSFORMED)
        return scene()->screenProjectionMatrix() * mvMatrix;

    return scene()->projectionMatrix() * mvMatrix;
}


// This method cannot be const because windowPixmap<T>() is not const
VulkanWindow::Texture VulkanWindow::getContentTexture()
{
    // Note that windowPixmap<T>() returns a pointer to the window pixmap,
    // casting it to a pointer to T
    if (auto pixmap = windowPixmap<VulkanWindowPixmap>()) {
        pixmap->aboutToRender();
        return Texture(pixmap->image(), pixmap->imageView(), pixmap->memory(), pixmap->imageLayout());
    }

    return Texture();
}


// This method cannot be const because previousWindowPixmap<T>() is not const
VulkanWindow::Texture VulkanWindow::getPreviousContentTexture()
{
    // Note that previousWindowPixmap<T>() returns a pointer to the previous window pixmap,
    // casting it to a pointer to T
    if (auto pixmap = previousWindowPixmap<VulkanWindowPixmap>())
        return Texture(pixmap->image(), pixmap->imageView(), pixmap->memory(), pixmap->imageLayout());

    return Texture();
}


void VulkanWindow::performPaint(int mask, QRegion clipRegion, WindowPaintData data)
{
    if (data.quads.isEmpty())
        return;

    // The x and y members of the scissor offset must be >= 0.
    // Note that it is legal for x + width and y + height to exceed the dimensions of the framebuffer.
    const QPoint clipOffset = clipRegion.boundingRect().topLeft();
    if (clipOffset.x() < 0 || clipOffset.y() < 0)
        clipRegion &= QRect(0, 0, INT_MAX, INT_MAX);

    if (clipRegion.isEmpty())
        return;

    auto pipelineManager = scene()->pipelineManager();
    auto uploadManager   = scene()->uploadManager();
    auto cmd             = scene()->mainCommandBuffer();

    const Texture contentTexture         = getContentTexture();
    const Texture previousContentTexture = getPreviousContentTexture();

    // Select the pipeline for the window contents
    // -------------------------------------------
    VulkanPipelineManager::Material contentMaterial    = VulkanPipelineManager::Texture;
    VulkanPipelineManager::Traits contentTraits        = VulkanPipelineManager::NoTraits;

    if (!isOpaque() || data.opacity() != 1.0)
        contentTraits |= VulkanPipelineManager::PreMultipliedAlphaBlend;

    if (data.opacity() != 1.0 || data.brightness() != 1.0) {
        contentTraits |= VulkanPipelineManager::Modulate;
    }

    if (data.saturation() != 1.0) {
        contentTraits |= VulkanPipelineManager::Desaturate;
    }

    if (data.crossFadeProgress() != 1.0 && previousContentTexture.imageView()) {
        contentMaterial = VulkanPipelineManager::TwoTextures;
        contentTraits |= VulkanPipelineManager::CrossFade;
    }

    VkPipeline contentPipeline;
    VkPipelineLayout contentPipelineLayout;

    std::tie(contentPipeline, contentPipelineLayout) =
            pipelineManager->pipeline(contentMaterial, contentTraits,
                                      VulkanPipelineManager::DescriptorSet,
                                      VulkanPipelineManager::TriangleList,
                                      VulkanPipelineManager::SwapchainRenderPass);

    // Upload the uniform buffer data
    const QMatrix4x4 mvpMatrix = modelViewProjectionMatrix(mask, data) * windowMatrix(mask, data);
    const VulkanBufferRange ubo = uploadManager->emplaceUniform<VulkanPipelineManager::TextureUniformData>(mvpMatrix,
                                                                                                           data.opacity(),
                                                                                                           data.brightness(),
                                                                                                           data.saturation(),
                                                                                                           data.crossFadeProgress());

    // Select the sampler
    const VkSampler sampler = (mask & (Effect::PAINT_WINDOW_TRANSFORMED | Effect::PAINT_SCREEN_TRANSFORMED)) ?
            scene()->linearSampler() : scene()->nearestSampler();

    WindowQuadList decorationQuads;
    WindowQuadList contentQuads;
    WindowQuadList shadowQuads;

    // Split the quads into separate lists for each type
    for (const WindowQuad &quad : qAsConst(data.quads)) {
        switch (quad.type()) {
        case WindowQuadDecoration:
            decorationQuads.append(quad);
            continue;

        case WindowQuadContents:
            contentQuads.append(quad);
            continue;

        case WindowQuadShadow:
            shadowQuads.append(quad);
            continue;

        default:
            continue;
        }
    }

    VulkanClippedDrawHelper clip(cmd, clipRegion);


    // Draw the contents
    // -----------------
    if (!contentQuads.isEmpty() && contentTexture) {
        if (!(contentTraits & VulkanPipelineManager::CrossFade)) {
            // Allocate and upload vertices
            auto vbo = uploadManager->allocate(contentQuads.count() * 4 * sizeof(GLVertex2D));
            GLVertex2D *vertices = static_cast<GLVertex2D *>(vbo.data());

            contentQuads.makeInterleavedArrays(GL_QUADS, vertices, QMatrix4x4());

            const auto &view = contentTexture.imageView();
            const auto imageLayout = contentTexture.imageLayout();
            auto &set = m_contentDescriptorSet;

            // Update the descriptor set if necessary
            if (!set || set->sampler() != sampler || set->imageView() != view || set->imageLayout() != imageLayout || set->uniformBuffer() != ubo.buffer()) {
                // Orphan the descriptor set if it is busy
                if (!set || set.use_count() > 1)
                    set = std::make_shared<TextureDescriptorSet>(scene()->textureDescriptorPool());

                set->update(sampler, view, imageLayout, ubo.buffer());
            }

            cmd->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, contentPipeline);
            cmd->bindIndexBuffer(scene()->indexBufferForQuadCount(contentQuads.count()), 0, VK_INDEX_TYPE_UINT16);
            cmd->bindVertexBuffers(0, { vbo.handle() }, { vbo.offset() });
            cmd->bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, contentPipelineLayout, 0, { set->handle() }, { (uint32_t) ubo.offset() });

            clip.drawIndexed(contentQuads.count() * 6, 1, 0, 0, 0);

            scene()->addBusyReference(set);
            scene()->addBusyReference(contentTexture.image());
            scene()->addBusyReference(contentTexture.imageView());
            scene()->addBusyReference(contentTexture.memory());

            // Drop the reference to the cross-fade descriptor set if we are not cross-fading
            m_crossFadeDescriptorSet = nullptr;
        } else {
            // Allocate and upload vertices
            auto vbo = uploadManager->allocate(contentQuads.count() * 4 * sizeof(GLCrossFadeVertex2D));
            GLCrossFadeVertex2D *vertex = static_cast<GLCrossFadeVertex2D *>(vbo.data());

            VulkanWindowPixmap *previous = previousWindowPixmap<VulkanWindowPixmap>();
            const QRect &oldGeometry = previous->contentsRect();

            for (const WindowQuad &quad : qAsConst(contentQuads)) {
                for (int i = 0; i < 4; ++i) {
                    const double xFactor = double(quad[i].u() -  toplevel->clientPos().x()) / double(toplevel->clientSize().width());
                    const double yFactor = double(quad[i].v() -  toplevel->clientPos().y()) / double(toplevel->clientSize().height());

                    *vertex++ = {
                        .position  = { float(quad[i].x()),                                      float(quad[i].y())                                      },
                        .texcoord1 = { float(xFactor * oldGeometry.width()  + oldGeometry.x()), float(yFactor * oldGeometry.height() + oldGeometry.y()) },
                        .texcoord2 = { float(quad[i].u()),                                      float(quad[i].v())                                      }
                    };
                }
            }

            const auto &currView   = contentTexture.imageView();
            const auto &prevView   = previousContentTexture.imageView();
            const auto imageLayout = contentTexture.imageLayout();
            auto &set = m_crossFadeDescriptorSet;

            // Update the descriptor set if necessary
            if (!set ||
                set->sampler() != sampler ||
                set->imageView1() != prevView ||
                set->imageView2() != currView ||
                set->imageLayout() != imageLayout ||
                set->uniformBuffer() != ubo.buffer()) {
                // Orphan the descriptor set if it is busy
                if (!set || set.use_count() > 1)
                    set = std::make_shared<CrossFadeDescriptorSet>(scene()->crossFadeDescriptorPool());

                set->update(sampler, prevView, currView, imageLayout, ubo.buffer());
            }

            cmd->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, contentPipeline);
            cmd->bindVertexBuffers(0, { vbo.handle() }, { vbo.offset() });
            cmd->bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, contentPipelineLayout, 0, { set->handle() }, { (uint32_t) ubo.offset() });

            clip.drawIndexed(contentQuads.count() * 6, 1, 0, 0, 0);

            scene()->addBusyReference(set);

            scene()->addBusyReference(contentTexture.image());
            scene()->addBusyReference(contentTexture.imageView());
            scene()->addBusyReference(contentTexture.memory());

            scene()->addBusyReference(previousContentTexture.image());
            scene()->addBusyReference(previousContentTexture.imageView());
            scene()->addBusyReference(previousContentTexture.memory());
        }
    }
}


WindowPixmap *VulkanWindow::createWindowPixmap()
{
    return new VulkanWindowPixmap(this, m_scene);
}


} // namespace KWin
