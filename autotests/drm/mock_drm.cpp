/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mock_drm.h"

#include <errno.h>
extern "C" {
#include <libxcvt/libxcvt.h>
}
#include <math.h>

#include <QMap>
#include <QDebug>

// Mock impls

static QMap<int, MockGpu*> s_gpus;

static MockGpu *getGpu(int fd)
{
    return s_gpus[fd];
}

MockGpu::MockGpu(int fd, int numCrtcs, int gammaSize)
    : fd(fd)
{
    s_gpus.insert(fd, this);
    for (int i = 0; i < numCrtcs; i++) {
        const auto &plane = std::make_shared<MockPlane>(this, PlaneType::Primary, i);
        crtcs << std::make_shared<MockCrtc>(this, plane, i, gammaSize);
        planes << plane;
    }
    deviceCaps.insert(MOCKDRM_DEVICE_CAP_ATOMIC, 1);
    deviceCaps.insert(DRM_CAP_DUMB_BUFFER, 1);
    deviceCaps.insert(DRM_CAP_PRIME, DRM_PRIME_CAP_IMPORT | DRM_PRIME_CAP_EXPORT);
    deviceCaps.insert(DRM_CAP_ASYNC_PAGE_FLIP, 0);
    deviceCaps.insert(DRM_CAP_ADDFB2_MODIFIERS, 1);
}

MockGpu::~MockGpu()
{
    s_gpus.remove(fd);
}

MockPropertyBlob *MockGpu::getBlob(uint32_t id) const
{
    auto it = std::find_if(propertyBlobs.begin(), propertyBlobs.end(), [id](const auto &propBlob) {
        return propBlob->id == id;
    });
    return it == propertyBlobs.end() ? nullptr : it->get();
}

MockConnector *MockGpu::findConnector(uint32_t id) const
{
    auto it = std::find_if(connectors.constBegin(), connectors.constEnd(), [id](const auto &c){return c->id == id;});
    return it == connectors.constEnd() ? nullptr : (*it).get();
}

MockCrtc *MockGpu::findCrtc(uint32_t id) const
{
    auto it = std::find_if(crtcs.constBegin(), crtcs.constEnd(), [id](const auto &c){return c->id == id;});
    return it == crtcs.constEnd() ? nullptr : (*it).get();
}

MockPlane *MockGpu::findPlane(uint32_t id) const
{
    auto it = std::find_if(planes.constBegin(), planes.constEnd(), [id](const auto &p){return p->id == id;});
    return it == planes.constEnd() ? nullptr : (*it).get();
}

void MockGpu::flipPage(uint32_t crtcId)
{
    auto crtc = findCrtc(crtcId);
    Q_ASSERT(crtc);
    for (const auto &plane : std::as_const(planes)) {
        if (plane->getProp(QStringLiteral("CRTC_ID")) == crtc->id) {
            plane->currentFb = plane->nextFb;
        }
    }
    // TODO page flip event?
}

//

MockObject::MockObject(MockGpu *gpu)
    : id(gpu->idCounter++)
    , gpu(gpu)
{
    gpu->objects << this;
}

MockObject::~MockObject()
{
    gpu->objects.removeOne(this);
}

uint64_t MockObject::getProp(const QString &propName) const
{
    for (const auto &prop : std::as_const(props)) {
        if (prop.name == propName) {
            return prop.value;
        }
    }
    Q_UNREACHABLE();
}

void MockObject::setProp(const QString &propName, uint64_t value)
{
    for (auto &prop : props) {
        if (prop.name == propName) {
            prop.value = value;
            return;
        }
    }
    Q_UNREACHABLE();
}

uint32_t MockObject::getPropId(const QString &propName) const
{
    for (const auto &prop : std::as_const(props)) {
        if (prop.name == propName) {
            return prop.id;
        }
    }
    Q_UNREACHABLE();
}

//

MockProperty::MockProperty(MockObject *obj, QString name, uint64_t initialValue, uint32_t flags, QVector<QByteArray> enums)
    : obj(obj)
    , id(obj->gpu->idCounter++)
    , flags(flags)
    , name(name)
    , value(initialValue)
    , enums(enums)
{
    qDebug("Added property %s with id %u to object %u", qPrintable(name), id, obj->id);
}

MockPropertyBlob::MockPropertyBlob(MockGpu *gpu, const void *d, size_t size)
    : gpu(gpu)
    , id(gpu->idCounter++)
    , data(malloc(size))
    , size(size)
{
    memcpy(data, d, size);
}

MockPropertyBlob::~MockPropertyBlob()
{
    free(data);
}

//

#define addProp(name, value, flags) props << MockProperty(this, QStringLiteral(name), value, flags)

MockConnector::MockConnector(MockGpu *gpu, bool nonDesktop)
    : MockObject(gpu)
    , connection(DRM_MODE_CONNECTED)
    , type(DRM_MODE_CONNECTOR_DisplayPort)
    , encoder(std::make_shared<MockEncoder>(gpu, 0xFF))
{
    gpu->encoders << encoder;
    addProp("CRTC_ID", 0, DRM_MODE_PROP_ATOMIC);

    addProp("Subpixel", DRM_MODE_SUBPIXEL_UNKNOWN, DRM_MODE_PROP_IMMUTABLE);
    addProp("non-desktop", nonDesktop, DRM_MODE_PROP_IMMUTABLE);
    addProp("vrr_capable", 0, DRM_MODE_PROP_IMMUTABLE);

    addProp("DPMS", DRM_MODE_DPMS_OFF, 0);
    addProp("EDID", 0, DRM_MODE_PROP_BLOB | DRM_MODE_PROP_IMMUTABLE);

    addMode(1920, 1080, 60.0);
}

