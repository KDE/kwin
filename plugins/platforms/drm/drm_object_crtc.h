/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Roman Gilg <subdiff@gmail.com>

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
#ifndef KWIN_DRM_OBJECT_CRTC_H
#define KWIN_DRM_OBJECT_CRTC_H

#include "drm_object.h"

namespace KWin
{

class DrmBackend;
class DrmBuffer;
class DrmDumbBuffer;

class DrmCrtc : public DrmObject
{
public:
    DrmCrtc(uint32_t crtc_id, DrmBackend *backend, int resIndex);

    virtual ~DrmCrtc();

    bool atomicInit() Q_DECL_OVERRIDE;

    enum class PropertyIndex {
        ModeId = 0,
        Active,
        Count
    };
    
    bool initProps() Q_DECL_OVERRIDE;

    int resIndex() const {
        return m_resIndex;
    }

    DrmBuffer *current() {
        return m_currentBuffer;
    }
    DrmBuffer *next() {
        return m_nextBuffer;
    }
    void setNext(DrmBuffer *buffer) {
        m_nextBuffer = buffer;
    }

    void flipBuffer();
    bool blank();

private:
    int m_resIndex;

    DrmBuffer *m_currentBuffer = nullptr;
    DrmBuffer *m_nextBuffer = nullptr;
    DrmDumbBuffer *m_blackBuffer = nullptr;
};

}

#endif

