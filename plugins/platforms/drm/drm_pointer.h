/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2019 Vlad Zahorodnii <vladzzag@gmail.com>

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
#ifndef KWIN_DRM_POINTER_H
#define KWIN_DRM_POINTER_H

#include <QScopedPointer>

#include <xf86drmMode.h>

namespace KWin
{

template <typename T>
struct DrmDeleter;

template <>
struct DrmDeleter<drmModeAtomicReq>
{
    static void cleanup(drmModeAtomicReq *req)
    {
        drmModeAtomicFree(req);
    }
};

template <>
struct DrmDeleter<drmModeConnector>
{
    static void cleanup(drmModeConnector *connector)
    {
        drmModeFreeConnector(connector);
    }
};

template <>
struct DrmDeleter<drmModeCrtc>
{
    static void cleanup(drmModeCrtc *crtc)
    {
        drmModeFreeCrtc(crtc);
    }
};

template <>
struct DrmDeleter<drmModeFB>
{
    static void cleanup(drmModeFB *fb)
    {
        drmModeFreeFB(fb);
    }
};

template <>
struct DrmDeleter<drmModeEncoder>
{
    static void cleanup(drmModeEncoder *encoder)
    {
        drmModeFreeEncoder(encoder);
    }
};

template <>
struct DrmDeleter<drmModeModeInfo>
{
    static void cleanup(drmModeModeInfo *info)
    {
        drmModeFreeModeInfo(info);
    }
};

template <>
struct DrmDeleter<drmModeObjectProperties>
{
    static void cleanup(drmModeObjectProperties *properties)
    {
        drmModeFreeObjectProperties(properties);
    }
};

template <>
struct DrmDeleter<drmModePlane>
{
    static void cleanup(drmModePlane *plane)
    {
        drmModeFreePlane(plane);
    }
};

template <>
struct DrmDeleter<drmModePlaneRes>
{
    static void cleanup(drmModePlaneRes *resources)
    {
        drmModeFreePlaneResources(resources);
    }
};

template <>
struct DrmDeleter<drmModePropertyRes>
{
    static void cleanup(drmModePropertyRes *property)
    {
        drmModeFreeProperty(property);
    }
};

template <>
struct DrmDeleter<drmModePropertyBlobRes>
{
    static void cleanup(drmModePropertyBlobRes *blob)
    {
        drmModeFreePropertyBlob(blob);
    }
};

template <>
struct DrmDeleter<drmModeRes>
{
    static void cleanup(drmModeRes *resources)
    {
        drmModeFreeResources(resources);
    }
};

template <typename T>
using DrmScopedPointer = QScopedPointer<T, DrmDeleter<T>>;

}

#endif