void MockConnector::addMode(uint32_t width, uint32_t height, float refreshRate, bool preferred)
{
    auto modeInfo = libxcvt_gen_mode_info(width, height, refreshRate, false, false);

    drmModeModeInfo mode{
        .clock = uint32_t(modeInfo->dot_clock),
        .hdisplay = uint16_t(modeInfo->hdisplay),
        .hsync_start = modeInfo->hsync_start,
        .hsync_end = modeInfo->hsync_end,
        .htotal = modeInfo->htotal,
        .hskew = 0,
        .vdisplay = uint16_t(modeInfo->vdisplay),
        .vsync_start = modeInfo->vsync_start,
        .vsync_end = modeInfo->vsync_end,
        .vtotal = modeInfo->vtotal,
        .vscan = 1,
        .vrefresh = uint32_t(modeInfo->vrefresh),
        .flags = modeInfo->mode_flags,
        .type = DRM_MODE_TYPE_DRIVER,
    };
    if (preferred) {
        mode.type |= DRM_MODE_TYPE_PREFERRED;
    }
    sprintf(mode.name, "%dx%d@%d", width, height, mode.vrefresh);

    modes.push_back(mode);
    free(modeInfo);
}

//

MockCrtc::MockCrtc(MockGpu *gpu, const std::shared_ptr<MockPlane> &legacyPlane, int pipeIndex, int gamma_size)
    : MockObject(gpu)
    , pipeIndex(pipeIndex)
    , gamma_size(gamma_size)
    , legacyPlane(legacyPlane)
{
    addProp("MODE_ID", 0, DRM_MODE_PROP_ATOMIC | DRM_MODE_PROP_BLOB);
    addProp("ACTIVE", 0, DRM_MODE_PROP_ATOMIC);
    addProp("GAMMA_LUT", 0, DRM_MODE_PROP_ATOMIC | DRM_MODE_PROP_BLOB);
    addProp("GAMMA_LUT_SIZE", gamma_size, DRM_MODE_PROP_ATOMIC);
}


//

MockPlane::MockPlane(MockGpu *gpu, PlaneType type, int crtcIndex)
    : MockObject(gpu)
    , possibleCrtcs(1 << crtcIndex)
    , type(type)
{
    props << MockProperty(this, QStringLiteral("type"), static_cast<uint64_t>(type), DRM_MODE_PROP_IMMUTABLE | DRM_MODE_PROP_ENUM | DRM_MODE_PROP_BITMASK,
                            {QByteArrayLiteral("Primary"), QByteArrayLiteral("Overlay"), QByteArrayLiteral("Cursor")});
    addProp("FB_ID", 0, DRM_MODE_PROP_ATOMIC);
    addProp("CRTC_ID", 0, DRM_MODE_PROP_ATOMIC);
    addProp("CRTC_X", 0, DRM_MODE_PROP_ATOMIC);
    addProp("CRTC_Y", 0, DRM_MODE_PROP_ATOMIC);
    addProp("CRTC_W", 0, DRM_MODE_PROP_ATOMIC);
    addProp("CRTC_H", 0, DRM_MODE_PROP_ATOMIC);
    addProp("SRC_X", 0, DRM_MODE_PROP_ATOMIC);
    addProp("SRC_Y", 0, DRM_MODE_PROP_ATOMIC);
    addProp("SRC_W", 0, DRM_MODE_PROP_ATOMIC);
    addProp("SRC_H", 0, DRM_MODE_PROP_ATOMIC);
}

//

MockEncoder::MockEncoder(MockGpu* gpu, uint32_t possible_crtcs)
    : MockObject(gpu)
    , possible_crtcs(possible_crtcs)
{
}


//

MockFb::MockFb(MockGpu *gpu, uint32_t width, uint32_t height)
    : id(gpu->idCounter++)
    , width(width)
    , height(height)
    , gpu(gpu)
{
    gpu->fbs << this;
}

MockFb::~MockFb()
{
    gpu->fbs.removeOne(this);
}

//

MockDumbBuffer::MockDumbBuffer(MockGpu *gpu, uint32_t width, uint32_t height, uint32_t bpp)
    : handle(gpu->idCounter++)
    , pitch(width * ceil(bpp / 8.0))
    , data(height * pitch)
    , gpu(gpu)
{
}

// drm functions

#define GPU(fd, error) auto gpu = getGpu(fd);\
if (!gpu) {\
    qWarning("invalid fd %d", fd);\
    errno = EINVAL;\
    return error;\
}

drmVersionPtr drmGetVersion(int fd)
{
    GPU(fd, nullptr);
    drmVersionPtr ptr = new drmVersion;
    ptr->name = new char[gpu->name.size() + 1];
    strcpy(ptr->name, gpu->name.data());
    return ptr;
}

void drmFreeVersion(drmVersionPtr ptr)
{
    Q_ASSERT(ptr);
    delete[] ptr->name;
    delete ptr;
}

int drmSetClientCap(int fd, uint64_t capability, uint64_t value)
{
    GPU(fd, -EINVAL);
    if (capability == DRM_CLIENT_CAP_ATOMIC) {
        if (!gpu->deviceCaps[MOCKDRM_DEVICE_CAP_ATOMIC]) {
            return -(errno = ENOTSUP);
        }
        qDebug("Setting DRM_CLIENT_CAP_ATOMIC to %lu", value);
    }
    gpu->clientCaps.insert(capability, value);
    return 0;
}

int drmGetCap(int fd, uint64_t capability, uint64_t *value)
{
    GPU(fd, -EINVAL);
    if (gpu->deviceCaps.contains(capability)) {
        *value = gpu->deviceCaps[capability];
        return 0;
    }
    qDebug("Could not find capability %lu", capability);
    return -(errno = EINVAL);
}

int drmHandleEvent(int fd, drmEventContextPtr evctx)
{
    GPU(fd, -EINVAL);
    return -(errno = ENOTSUP);
}

