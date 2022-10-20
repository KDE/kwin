/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <memory>
#include <xf86drm.h>
#include <xf86drmMode.h>

namespace KWin
{

template<typename T>
struct DrmDeleter;

template<>
struct DrmDeleter<drmVersion>
{
    void operator()(drmVersion *version)
    {
        drmFreeVersion(version);
    }
};

template<>
struct DrmDeleter<drmModeAtomicReq>
{
    void operator()(drmModeAtomicReq *req)
    {
        drmModeAtomicFree(req);
    }
};

template<>
struct DrmDeleter<drmModeConnector>
{
    void operator()(drmModeConnector *connector)
    {
        drmModeFreeConnector(connector);
    }
};

template<>
struct DrmDeleter<drmModeCrtc>
{
    void operator()(drmModeCrtc *crtc)
    {
        drmModeFreeCrtc(crtc);
    }
};

template<>
struct DrmDeleter<drmModeFB>
{
    void operator()(drmModeFB *fb)
    {
        drmModeFreeFB(fb);
    }
};

template<>
struct DrmDeleter<drmModeEncoder>
{
    void operator()(drmModeEncoder *encoder)
    {
        drmModeFreeEncoder(encoder);
    }
};

template<>
struct DrmDeleter<drmModeModeInfo>
{
    void operator()(drmModeModeInfo *info)
    {
        drmModeFreeModeInfo(info);
    }
};

template<>
struct DrmDeleter<drmModeObjectProperties>
{
    void operator()(drmModeObjectProperties *properties)
    {
        drmModeFreeObjectProperties(properties);
    }
};

template<>
struct DrmDeleter<drmModePlane>
{
    void operator()(drmModePlane *plane)
    {
        drmModeFreePlane(plane);
    }
};

template<>
struct DrmDeleter<drmModePlaneRes>
{
    void operator()(drmModePlaneRes *resources)
    {
        drmModeFreePlaneResources(resources);
    }
};

template<>
struct DrmDeleter<drmModePropertyRes>
{
    void operator()(drmModePropertyRes *property)
    {
        drmModeFreeProperty(property);
    }
};

template<>
struct DrmDeleter<drmModePropertyBlobRes>
{
    void operator()(drmModePropertyBlobRes *blob)
    {
        drmModeFreePropertyBlob(blob);
    }
};

template<>
struct DrmDeleter<drmModeRes>
{
    void operator()(drmModeRes *resources)
    {
        drmModeFreeResources(resources);
    }
};

template<>
struct DrmDeleter<drmModeLesseeListRes>
{
    void operator()(drmModeLesseeListRes *ptr)
    {
        drmFree(ptr);
    }
};

template<typename T>
using DrmUniquePtr = std::unique_ptr<T, DrmDeleter<T>>;
}
