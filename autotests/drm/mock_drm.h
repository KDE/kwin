/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <QList>
#include <QMap>
#include <QRect>
#include <memory>
#include <mutex>
#include <vector>

class MockGpu;
class MockFb;
class MockCrtc;
class MockEncoder;
class MockObject;
class MockPlane;

class MockProperty {
public:
    MockProperty(MockObject *obj, QString name, uint64_t initialValue, uint32_t flags, QList<QByteArray> enums = {});
    ~MockProperty() = default;

    MockObject *obj;
    uint32_t id;
    uint32_t flags;
    QString name;
    uint64_t value;
    QList<QByteArray> enums;
};

class MockPropertyBlob {
public:
    MockPropertyBlob(MockGpu *gpu, const void *data, size_t size);
    ~MockPropertyBlob();

    MockGpu *gpu;
    uint32_t id;
    void *data;
    size_t size;
};

class MockObject {
public:
    MockObject(MockGpu *gpu);
    virtual ~MockObject();

    uint64_t getProp(const QString &propName) const;
    void setProp(const QString &propName, uint64_t value);

    uint32_t getPropId(const QString &propName) const;

    uint32_t id;
    QList<MockProperty> props;
    MockGpu *gpu;
};

class MockConnector : public MockObject {
public:
    MockConnector(MockGpu *gpu, bool nonDesktop = false);
    MockConnector(const MockConnector &obj) = default;
    ~MockConnector() = default;

    void addMode(uint32_t width, uint32_t height, float refreshRate, bool preferred = false);

    drmModeConnection connection;
    uint32_t type;
    std::shared_ptr<MockEncoder> encoder;
    QList<drmModeModeInfo> modes;
};

class MockEncoder : public MockObject {
public:
    MockEncoder(MockGpu *gpu, uint32_t possible_crtcs);
    MockEncoder(const MockEncoder &obj) = default;
    ~MockEncoder() = default;

    MockCrtc *crtc = nullptr;
    uint32_t possible_crtcs;
    uint32_t possible_clones = 0;
};

class MockCrtc : public MockObject {
public:
    MockCrtc(MockGpu *gpu, const std::shared_ptr<MockPlane> &legacyPlane, int pipeIndex, int gamma_size = 255);
    MockCrtc(const MockCrtc &obj) = default;
    ~MockCrtc() = default;

    int pipeIndex;
    int gamma_size;
    drmModeModeInfo mode;
    bool modeValid = true;
    MockFb *currentFb = nullptr;
    MockFb *nextFb = nullptr;
    QRect cursorRect;
    std::shared_ptr<MockPlane> legacyPlane;
};

enum class PlaneType {
    Primary = 0,
    Overlay,
    Cursor
};

class MockPlane : public MockObject {
public:
    MockPlane(MockGpu *gpu, PlaneType type, int crtcIndex);
    MockPlane(const MockPlane &obj) = default;
    ~MockPlane() = default;

    MockFb *currentFb = nullptr;
    MockFb *nextFb = nullptr;
    int possibleCrtcs;
    PlaneType type;
};

class MockFb {
public:
    MockFb(MockGpu *gpu, uint32_t width, uint32_t height);
    ~MockFb();

    uint32_t id;
    uint32_t width, height;
    MockGpu *gpu;
};

struct Prop {
    uint32_t obj;
    uint32_t prop;
    uint64_t value;
};

struct _drmModeAtomicReq {
    bool legacyEmulation = false;
    QList<Prop> props;
};

#define MOCKDRM_DEVICE_CAP_ATOMIC 0xFF

class MockGpu {
public:
    MockGpu(int fd, const QString &devNode, int numCrtcs, int gammaSize = 255);
    ~MockGpu();

    MockConnector *findConnector(uint32_t id) const;
    MockCrtc *findCrtc(uint32_t id) const;
    MockPlane *findPlane(uint32_t id) const;
    MockPropertyBlob *getBlob(uint32_t id) const;

    void flipPage(uint32_t crtcId);

    int fd;
    QString devNode;
    QByteArray name = QByteArrayLiteral("mock");
    QMap<uint32_t, uint64_t> clientCaps;
    QMap<uint32_t, uint64_t> deviceCaps;

    uint32_t idCounter = 1;
    QList<MockObject *> objects;

    QList<std::shared_ptr<MockConnector>> connectors;
    QList<drmModeConnectorPtr> drmConnectors;

    QList<std::shared_ptr<MockEncoder>> encoders;
    QList<drmModeEncoderPtr> drmEncoders;

    QList<std::shared_ptr<MockCrtc>> crtcs;
    QList<drmModeCrtcPtr> drmCrtcs;

    QList<std::shared_ptr<MockPlane>> planes;
    QList<drmModePlanePtr> drmPlanes;

    QList<MockFb *> fbs;
    std::vector<std::unique_ptr<MockPropertyBlob>> propertyBlobs;

    QList<drmModeResPtr> resPtrs;
    QList<drmModePropertyPtr> drmProps;
    QList<drmModePropertyBlobPtr> drmPropertyBlobs;
    QList<drmModeObjectPropertiesPtr> drmObjectProperties;
    QList<drmModePlaneResPtr> drmPlaneRes;
    std::mutex m_mutex;
};