int drmIoctl(int fd, unsigned long request, void *arg)
{
    GPU(fd, -EINVAL);
    if (request == DRM_IOCTL_MODE_CREATE_DUMB) {
        auto args = static_cast<drm_mode_create_dumb*>(arg);
        auto dumb = std::make_shared<MockDumbBuffer>(gpu, args->width, args->height, args->bpp);
        args->handle = dumb->handle;
        args->pitch = dumb->pitch;
        args->size = dumb->data.size();
        gpu->dumbBuffers << dumb;
        return 0;
    } else if (request == DRM_IOCTL_MODE_DESTROY_DUMB) {
        auto args = static_cast<drm_mode_destroy_dumb*>(arg);
        auto it = std::find_if(gpu->dumbBuffers.begin(), gpu->dumbBuffers.end(), [args](const auto &buf){return buf->handle == args->handle;});
        if (it == gpu->dumbBuffers.end()) {
            qWarning("buffer %u not found!", args->handle);
            return -(errno = EINVAL);
        } else {
            gpu->dumbBuffers.erase(it);
            return 0;
        }
    } else if (request == DRM_IOCTL_MODE_MAP_DUMB) {
        auto args = static_cast<drm_mode_map_dumb*>(arg);
        auto it = std::find_if(gpu->dumbBuffers.begin(), gpu->dumbBuffers.end(), [args](const auto &buf){return buf->handle == args->handle;});
        if (it == gpu->dumbBuffers.end()) {
            qWarning("buffer %u not found!", args->handle);
            return -(errno = EINVAL);
        } else {
            args->offset = reinterpret_cast<uintptr_t>((*it)->data.data());
            return 0;
        }
    }
    return -(errno = ENOTSUP);
}

drmModeResPtr drmModeGetResources(int fd)
{
    GPU(fd, nullptr)
    drmModeResPtr res = new drmModeRes;

    res->count_connectors = gpu->connectors.count();
    res->connectors = res->count_connectors ? new uint32_t[res->count_connectors] : nullptr;
    int i = 0;
    for (const auto &conn : std::as_const(gpu->connectors)) {
        res->connectors[i++] = conn->id;
    }

    res->count_encoders = gpu->encoders.count();
    res->encoders = res->count_encoders ? new uint32_t[res->count_encoders] : nullptr;
    i = 0;
    for (const auto &enc : std::as_const(gpu->encoders)) {
        res->encoders[i++] = enc->id;
    }

    res->count_crtcs = gpu->crtcs.count();
    res->crtcs = res->count_crtcs ? new uint32_t[res->count_crtcs] : nullptr;
    i = 0;
    for (const auto &crtc : std::as_const(gpu->crtcs)) {
        res->crtcs[i++] = crtc->id;
    }

    res->count_fbs = gpu->fbs.count();
    res->fbs = res->count_fbs ? new uint32_t[res->count_fbs] : nullptr;
    i = 0;
    for (const auto &fb : std::as_const(gpu->fbs)) {
        res->fbs[i++] = fb->id;
    }

    res->min_width = 0;
    res->min_height = 0;
    res->max_width = 2 << 14;
    res->max_height = 2 << 14;

    gpu->resPtrs << res;
    return res;
}

int drmModeAddFB(int fd, uint32_t width, uint32_t height, uint8_t depth,
                 uint8_t bpp, uint32_t pitch, uint32_t bo_handle,
                 uint32_t *buf_id)
{
    GPU(fd, EINVAL)
    auto fb = new MockFb(gpu, width, height);
    *buf_id = fb->id;
    return 0;
}

int drmModeAddFB2(int fd, uint32_t width, uint32_t height,
                  uint32_t pixel_format, const uint32_t bo_handles[4],
                  const uint32_t pitches[4], const uint32_t offsets[4],
                  uint32_t *buf_id, uint32_t flags)
{
    GPU(fd, EINVAL)
    auto fb = new MockFb(gpu, width, height);
    *buf_id = fb->id;
    return 0;
}

int drmModeAddFB2WithModifiers(int fd, uint32_t width, uint32_t height,
                               uint32_t pixel_format, const uint32_t bo_handles[4],
                               const uint32_t pitches[4], const uint32_t offsets[4],
                               const uint64_t modifier[4], uint32_t *buf_id,
                               uint32_t flags)
{
    GPU(fd, EINVAL)
    if (!gpu->deviceCaps.contains(DRM_CAP_ADDFB2_MODIFIERS)) {
        return -(errno = ENOTSUP);
    }
    auto fb = new MockFb(gpu, width, height);
    *buf_id = fb->id;
    return 0;
}

int drmModeRmFB(int fd, uint32_t bufferId)
{
    GPU(fd, EINVAL)
    auto it = std::find_if(gpu->fbs.begin(), gpu->fbs.end(), [bufferId](const auto &fb){return fb->id == bufferId;});
    if (it == gpu->fbs.end()) {
        qWarning("invalid bufferId %u passed to drmModeRmFB", bufferId);
        return EINVAL;
    } else {
        auto fb = *it;
        gpu->fbs.erase(it);
        for (const auto &plane : std::as_const(gpu->planes)) {
            if (plane->nextFb == fb) {
                plane->nextFb = nullptr;
            }
            if (plane->currentFb == fb) {
                qWarning("current fb %u of plane %u got removed. Deactivating plane", bufferId, plane->id);
                plane->setProp(QStringLiteral("CRTC_ID"), 0);
                plane->setProp(QStringLiteral("FB_ID"), 0);
                plane->currentFb = nullptr;

                auto crtc = gpu->findCrtc(plane->getProp(QStringLiteral("CRTC_ID")));
                Q_ASSERT(crtc);
                crtc->setProp(QStringLiteral("ACTIVE"), 0);
                qWarning("deactvating crtc %u", crtc->id);

                for (const auto &conn : std::as_const(gpu->connectors)) {
                    if (conn->getProp(QStringLiteral("CRTC_ID")) == crtc->id) {
                        conn->setProp(QStringLiteral("CRTC_ID"), 0);
                        qWarning("deactvating connector %u", conn->id);
                    }
                }
            }
        }
        delete fb;
        return 0;
    }
}

drmModeCrtcPtr drmModeGetCrtc(int fd, uint32_t crtcId)
{
    GPU(fd, nullptr);
    if (auto crtc = gpu->findCrtc(crtcId)) {
        drmModeCrtcPtr c = new drmModeCrtc;
        c->crtc_id = crtcId;
        c->buffer_id = crtc->currentFb ? crtc->currentFb->id : 0;
        c->gamma_size = crtc->gamma_size;
        c->mode_valid = crtc->modeValid;
        c->mode = crtc->mode;
        c->x = 0;
        c->y = 0;
        c->width = crtc->mode.hdisplay;
        c->height = crtc->mode.vdisplay;
        gpu->drmCrtcs << c;
        return c;
    } else {
        qWarning("invalid crtcId %u passed to drmModeGetCrtc", crtcId);
        errno = EINVAL;
        return nullptr;
    }
}

