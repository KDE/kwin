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


#ifndef VULKANDEVICEMODEL_H
#define VULKANDEVICEMODEL_H

#include <QAbstractListModel>

#include <kwinvulkanutils_funcs.h>

namespace KWin {
namespace Compositing {


class VulkanDeviceModel : public QAbstractListModel
{
    struct Device
    {
        uint32_t index;
        QString name;
        uint32_t vendorId;
        uint32_t deviceId;
        VkPhysicalDeviceType deviceType;
        uint32_t driverVersion;
        int score;
    };

public:
    enum Role {
        IndexRole         = Qt::UserRole + 1,
        VendorIdRole      = Qt::UserRole + 3,
        DeviceIdRole      = Qt::UserRole + 2,
        DeviceTypeRole    = Qt::UserRole + 4,
        DriverVersionRole = Qt::UserRole + 5
    };

    explicit VulkanDeviceModel(QObject *parent = nullptr);
    ~VulkanDeviceModel() override final;

    int indexOf(uint32_t index, uint32_t vendorId, uint32_t deviceId) const;
    bool deviceAt(int row, uint32_t *index, uint32_t *vendorId, uint32_t *deviceId) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override final;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override final;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override final;

private:
    QVector<Device *> m_devices;
};


} // namespace Compositing
} // namespace KWin

#endif // VULKANDEVICEMODEL_H
