/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "linux_drm_syncobj_v1.h"

namespace KWin
{

class LinuxDrmSyncObjTimelineV1 : public QtWaylandServer::wp_linux_drm_syncobj_timeline_v1
{
public:
    explicit LinuxDrmSyncObjTimelineV1(wl_client *client, uint32_t id, std::unique_ptr<SyncTimeline> &&timeline);
    ~LinuxDrmSyncObjTimelineV1() override;

    /**
     * May return nullptr if the timeline resource is inert
     */
    std::shared_ptr<SyncTimeline> timeline() const;

private:
    void wp_linux_drm_syncobj_timeline_v1_destroy_resource(Resource *resource) override;
    void wp_linux_drm_syncobj_timeline_v1_destroy(Resource *resource) override;

    const std::shared_ptr<SyncTimeline> m_timeline;
};
}