int drmModeSetCrtc(int fd, uint32_t crtcId, uint32_t bufferId,
                   uint32_t x, uint32_t y, uint32_t *connectors, int count,
                   drmModeModeInfoPtr mode)
{
    GPU(fd, -EINVAL);
    auto crtc = gpu->findCrtc(crtcId);
    if (!crtc) {
        qWarning("invalid crtcId %u passed to drmModeSetCrtc", crtcId);
        return -(errno = EINVAL);
    }
    auto oldModeBlob = crtc->getProp(QStringLiteral("MODE_ID"));
    uint32_t modeBlob = 0;
    if (mode) {
        drmModeCreatePropertyBlob(fd, mode, sizeof(drmModeModeInfo), &modeBlob);
    }

    auto req = drmModeAtomicAlloc();
    req->legacyEmulation = true;
    drmModeAtomicAddProperty(req, crtcId, crtc->getPropId(QStringLiteral("MODE_ID")), modeBlob);
    drmModeAtomicAddProperty(req, crtcId, crtc->getPropId(QStringLiteral("ACTIVE")), modeBlob && count);
    QVector<uint32_t> conns;
    for (int i = 0; i < count; i++) {
        conns << connectors[i];
    }
    for (const auto &conn : std::as_const(gpu->connectors)) {
        if (conns.contains(conn->id)) {
            drmModeAtomicAddProperty(req, conn->id, conn->getPropId(QStringLiteral("CRTC_ID")), modeBlob ? crtc->id : 0);
            conns.removeOne(conn->id);
        } else if (conn->getProp(QStringLiteral("CRTC_ID")) == crtc->id) {
            drmModeAtomicAddProperty(req, conn->id, conn->getPropId(QStringLiteral("CRTC_ID")), 0);
        }
    }
    if (!conns.isEmpty()) {
        for (const auto &c : std::as_const(conns)) {
            qWarning("invalid connector %u passed to drmModeSetCrtc", c);
        }
        drmModeAtomicFree(req);
        return -(errno = EINVAL);
    }
    drmModeAtomicAddProperty(req, crtc->legacyPlane->id, crtc->legacyPlane->getPropId(QStringLiteral("CRTC_ID")), modeBlob && count ? crtc->id : 0);
    drmModeAtomicAddProperty(req, crtc->legacyPlane->id, crtc->legacyPlane->getPropId(QStringLiteral("CRTC_X")), x);
    drmModeAtomicAddProperty(req, crtc->legacyPlane->id, crtc->legacyPlane->getPropId(QStringLiteral("CRTC_Y")), y);
    drmModeAtomicAddProperty(req, crtc->legacyPlane->id, crtc->legacyPlane->getPropId(QStringLiteral("CRTC_W")), mode->hdisplay - x);
    drmModeAtomicAddProperty(req, crtc->legacyPlane->id, crtc->legacyPlane->getPropId(QStringLiteral("CRTC_H")), mode->vdisplay - y);
    drmModeAtomicAddProperty(req, crtc->legacyPlane->id, crtc->legacyPlane->getPropId(QStringLiteral("FB_ID")), bufferId);
    int result = drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, nullptr);
    drmModeAtomicFree(req);
    if (result == 0) {
        drmModeDestroyPropertyBlob(fd, oldModeBlob);
    }
    return result;
}

int drmModeSetCursor(int fd, uint32_t crtcId, uint32_t bo_handle, uint32_t width, uint32_t height)
{
    GPU(fd, -EINVAL);
    if (auto crtc = gpu->findCrtc(crtcId)) {
        if (bo_handle != 0) {
            auto it = std::find_if(gpu->dumbBuffers.constBegin(), gpu->dumbBuffers.constEnd(), [bo_handle](const auto &bo){return bo->handle == bo_handle;});
            if (it == gpu->dumbBuffers.constEnd()) {
                qWarning("invalid bo_handle %u passed to drmModeSetCursor", bo_handle);
                return -(errno = EINVAL);
            }
            crtc->cursorBo = (*it).get();
        } else {
            crtc->cursorBo = nullptr;
        }
        crtc->cursorRect.setSize(QSize(width, height));
        return 0;
    } else {
        qWarning("invalid crtcId %u passed to drmModeSetCursor", crtcId);
        return -(errno = EINVAL);
    }
}

int drmModeSetCursor2(int fd, uint32_t crtcId, uint32_t bo_handle, uint32_t width, uint32_t height, int32_t hot_x, int32_t hot_y)
{
    GPU(fd, -EINVAL);
    return -(errno = ENOTSUP);
}

int drmModeMoveCursor(int fd, uint32_t crtcId, int x, int y)
{
    GPU(fd, -EINVAL);
    if (auto crtc = gpu->findCrtc(crtcId)) {
        crtc->cursorRect.moveTo(x, y);
        return 0;
    } else {
        qWarning("invalid crtcId %u passed to drmModeMoveCursor", crtcId);
        return -(errno = EINVAL);
    }
}

drmModeEncoderPtr drmModeGetEncoder(int fd, uint32_t encoder_id)
{
    GPU(fd, nullptr);
    auto it = std::find_if(gpu->encoders.constBegin(), gpu->encoders.constEnd(), [encoder_id](const auto &e){return e->id == encoder_id;});
    if (it == gpu->encoders.constEnd()) {
        qWarning("invalid encoder_id %u passed to drmModeGetEncoder", encoder_id);
        errno = EINVAL;
        return nullptr;
    } else {
        auto encoder = *it;
        drmModeEncoderPtr enc = new drmModeEncoder;
        enc->encoder_id = encoder_id;
        enc->crtc_id = encoder->crtc ? encoder->crtc->id : 0;
        enc->encoder_type = 0;
        enc->possible_crtcs = encoder->possible_crtcs;
        enc->possible_clones = encoder->possible_clones;

        gpu->drmEncoders << enc;
        return enc;
    }
}

