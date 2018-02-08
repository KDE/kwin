/**************************************************************************
 * KWin - the KDE window manager                                          *
 * This file is part of the KDE project.                                  *
 *                                                                        *
 * Copyright © 2017-2018 Fredrik Höglund <fredrik@kde.com>                *
 *                                                                        *
 * This program is free software; you can redistribute it and/or modify   *
 * it under the terms of the GNU General Public License as published by   *
 * the Free Software Foundation; either version 2 of the License, or      *
 * (at your option) any later version.                                    *
 *                                                                        *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 **************************************************************************/


#include "vulkandevicemodel.h"

#include <QAbstractItemModel>
#include <QObject>

#include "config-kwin.h"
#include "kwinvulkanutils.h"

namespace KWin {
namespace Compositing {


VulkanDeviceModel::VulkanDeviceModel(QObject *parent)
    : QAbstractListModel(parent)
{
    const VulkanExtensionVector supportedExtensions = VulkanInstance::supportedExtensions();
    std::vector<const char *> instanceExtensions;

    if (supportedExtensions.contains(VK_KHR_SURFACE_EXTENSION_NAME))
        instanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

    // Create the vulkan instance
    // --------------------------
    const VkApplicationInfo applicationInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "kcmkwincompositing",
        .applicationVersion = VK_MAKE_VERSION(KWIN_VERSION_MAJOR, KWIN_VERSION_MINOR, KWIN_VERSION_PATCH),
        .pEngineName = nullptr,
        .engineVersion = VK_MAKE_VERSION(0, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };

    VulkanInstance instance(applicationInfo, {}, instanceExtensions);
    if (!instance.isValid()) {
        qWarning() << "Failed to create a Vulkan instance";
        return;
    }

    // FIXME: We should create a dummy surface for the platform, and verify
    //        that the devices can present to the surface.

    // Enumerate the available GPU's
    // -----------------------------
    const char *requiredDeviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    std::vector<VulkanPhysicalDevice> devices = instance.enumeratePhysicalDevices();

    for (uint32_t index = 0; index < devices.size(); index++) {
        VulkanPhysicalDevice &device = devices[index];

        // The device must support all required extensions
        const VulkanExtensionVector supportedDeviceExtensions = device.enumerateDeviceExtensionProperties();
        if (!supportedDeviceExtensions.containsAll(requiredDeviceExtensions))
            continue;

        // The device must support sampling linear VK_FORMAT_B8G8R8A8_UNORM images.
        // This is used to upload decoration textures.
        // AMD, Intel, and NVIDIA all support this.
        VkFormatProperties formatProperties;
        device.getFormatProperties(VK_FORMAT_B8G8R8A8_UNORM, &formatProperties);

        if (!(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
            continue;

        // Enumerate the available queue families
        const std::vector<VkQueueFamilyProperties> queueFamilies = device.getQueueFamilyProperties();

        // Find a queue family that supports graphics
        bool supportsGraphics = false;

        for (unsigned i = 0; i < queueFamilies.size(); i++) {
            const VkQueueFamilyProperties &family = queueFamilies[i];

            if (family.queueCount < 1)
                continue;

            // Note that the VK_QUEUE_TRANSFER_BIT does not need to be set when
            // VK_QUEUE_GRAPHICS_BIT is set; transfer support is implied
            if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                supportsGraphics = true;
                break;
	    }
        }

        // The device must support graphics and presentation
        if (!supportsGraphics)
            continue;

        VkPhysicalDeviceProperties properties;
        device.getProperties(&properties);

        const struct {
            VkPhysicalDeviceType type;
            int score;
        } scores[] = {
            { VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, 500 },
            { VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,   400 },
            { VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,    300 },
            { VK_PHYSICAL_DEVICE_TYPE_CPU,            200 }
        };

        // Assign a score to the device based on the type
        int score = 100;

        const auto it = std::find_if(std::cbegin(scores), std::cend(scores),
                                     [&](const auto &entry) {
                                         return entry.type == properties.deviceType;
                                     });

        if (it != std::cend(scores))
            score = it->score;

	m_devices.append(new Device {
                            .index = index,
			    .name = QString::fromUtf8(properties.deviceName),
                            .vendorId = properties.vendorID,
                            .deviceId = properties.deviceID,
                            .deviceType = properties.deviceType,
                            .driverVersion = properties.driverVersion,
                            .score = score
                        });
    }

    std::stable_sort(m_devices.begin(), m_devices.end(), [](const Device *lhs, const Device *rhs) {
                         return lhs->score < rhs->score;
                     });
}


VulkanDeviceModel::~VulkanDeviceModel()
{
    qDeleteAll(m_devices);
}


int VulkanDeviceModel::indexOf(uint32_t index, uint32_t vendorId, uint32_t deviceId) const
{
    for (int i = 0; i < m_devices.count(); i++) {
        auto *device = m_devices.at(i);
        if (device->index == index &&
            device->vendorId == vendorId &&
            device->deviceId == deviceId)
            return i;
    }

    return -1;
}


bool VulkanDeviceModel::deviceAt(int row, uint32_t *index, uint32_t *vendorId, uint32_t *deviceId) const
{
    if (row < 0 || row >= m_devices.count())
        return false;

    *index = m_devices.at(row)->index;
    *vendorId = m_devices.at(row)->vendorId;
    *deviceId = m_devices.at(row)->deviceId;
    return true;
}


int VulkanDeviceModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
       return 0;

    return m_devices.count();
}


QVariant VulkanDeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() > m_devices.count() || index.column() < 0 || index.column() > 0)
        return QVariant();

    if (role == Qt::DisplayRole)
        return m_devices.at(index.row())->name;

    return QVariant();
}


QVariant VulkanDeviceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        if (section == 0)
            return QStringLiteral("Name");
    } else {
        return QString::number(section + 1);
    }

    return QVariant();
}


} // namespace Compositing
} // namespace KWin
