/*
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>
    SPDX-FileCopyrightText: 2021 Méven Car <meven.car@enioka.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "qwayland-server-kde-output-management-v2.h"

#include "outputchangeset_v2.h"
#include "outputdevice_v2_interface.h"
#include "outputmanagement_v2_interface.h"

#include <QHash>

#include <optional>

namespace KWaylandServer
{

class OutputConfigurationV2Interface : public QtWaylandServer::kde_output_configuration_v2
{
public:
    explicit OutputConfigurationV2Interface(wl_resource *resource);
    ~OutputConfigurationV2Interface() override;

    OutputChangeSetV2 *pendingChanges(OutputDeviceV2Interface *outputdevice);

    bool applied = false;
    QHash<OutputDeviceV2Interface *, OutputChangeSetV2 *> changes;
    std::optional<OutputDeviceV2Interface *> primaryOutput;

protected:
    void kde_output_configuration_v2_enable(Resource *resource, wl_resource *outputdevice, int32_t enable) override;
    void kde_output_configuration_v2_mode(Resource *resource, struct ::wl_resource *outputdevice, struct ::wl_resource *mode) override;
    void kde_output_configuration_v2_transform(Resource *resource, wl_resource *outputdevice, int32_t transform) override;
    void kde_output_configuration_v2_position(Resource *resource, wl_resource *outputdevice, int32_t x, int32_t y) override;
    void kde_output_configuration_v2_scale(Resource *resource, wl_resource *outputdevice, wl_fixed_t scale) override;
    void kde_output_configuration_v2_apply(Resource *resource) override;
    void kde_output_configuration_v2_destroy(Resource *resource) override;
    void kde_output_configuration_v2_destroy_resource(Resource *resource) override;
    void kde_output_configuration_v2_overscan(Resource *resource, wl_resource *outputdevice, uint32_t overscan) override;
    void kde_output_configuration_v2_set_vrr_policy(Resource *resource, struct ::wl_resource *outputdevice, uint32_t policy) override;
    void kde_output_configuration_v2_set_rgb_range(Resource *resource, wl_resource *outputdevice, uint32_t rgbRange) override;
    void kde_output_configuration_v2_set_primary_output(Resource *resource, struct ::wl_resource *output) override;
};

}