drmModeConnectorPtr drmModeGetConnector(int fd, uint32_t connectorId)
{
    GPU(fd, nullptr);
    if (auto conn = gpu->findConnector(connectorId)) {
        drmModeConnectorPtr c = new drmModeConnector{};
        c->connector_id = conn->id;
        c->connection = conn->connection;
        c->connector_type = conn->type;
        c->encoder_id = conn->encoder ? conn->encoder->id : 0;
        c->count_encoders = conn->encoder ? 1 : 0;
        c->encoders = c->count_encoders ? new uint32_t[1] : nullptr;
        if (c->encoders) {
            c->encoders[0] = conn->encoder->id;
        }
        c->count_modes = conn->modes.count();
        c->modes = c->count_modes ? new drmModeModeInfo[c->count_modes] : nullptr;
        for (int i = 0; i < c->count_modes; i++) {
            c->modes[i] = conn->modes[i];
        }
        c->mmHeight = 900;
        c->mmWidth = 1600;
        c->subpixel = DRM_MODE_SUBPIXEL_HORIZONTAL_RGB;

        c->connector_type_id = DRM_MODE_CONNECTOR_DisplayPort;// ?

        // these are not used nor will they be
        c->count_props = -1;
        c->props = nullptr;
        c->prop_values = nullptr;

        gpu->drmConnectors << c;
        return c;
    } else {
        qWarning("invalid connectorId %u passed to drmModeGetConnector", connectorId);
        errno = EINVAL;
        return nullptr;
    }
}

drmModeConnectorPtr drmModeGetConnectorCurrent(int fd, uint32_t connector_id)
{
    return drmModeGetConnector(fd, connector_id);
}

int drmModeCrtcSetGamma(int fd, uint32_t crtc_id, uint32_t size, uint16_t *red, uint16_t *green, uint16_t *blue)
{
    return -(errno = ENOTSUP);
}

int drmModePageFlip(int fd, uint32_t crtc_id, uint32_t fb_id, uint32_t flags, void *user_data)
{
    GPU(fd, -EINVAL);
    auto crtc = gpu->findCrtc(crtc_id);
    if (!crtc) {
        qWarning("invalid crtc_id %u passed to drmModePageFlip", crtc_id);
        return -(errno = EINVAL);
    }
    auto req = drmModeAtomicAlloc();
    req->legacyEmulation = true;
    drmModeAtomicAddProperty(req, crtc->legacyPlane->id, crtc->legacyPlane->getPropId(QStringLiteral("FB_ID")), fb_id);
    int result = drmModeAtomicCommit(fd, req, flags, user_data);
    drmModeAtomicFree(req);
    return result;
}


drmModePlaneResPtr drmModeGetPlaneResources(int fd)
{
    GPU(fd, nullptr);
    drmModePlaneResPtr res = new drmModePlaneRes;
    res->count_planes = gpu->planes.count();
    res->planes = res->count_planes ? new uint32_t[res->count_planes] : nullptr;
    for (uint i = 0; i < res->count_planes; i++) {
        res->planes[i] = gpu->planes[i]->id;
    }
    gpu->drmPlaneRes << res;
    return res;
}

drmModePlanePtr drmModeGetPlane(int fd, uint32_t plane_id)
{
    GPU(fd, nullptr);
    if (auto plane = gpu->findPlane(plane_id)) {
        drmModePlanePtr p = new drmModePlane;
        p->plane_id = plane_id;
        p->crtc_id = plane->getProp(QStringLiteral("CRTC_ID"));
        p->crtc_x = plane->getProp(QStringLiteral("CRTC_X"));
        p->crtc_y = plane->getProp(QStringLiteral("CRTC_Y"));
        p->fb_id = plane->getProp(QStringLiteral("FB_ID"));
        p->x = plane->getProp(QStringLiteral("SRC_X"));
        p->y = plane->getProp(QStringLiteral("SRC_Y"));
        p->possible_crtcs = plane->possibleCrtcs;

        // unused atm:
        p->count_formats = 0;
        p->formats = nullptr;
        p->gamma_size = 0;

        gpu->drmPlanes << p;
        return p;
    } else {
        qWarning("invalid plane_id %u passed to drmModeGetPlane", plane_id);
        errno = EINVAL;
        return nullptr;
    }
}

drmModePropertyPtr drmModeGetProperty(int fd, uint32_t propertyId)
{
    GPU(fd, nullptr);
    for (const auto &obj : std::as_const(gpu->objects)) {
        for (auto &prop : std::as_const(obj->props)) {
            if (prop.id == propertyId) {
                drmModePropertyPtr p = new drmModePropertyRes;
                p->prop_id = prop.id;
                p->flags = prop.flags;
                auto arr = prop.name.toLocal8Bit();
                strcpy(p->name, arr.constData());

                p->count_blobs = prop.flags & DRM_MODE_PROP_BLOB ? 1 : 0;
                if (p->count_blobs) {
                    p->blob_ids = new uint32_t[1];
                    p->blob_ids[0] = prop.value;
                } else {
                    p->blob_ids = nullptr;
                }

                p->count_enums = prop.enums.count();
                p->enums = new drm_mode_property_enum[p->count_enums];
                for (int i = 0; i < p->count_enums; i++) {
                    strcpy(p->enums[i].name, prop.enums[i].constData());
                    p->enums[i].value = i;
                }

                p->count_values = 1;
                p->values = new uint64_t[1];
                p->values[0] = prop.value;

                gpu->drmProps << p;
                return p;
            }
        }
    }
    qWarning("invalid propertyId %u passed to drmModeGetProperty", propertyId);
    errno = EINVAL;
    return nullptr;
}

void drmModeFreeProperty(drmModePropertyPtr ptr)
{
    if (!ptr) {
        return;
    }
    for (const auto &gpu : std::as_const(s_gpus)) {
        if (gpu->drmProps.removeOne(ptr)) {
            delete[] ptr->values;
            delete[] ptr->blob_ids;
            delete[] ptr->enums;
            delete ptr;
            return;
        }
    }
}



drmModePropertyBlobPtr drmModeGetPropertyBlob(int fd, uint32_t blob_id)
{
    GPU(fd, nullptr);
    if (blob_id == 0) {
        return nullptr;
    }
    auto it = std::find_if(gpu->propertyBlobs.begin(), gpu->propertyBlobs.end(), [blob_id](const auto &blob) {
        return blob->id == blob_id;
    });
    if (it == gpu->propertyBlobs.end()) {
        qWarning("invalid blob_id %u passed to drmModeGetPropertyBlob", blob_id);
        errno = EINVAL;
        return nullptr;
    } else {
        auto blob = new drmModePropertyBlobRes;
        blob->id = (*it)->id;
        blob->length = (*it)->size;
        blob->data = malloc(blob->length);
        memcpy(blob->data, (*it)->data, blob->length);
        return blob;
    }
}

