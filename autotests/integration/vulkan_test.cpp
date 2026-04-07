/*
    SPDX-FileCopyrightText: 2026 Diego Gomez <diego.gomez2005@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "core/gpumanager.h"
#include "core/renderdevice.h"
#include "vulkan/vulkan_descriptor.h"
#include "vulkan/vulkan_device.h"
#include "vulkan/vulkan_shader.h"
#include "vulkan/vulkan_texture.h"
#include "wayland_server.h"

namespace KWin
{

class VulkanTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testAllocateImage();
    void testUploadPattern();
    void testUpdateRegion();
    void testUpdateCorners();
    void testComputeShader();

private:
    VulkanDevice *m_device = nullptr;
};

void VulkanTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(qAppName()));
    kwinApp()->start();

    const auto &renderDevices = GpuManager::self()->renderDevices();
    for (const auto &dev : renderDevices) {
        if (dev->vulkanDevice()) {
            m_device = dev->vulkanDevice();
            break;
        }
    }
    if (!m_device) {
        QSKIP("No Vulkan device available");
    }
}

void VulkanTest::testAllocateImage()
{
    auto texture = VulkanTexture::allocate(m_device, vk::Format::eR8G8B8A8Unorm, QSize(64, 64),
                                           vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled);
    QVERIFY(texture != nullptr);
    QCOMPARE(texture->size(), QSize(64, 64));
    QCOMPARE(texture->format(), vk::Format::eR8G8B8A8Unorm);
}

void VulkanTest::testUploadPattern()
{
    const QSize size(7, 13);
    QImage source(size, QImage::Format_RGBA8888_Premultiplied);
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x) {
            if (y < size.height() / 2) {
                source.setPixelColor(x, y, QColor(255, 0, 0, 255));
            } else {
                source.setPixelColor(x, y, QColor(0, 0, 255, 255));
            }
        }
    }

    auto texture = VulkanTexture::upload(m_device, source,
                                         vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled);
    QVERIFY(texture != nullptr);
    QCOMPARE(texture->size(), size);

    QImage downloaded = texture->download();
    QVERIFY(!downloaded.isNull());
    QCOMPARE(downloaded.size(), size);

    QColor topPixel(downloaded.pixel(3, 2));
    QCOMPARE(topPixel.red(), 255);
    QCOMPARE(topPixel.blue(), 0);

    QColor bottomPixel(downloaded.pixel(3, 10));
    QCOMPARE(bottomPixel.red(), 0);
    QCOMPARE(bottomPixel.blue(), 255);
}

void VulkanTest::testUpdateRegion()
{
    QImage source(64, 64, QImage::Format_RGBA8888_Premultiplied);
    source.fill(QColor(0, 255, 0, 255));

    auto texture = VulkanTexture::upload(m_device, source,
                                         vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled);
    QVERIFY(texture != nullptr);

    QImage updateSource(64, 64, QImage::Format_RGBA8888_Premultiplied);
    updateSource.fill(QColor(0, 255, 0, 255));
    for (int y = 24; y < 40; ++y) {
        for (int x = 24; x < 40; ++x) {
            updateSource.setPixelColor(x, y, QColor(255, 0, 0, 255));
        }
    }

    Region dirtyRegion(Rect(24, 24, 16, 16));
    QVERIFY(texture->update(updateSource, dirtyRegion));

    QImage downloaded = texture->download();
    QVERIFY(!downloaded.isNull());

    QColor corner(downloaded.pixel(0, 0));
    QCOMPARE(corner.green(), 255);
    QCOMPARE(corner.red(), 0);

    QColor center(downloaded.pixel(32, 32));
    QCOMPARE(center.red(), 255);
    QCOMPARE(center.green(), 0);
}

void VulkanTest::testUpdateCorners()
{
    QImage source(64, 64, QImage::Format_RGBA8888_Premultiplied);
    source.fill(QColor(0, 0, 0, 255));

    auto texture = VulkanTexture::upload(m_device, source,
                                         vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled);
    QVERIFY(texture != nullptr);

    QImage updateSource(64, 64, QImage::Format_RGBA8888_Premultiplied);
    updateSource.fill(QColor(0, 0, 0, 255));

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            updateSource.setPixelColor(x, y, QColor(255, 0, 0, 255));
            updateSource.setPixelColor(56 + x, y, QColor(255, 0, 0, 255));
            updateSource.setPixelColor(x, 56 + y, QColor(255, 0, 0, 255));
            updateSource.setPixelColor(56 + x, 56 + y, QColor(255, 0, 0, 255));
        }
    }

    Region corners;
    corners += Rect(0, 0, 8, 8);
    corners += Rect(56, 0, 8, 8);
    corners += Rect(0, 56, 8, 8);
    corners += Rect(56, 56, 8, 8);
    QVERIFY(texture->update(updateSource, corners));

    QImage downloaded = texture->download();
    QVERIFY(!downloaded.isNull());

    QCOMPARE(QColor(downloaded.pixel(0, 0)).red(), 255);
    QCOMPARE(QColor(downloaded.pixel(63, 0)).red(), 255);
    QCOMPARE(QColor(downloaded.pixel(0, 63)).red(), 255);
    QCOMPARE(QColor(downloaded.pixel(63, 63)).red(), 255);

    QCOMPARE(QColor(downloaded.pixel(32, 32)).red(), 0);
}

void VulkanTest::testComputeShader()
{
    const std::array layouts{
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute),
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute),
    };
    const auto shader = VulkanShader::compileFromPath(m_device, QT_TESTCASE_BUILDDIR + QStringLiteral("/data/vulkan_test_shader.spv"), layouts);
    QVERIFY(shader);

    QImage img(64, 64, QImage::Format_RGBA8888_Premultiplied);
    for (int x = 0; x < img.width(); x++) {
        for (int y = 0; y < img.width(); y++) {
            img.setPixel(x, y, qRgba(x, y, 0, 255));
        }
    }
    auto src = VulkanTexture::upload(m_device, img, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);
    auto dst = VulkanTexture::allocate(m_device, vk::Format::eR8G8B8A8Unorm, QSize(64, 64),
                                       vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage);

    auto cmd = m_device->computeQueue()->createCommandBuffer();
    QCOMPARE(cmd.begin(vk::CommandBufferBeginInfo{
                 vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
             }),
             vk::Result::eSuccess);

    cmd.bindShadersEXT(vk::ShaderStageFlagBits::eCompute, *shader->handle());

    auto outDescriptor = VulkanDescriptor::allocate(m_device, shader->descriptorSetLayouts()[0], vk::DescriptorType::eStorageImage);
    QVERIFY(outDescriptor);
    outDescriptor->write(dst.get());

    auto inDescriptor = VulkanDescriptor::allocate(m_device, shader->descriptorSetLayouts()[1], vk::DescriptorType::eCombinedImageSampler);
    QVERIFY(inDescriptor);
    inDescriptor->write(src.get());

    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *shader->pipelineLayout(), 0, {*outDescriptor->set(), *inDescriptor->set()}, {});
    cmd.dispatch(dst->size().width() / 16, dst->size().height() / 16, 1);

    QCOMPARE(cmd.end(), vk::Result::eSuccess);
    QVERIFY(m_device->computeQueue()->submitBlocking(std::move(cmd)));

    QCOMPARE(img, dst->download());
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::VulkanTest)
#include "vulkan_test.moc"