void drmModeFreePropertyBlob(drmModePropertyBlobPtr ptr)
{
    if (!ptr) {
        return;
    }
    for (const auto &gpu : std::as_const(s_gpus)) {
        if (gpu->drmPropertyBlobs.removeOne(ptr)) {
            free(ptr->data);
            delete ptr;
            return;
        }
    }
}

int drmModeConnectorSetProperty(int fd, uint32_t connector_id, uint32_t property_id, uint64_t value)
{
    return drmModeObjectSetProperty(fd, connector_id, DRM_MODE_OBJECT_CONNECTOR, property_id, value);
}

static uint32_t getType(MockObject *obj)
{
    if (dynamic_cast<MockConnector*>(obj)) {
        return DRM_MODE_OBJECT_CONNECTOR;
    } else if (dynamic_cast<MockCrtc*>(obj)) {
        return DRM_MODE_OBJECT_CRTC;
    } else if (dynamic_cast<MockPlane*>(obj)) {
        return DRM_MODE_OBJECT_PLANE;
    } else {
        return DRM_MODE_OBJECT_ANY;
    }
}

drmModeObjectPropertiesPtr drmModeObjectGetProperties(int fd, uint32_t object_id, uint32_t object_type)
{
    GPU(fd, nullptr);
    auto it = std::find_if(gpu->objects.constBegin(), gpu->objects.constEnd(), [object_id](const auto &obj){return obj->id == object_id;});
    if (it == gpu->objects.constEnd()) {
        qWarning("invalid object_id %u passed to drmModeObjectGetProperties", object_id);
        errno = EINVAL;
        return nullptr;
    } else {
        auto obj = *it;
        if (auto type = getType(obj); type != object_type) {
            qWarning("wrong object_type %u passed to drmModeObjectGetProperties for object %u with type %u", object_type, object_id, type);
            errno = EINVAL;
            return nullptr;
        }
        QVector<MockProperty> props;
        bool deviceAtomic = gpu->clientCaps.contains(DRM_CLIENT_CAP_ATOMIC) && gpu->clientCaps[DRM_CLIENT_CAP_ATOMIC];
        for (const auto &prop : std::as_const(obj->props)) {
            if (deviceAtomic || !(prop.flags & DRM_MODE_PROP_ATOMIC)) {
                props << prop;
            }
        }
        drmModeObjectPropertiesPtr p = new drmModeObjectProperties;
        p->count_props = props.count();
        p->props = new uint32_t[p->count_props];
        p->prop_values = new uint64_t[p->count_props];
        int i = 0;
        for (const auto &prop : std::as_const(props)) {
            p->props[i] = prop.id;
            p->prop_values[i] = prop.value;
            i++;
        }
        gpu->drmObjectProperties << p;
        return p;
    }
}

void drmModeFreeObjectProperties(drmModeObjectPropertiesPtr ptr)
{
    for (const auto &gpu : std::as_const(s_gpus)) {
        if (gpu->drmObjectProperties.removeOne(ptr)) {
            delete[] ptr->props;
            delete[] ptr->prop_values;
            delete ptr;
            return;
        }
    }
}

int drmModeObjectSetProperty(int fd, uint32_t object_id, uint32_t object_type, uint32_t property_id, uint64_t value)
{
    GPU(fd, -EINVAL);
    auto it = std::find_if(gpu->objects.constBegin(), gpu->objects.constEnd(), [object_id](const auto &obj){return obj->id == object_id;});
    if (it == gpu->objects.constEnd()) {
        qWarning("invalid object_id %u passed to drmModeObjectSetProperty", object_id);
        return -(errno = EINVAL);
    } else {
        auto obj = *it;
        if (auto type = getType(obj); type != object_type) {
            qWarning("wrong object_type %u passed to drmModeObjectSetProperty for object %u with type %u", object_type, object_id, type);
            return -(errno = EINVAL);
        }
        auto req = drmModeAtomicAlloc();
        req->legacyEmulation = true;
        drmModeAtomicAddProperty(req, object_id, property_id, value);
        int result = drmModeAtomicCommit(fd, req, 0, nullptr);
        drmModeAtomicFree(req);
        return result;
    }
}

static QVector<drmModeAtomicReqPtr> s_atomicReqs;

drmModeAtomicReqPtr drmModeAtomicAlloc(void)
{
    auto req = new drmModeAtomicReq;
    s_atomicReqs << req;
    return req;
}

void drmModeAtomicFree(drmModeAtomicReqPtr req)
{
    s_atomicReqs.removeOne(req);
    delete req;
}

int drmModeAtomicAddProperty(drmModeAtomicReqPtr req, uint32_t object_id, uint32_t property_id, uint64_t value)
{
    if (!req) {
        return -(errno = EINVAL);
    }
    Prop p;
    p.obj = object_id;
    p.prop = property_id;
    p.value = value;
    req->props << p;
    return req->props.count();
}

static bool checkIfEqual(const drmModeModeInfo &one, const drmModeModeInfo &two)
{
    return one.clock       == two.clock
        && one.hdisplay    == two.hdisplay
        && one.hsync_start == two.hsync_start
        && one.hsync_end   == two.hsync_end
        && one.htotal      == two.htotal
        && one.hskew       == two.hskew
        && one.vdisplay    == two.vdisplay
        && one.vsync_start == two.vsync_start
        && one.vsync_end   == two.vsync_end
        && one.vtotal      == two.vtotal
        && one.vscan       == two.vscan
        && one.vrefresh    == two.vrefresh;
}

int drmModeAtomicCommit(int fd, drmModeAtomicReqPtr req, uint32_t flags, void *user_data)
{
    GPU(fd, -EINVAL);
    if (!req->legacyEmulation && (!gpu->clientCaps.contains(DRM_CLIENT_CAP_ATOMIC) || !gpu->clientCaps[DRM_CLIENT_CAP_ATOMIC])) {
        qWarning("drmModeAtomicCommit requires the atomic capability");
        return -(errno = EINVAL);
    }

    // verify flags
    if ((flags & DRM_MODE_ATOMIC_NONBLOCK) && (flags & DRM_MODE_ATOMIC_ALLOW_MODESET)) {
        qWarning() << "NONBLOCK and MODESET are not allowed together";
        return -(errno = EINVAL);
    } else if ((flags & DRM_MODE_ATOMIC_TEST_ONLY) && (flags & DRM_MODE_PAGE_FLIP_EVENT)) {
        qWarning() << "TEST_ONLY and PAGE_FLIP_EVENT are not allowed together";
        return -(errno = EINVAL);
    } else if (flags & DRM_MODE_PAGE_FLIP_ASYNC) {
        qWarning() << "PAGE_FLIP_ASYNC is currently not supported with AMS";
        return -(errno = EINVAL);
    }

    QVector<MockConnector> connCopies;
    for (const auto &conn : std::as_const(gpu->connectors)) {
        connCopies << *conn;
    }
    QVector<MockCrtc> crtcCopies;
    for (const auto &crtc : std::as_const(gpu->crtcs)) {
        crtcCopies << *crtc;
    }
    QVector<MockPlane> planeCopies;
    for (const auto &plane : std::as_const(gpu->planes)) {
        planeCopies << *plane;
    }

    QVector<MockObject*> objects;
    for (int i = 0; i < connCopies.count(); i++) {
        objects << &connCopies[i];
    }
    for (int i = 0; i < crtcCopies.count(); i++) {
        objects << &crtcCopies[i];
    }
    for (int i = 0; i < planeCopies.count(); i++) {
        objects << &planeCopies[i];
    }

    // apply changes to the copies
    for (int i = 0; i < req->props.count(); i++) {
        auto p = req->props[i];
        auto it = std::find_if(objects.constBegin(), objects.constEnd(), [p](const auto &obj){return obj->id == p.obj;});
        if (it == objects.constEnd()) {
            qWarning("Object %u in atomic request not found!", p.obj);
            return -(errno = EINVAL);
        }
        auto &obj = *it;
        if (obj->id == p.obj) {
            auto prop = std::find_if(obj->props.begin(), obj->props.end(), [p](const auto &prop){return prop.id == p.prop;});
            if (prop == obj->props.end()) {
                qWarning("Property %u in atomic request for object %u not found!", p.prop, p.obj);
                return -(errno = EINVAL);
            }
            if (prop->value != p.value) {
                if (!(flags & DRM_MODE_ATOMIC_ALLOW_MODESET) && (prop->name == QStringLiteral("CRTC_ID") || prop->name == QStringLiteral("ACTIVE"))) {
                    qWarning("Atomic request without DRM_MODE_ATOMIC_ALLOW_MODESET tries to do a modeset with object %u", obj->id);
                    return -(errno = EINVAL);
                }
                if (prop->flags & DRM_MODE_PROP_BLOB) {
                    auto blobExists = gpu->getBlob(p.value) != nullptr;
                    if (blobExists != (p.value > 0)) {
                        qWarning("Atomic request tries to set property %s on obj %u to invalid blob id %lu", qPrintable(prop->name), obj->id, p.value);
                        return -(errno = EINVAL);
                    }
                }
                prop->value = p.value;
            }
        }
    }

    // check if the desired changes are allowed
    struct Pipeline {
        MockCrtc *crtc;
        QVector<MockConnector*> conns;
        MockPlane *primaryPlane = nullptr;
    };
    QVector<Pipeline> pipelines;
    for (int i = 0; i < crtcCopies.count(); i++) {
        if (crtcCopies[i].getProp(QStringLiteral("ACTIVE"))) {
            auto blob = gpu->getBlob(crtcCopies[i].getProp(QStringLiteral("MODE_ID")));
            if (!blob) {
                qWarning("Atomic request tries to enable CRTC %u without a mode", crtcCopies[i].id);
                return -(errno = EINVAL);
            } else if (blob->size != sizeof(drmModeModeInfo)) {
                qWarning("Atomic request tries to enable CRTC %u with an invalid mode blob", crtcCopies[i].id);
                return -(errno = EINVAL);
            }
            Pipeline pipeline;
            pipeline.crtc = &crtcCopies[i];
            pipelines << pipeline;
        }
    }
    for (int i = 0; i < connCopies.count(); i++) {
        if (auto crtc = connCopies[i].getProp(QStringLiteral("CRTC_ID"))) {
            bool found = false;
            for (int p = 0; p < pipelines.count(); p++) {
                if (pipelines[p].crtc->id == crtc) {
                    pipelines[p].conns << &connCopies[i];
                    found = true;
                    break;
                }
            }
            if (!found) {
                qWarning("CRTC_ID of connector %u points to inactive or wrong crtc", connCopies[i].id);
                return -(errno = EINVAL);
            }
        }
    }
    for (int i = 0; i < planeCopies.count(); i++) {
        if (auto crtc = planeCopies[i].getProp(QStringLiteral("CRTC_ID"))) {
            bool found = false;
            for (int p = 0; p < pipelines.count(); p++) {
                if (pipelines[p].crtc->id == crtc) {
                    if (pipelines[p].primaryPlane) {
                        qWarning("crtc %u has more than one primary planes assigned: %u and %u", pipelines[p].crtc->id, pipelines[p].primaryPlane->id, planeCopies[i].id);
                        return -(errno = EINVAL);
                    } else if (!(planeCopies[i].possibleCrtcs & (1 << pipelines[p].crtc->pipeIndex))) {
                        qWarning("crtc %u is not suitable for primary plane %u", pipelines[p].crtc->id, planeCopies[i].id);
                        return -(errno = EINVAL);
                    } else {
                        pipelines[p].primaryPlane = &planeCopies[i];
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                qWarning("CRTC_ID of plane %u points to inactive or wrong crtc", planeCopies[i].id);
                return -(errno = EINVAL);
            }
            auto fbId = planeCopies[i].getProp(QStringLiteral("FB_ID"));
            if (!fbId) {
                qWarning("FB_ID of active plane %u is 0", planeCopies[i].id);
                return -(errno = EINVAL);
            }
            auto it = std::find_if(gpu->fbs.constBegin(), gpu->fbs.constEnd(), [fbId](auto fb){return fb->id == fbId;});
            if (it == gpu->fbs.constEnd()) {
                qWarning("FB_ID %lu of active plane %u is invalid", fbId, planeCopies[i].id);
                return -(errno = EINVAL);
            }
            planeCopies[i].nextFb = *it;
        } else {
            planeCopies[i].nextFb = nullptr;
        }
    }
    for (const auto &p : std::as_const(pipelines)) {
        if (p.conns.isEmpty()) {
            qWarning("Active crtc %u has no assigned connectors", p.crtc->id);
            return -(errno = EINVAL);
        } else if (!p.primaryPlane) {
            qWarning("Active crtc %u has no assigned primary plane", p.crtc->id);
            return -(errno = EINVAL);
        } else {
            drmModeModeInfo mode = *static_cast<drmModeModeInfo*>(gpu->getBlob(p.crtc->getProp(QStringLiteral("MODE_ID")))->data);
            for (const auto &conn : p.conns) {
                bool modeFound = std::find_if(conn->modes.constBegin(), conn->modes.constEnd(), [mode](const auto &m){
                    return checkIfEqual(mode, m);
                }) != conn->modes.constEnd();
                if (!modeFound) {
                    qWarning("mode on crtc %u is incompatible with connector %u", p.crtc->id, conn->id);
                    return -(errno = EINVAL);
                }
            }
        }
    }

    // if wanted, apply them

    if (!(flags & DRM_MODE_ATOMIC_TEST_ONLY)) {
        for (auto &conn : std::as_const(gpu->connectors)) {
            auto it = std::find_if(connCopies.constBegin(), connCopies.constEnd(), [conn](auto c){return c.id == conn->id;});
            if (it == connCopies.constEnd()) {
                qCritical("implementation error: can't find connector %u", conn->id);
                return -(errno = EINVAL);
            }
            *conn = *it;
        }
        for (auto &crtc : std::as_const(gpu->crtcs)) {
            auto it = std::find_if(crtcCopies.constBegin(), crtcCopies.constEnd(), [crtc](auto c){return c.id == crtc->id;});
            if (it == crtcCopies.constEnd()) {
                qCritical("implementation error: can't find crtc %u", crtc->id);
                return -(errno = EINVAL);
            }
            *crtc = *it;
        }
        for (auto &plane : std::as_const(gpu->planes)) {
            auto it = std::find_if(planeCopies.constBegin(), planeCopies.constEnd(), [plane](auto c){return c.id == plane->id;});
            if (it == planeCopies.constEnd()) {
                qCritical("implementation error: can't find plane %u", plane->id);
                return -(errno = EINVAL);
            }
            *plane = *it;
        }

        if (flags & DRM_MODE_PAGE_FLIP_EVENT) {
            // Unsupported
        }
    }

    return 0;
}


int drmModeCreatePropertyBlob(int fd, const void *data, size_t size, uint32_t *id)
{
    GPU(fd, -EINVAL);
    if (!data || !size || !id) {
        return -(errno = EINVAL);
    }
    auto blob = std::make_unique<MockPropertyBlob>(gpu, data, size);
    *id = blob->id;
    gpu->propertyBlobs.push_back(std::move(blob));
    return 0;
}

int drmModeDestroyPropertyBlob(int fd, uint32_t id)
{
    GPU(fd, -EINVAL);
    auto it = std::remove_if(gpu->propertyBlobs.begin(), gpu->propertyBlobs.end(), [id](const auto &blob) {
        return blob->id == id;
    });
    if (it == gpu->propertyBlobs.end()) {
        return -(errno = EINVAL);
    } else {
        gpu->propertyBlobs.erase(it, gpu->propertyBlobs.end());
        return 0;
    }
}

int drmModeCreateLease(int fd, const uint32_t *objects, int num_objects, int flags, uint32_t *lessee_id)
{
    return -(errno = ENOTSUP);
}

drmModeLesseeListPtr drmModeListLessees(int fd)
{
    return nullptr;
}

drmModeObjectListPtr drmModeGetLease(int fd)
{
    return nullptr;
}

int drmModeRevokeLease(int fd, uint32_t lessee_id)
{
    return -(errno = ENOTSUP);
}

void drmModeFreeResources(drmModeResPtr ptr)
{
    for (const auto &gpu : std::as_const(s_gpus)) {
        if (gpu->resPtrs.removeOne(ptr)) {
            delete[] ptr->connectors;
            delete[] ptr->crtcs;
            delete[] ptr->encoders;
            delete[] ptr->fbs;
            delete ptr;
        }
    }
}

void drmModeFreePlaneResources(drmModePlaneResPtr ptr)
{
    for (const auto &gpu : std::as_const(s_gpus)) {
        if (gpu->drmPlaneRes.removeOne(ptr)) {
            delete[] ptr->planes;
            delete ptr;
        }
    }
}

void drmModeFreeCrtc(drmModeCrtcPtr ptr)
{
    for (const auto &gpu : std::as_const(s_gpus)) {
        if (gpu->drmCrtcs.removeOne(ptr)) {
            delete ptr;
            return;
        }
    }
    Q_UNREACHABLE();
}

void drmModeFreeConnector(drmModeConnectorPtr ptr)
{
    for (const auto &gpu : std::as_const(s_gpus)) {
        if (gpu->drmConnectors.removeOne(ptr)) {
            delete[] ptr->encoders;
            delete[] ptr->props;
            delete[] ptr->prop_values;
            delete ptr;
            return;
        }
    }
    Q_UNREACHABLE();
}

void drmModeFreeEncoder(drmModeEncoderPtr ptr)
{
    for (const auto &gpu : std::as_const(s_gpus)) {
        if (gpu->drmEncoders.removeOne(ptr)) {
            delete ptr;
            return;
        }
    }
    Q_UNREACHABLE();
}

void drmModeFreePlane(drmModePlanePtr ptr)
{
    for (const auto &gpu : std::as_const(s_gpus)) {
        if (gpu->drmPlanes.removeOne(ptr)) {
            delete ptr;
            return;
        }
    }
    Q_UNREACHABLE();
}
