/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "outputdevice_interface.h"
#include "display_p.h"
#include "global_p.h"
#include "display.h"
#include "logging.h"

#include <wayland-server.h>
#include "wayland-org_kde_kwin_outputdevice-server-protocol.h"
#include <QDebug>

namespace KWaylandServer
{

class OutputDeviceInterface::Private : public Global::Private
{
public:
    struct ResourceData {
        wl_resource *resource;
        uint32_t version;
    };
    Private(OutputDeviceInterface *q, Display *d);
    ~Private();


    void updateGeometry();
    void updateUuid();
    void updateEdid();
    void updateEnabled();
    void updateScale();
    void updateColorCurves();
    void updateEisaId();
    void updateSerialNumber();

    void sendGeometry(wl_resource *resource);
    void sendMode(wl_resource *resource, const Mode &mode);
    void sendDone(const ResourceData &data);
    void sendUuid(const ResourceData &data);
    void sendEdid(const ResourceData &data);
    void sendEnabled(const ResourceData &data);
    void sendScale(const ResourceData &data);
    void sendColorCurves(const ResourceData &data);
    void sendEisaId(const ResourceData &data);
    void sendSerialNumber(const ResourceData &data);

    QSize physicalSize;
    QPoint globalPosition;
    QString manufacturer = QStringLiteral("org.kde.kwin");
    QString model = QStringLiteral("none");
    qreal scale = 1.0;
    QString serialNumber;
    QString eisaId;
    SubPixel subPixel = SubPixel::Unknown;
    Transform transform = Transform::Normal;
    ColorCurves colorCurves;
    QList<Mode> modes;
    Mode currentMode;
    QList<ResourceData> resources;

    QByteArray edid;
    Enablement enabled = Enablement::Enabled;
    QByteArray uuid;

    static OutputDeviceInterface *get(wl_resource *native);

private:
    static Private *cast(wl_resource *native);
    static void unbind(wl_resource *resource);
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    int32_t toTransform() const;
    int32_t toSubPixel() const;

    static const quint32 s_version;
    OutputDeviceInterface *q;
    static QVector<Private*> s_privates;
};

const quint32 OutputDeviceInterface::Private::s_version = 2;

QVector<OutputDeviceInterface::Private*> OutputDeviceInterface::Private::s_privates;

OutputDeviceInterface::Private::Private(OutputDeviceInterface *q, Display *d)
    : Global::Private(d, &org_kde_kwin_outputdevice_interface, s_version)
    , q(q)
{
    DisplayPrivate *displayPrivate = DisplayPrivate::get(display);
    displayPrivate->outputdevices.append(q);

    s_privates << this;
}

OutputDeviceInterface::Private::~Private()
{
    if (display) {
        DisplayPrivate *displayPrivate = DisplayPrivate::get(display);
        displayPrivate->outputdevices.removeOne(q);
    }

    s_privates.removeAll(this);
}

OutputDeviceInterface *OutputDeviceInterface::Private::get(wl_resource *native)
{
    if (Private *p = cast(native)) {
        return p->q;
    }
    return nullptr;
}

OutputDeviceInterface::Private *OutputDeviceInterface::Private::cast(wl_resource *native)
{
    for (auto it = s_privates.constBegin(); it != s_privates.constEnd(); ++it) {
        const auto &resources = (*it)->resources;
        auto rit = std::find_if(resources.begin(), resources.end(), [native] (const ResourceData &data) { return data.resource == native; });
        if (rit != resources.end()) {
            return (*it);
        }
    }
    return nullptr;
}

OutputDeviceInterface::OutputDeviceInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
    Q_D();
    connect(this, &OutputDeviceInterface::currentModeChanged, this,
        [d] {
            Q_ASSERT(d->currentMode.id >= 0);
            for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
                d->sendMode((*it).resource, d->currentMode);
                d->sendDone(*it);
            }
            wl_display_flush_clients(*(d->display));
        }
    );
    connect(this, &OutputDeviceInterface::subPixelChanged,       this, [d] { d->updateGeometry(); });
    connect(this, &OutputDeviceInterface::transformChanged,      this, [d] { d->updateGeometry(); });
    connect(this, &OutputDeviceInterface::globalPositionChanged, this, [d] { d->updateGeometry(); });
    connect(this, &OutputDeviceInterface::modelChanged,          this, [d] { d->updateGeometry(); });
    connect(this, &OutputDeviceInterface::manufacturerChanged,   this, [d] { d->updateGeometry(); });
    connect(this, &OutputDeviceInterface::scaleFChanged,         this, [d] { d->updateScale(); });
    connect(this, &OutputDeviceInterface::colorCurvesChanged,    this, [d] { d->updateColorCurves(); });
}

OutputDeviceInterface::~OutputDeviceInterface() = default;

QSize OutputDeviceInterface::pixelSize() const
{
    Q_D();

    if (d->currentMode.id == -1) {
        return QSize();
    }
    return d->currentMode.size;
}

OutputDeviceInterface *OutputDeviceInterface::get(wl_resource* native)
{
    return Private::get(native);
}

int OutputDeviceInterface::refreshRate() const
{
    Q_D();

    if (d->currentMode.id == -1) {
        return 60000;
    }
    return d->currentMode.refreshRate;
}

void OutputDeviceInterface::addMode(Mode &mode)
{
    Q_ASSERT(!isValid());
    Q_ASSERT(mode.id >= 0);
    Q_ASSERT(mode.size.isValid());
    Q_D();

    auto currentModeIt = std::find_if(d->modes.begin(), d->modes.end(),
        [](const Mode &mode) {
            return mode.flags.testFlag(ModeFlag::Current);
        }
    );
    if (currentModeIt == d->modes.end() && !mode.flags.testFlag(ModeFlag::Current)) {
        // no mode with current flag - enforce
        mode.flags |= ModeFlag::Current;
    }
    if (currentModeIt != d->modes.end() && mode.flags.testFlag(ModeFlag::Current)) {
        // another mode has the current flag - remove
        (*currentModeIt).flags &= ~uint(ModeFlag::Current);
    }

    if (mode.flags.testFlag(ModeFlag::Preferred)) {
        // remove from existing Preferred mode
        auto preferredIt = std::find_if(d->modes.begin(), d->modes.end(),
            [](const Mode &mode) {
                return mode.flags.testFlag(ModeFlag::Preferred);
            }
        );
        if (preferredIt != d->modes.end()) {
            (*preferredIt).flags &= ~uint(ModeFlag::Preferred);
        }
    }

    auto existingModeIt = std::find_if(d->modes.begin(), d->modes.end(),
        [mode](const Mode &mode_it) {
            return mode.size == mode_it.size &&
                   mode.refreshRate == mode_it.refreshRate &&
                   mode.id == mode_it.id;
        }
    );
    auto emitChanges = [this, d, mode] {
        emit modesChanged();
        if (mode.flags.testFlag(ModeFlag::Current)) {
            d->currentMode = mode;
            emit refreshRateChanged(mode.refreshRate);
            emit pixelSizeChanged(mode.size);
            emit currentModeChanged();
        }
    };
    if (existingModeIt != d->modes.end()) {
        if ((*existingModeIt).flags == mode.flags) {
            // nothing to do
            return;
        }
        (*existingModeIt).flags = mode.flags;
        emitChanges();
        return;
    } else {
        auto idIt = std::find_if(d->modes.constBegin(), d->modes.constEnd(),
                                        [mode](const Mode &mode_it) {
                                            return mode.id == mode_it.id;
                                        }
        );
        if (idIt != d->modes.constEnd()) {
            qCWarning(KWAYLAND_SERVER) << "Duplicate Mode id" << mode.id << ": not adding mode" << mode.size << mode.refreshRate;
            return;
        }

    }
    d->modes << mode;
    emitChanges();
}

void OutputDeviceInterface::setCurrentMode(const int modeId)
{
    Q_D();
    auto currentModeIt = std::find_if(d->modes.begin(), d->modes.end(),
        [](const Mode &mode) {
            return mode.flags.testFlag(ModeFlag::Current);
        }
    );
    if (currentModeIt != d->modes.end()) {
        // another mode has the current flag - remove
        (*currentModeIt).flags &= ~uint(ModeFlag::Current);
    }

    auto existingModeIt = std::find_if(d->modes.begin(), d->modes.end(),
        [modeId](const Mode &mode) {
            return mode.id == modeId;
        }
    );

    Q_ASSERT(existingModeIt != d->modes.end());
    (*existingModeIt).flags |= ModeFlag::Current;
    d->currentMode = *existingModeIt;
    emit modesChanged();
    emit refreshRateChanged((*existingModeIt).refreshRate);
    emit pixelSizeChanged((*existingModeIt).size);
    emit currentModeChanged();
}

bool OutputDeviceInterface::setCurrentMode(const QSize &size, int refreshRate)
{
    Q_D();
    auto mode = std::find_if(d->modes.constBegin(), d->modes.constEnd(),
        [size, refreshRate](const Mode &mode) {
            return mode.size == size && mode.refreshRate == refreshRate;
        }
    );
    if (mode == d->modes.constEnd()) {
        return false;
    }
    setCurrentMode((*mode).id);
    return true;
}

int32_t OutputDeviceInterface::Private::toTransform() const
{
    switch (transform) {
    case Transform::Normal:
        return WL_OUTPUT_TRANSFORM_NORMAL;
    case Transform::Rotated90:
        return WL_OUTPUT_TRANSFORM_90;
    case Transform::Rotated180:
        return WL_OUTPUT_TRANSFORM_180;
    case Transform::Rotated270:
        return WL_OUTPUT_TRANSFORM_270;
    case Transform::Flipped:
        return WL_OUTPUT_TRANSFORM_FLIPPED;
    case Transform::Flipped90:
        return WL_OUTPUT_TRANSFORM_FLIPPED_90;
    case Transform::Flipped180:
        return WL_OUTPUT_TRANSFORM_FLIPPED_180;
    case Transform::Flipped270:
        return WL_OUTPUT_TRANSFORM_FLIPPED_270;
    }
    abort();
}

int32_t OutputDeviceInterface::Private::toSubPixel() const
{
    switch (subPixel) {
    case SubPixel::Unknown:
        return WL_OUTPUT_SUBPIXEL_UNKNOWN;
    case SubPixel::None:
        return WL_OUTPUT_SUBPIXEL_NONE;
    case SubPixel::HorizontalRGB:
        return WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB;
    case SubPixel::HorizontalBGR:
        return WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR;
    case SubPixel::VerticalRGB:
        return WL_OUTPUT_SUBPIXEL_VERTICAL_RGB;
    case SubPixel::VerticalBGR:
        return WL_OUTPUT_SUBPIXEL_VERTICAL_BGR;
    }
    abort();
}

void OutputDeviceInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&org_kde_kwin_outputdevice_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_user_data(resource, this);
    wl_resource_set_destructor(resource, unbind);
    ResourceData r;
    r.resource = resource;
    r.version = version;
    resources << r;

    sendGeometry(resource);
    sendScale(r);
    sendColorCurves(r);
    sendEisaId(r);
    sendSerialNumber(r);

    auto currentModeIt = modes.constEnd();
    for (auto it = modes.constBegin(); it != modes.constEnd(); ++it) {
        const Mode &mode = *it;
        if (mode.flags.testFlag(ModeFlag::Current)) {
            // needs to be sent as last mode
            currentModeIt = it;
            continue;
        }
        sendMode(resource, mode);
    }

    if (currentModeIt != modes.constEnd()) {
        sendMode(resource, *currentModeIt);
    }

    sendUuid(r);
    sendEdid(r);
    sendEnabled(r);

    sendDone(r);
    c->flush();
}

void OutputDeviceInterface::Private::unbind(wl_resource *resource)
{
    Private *o = cast(resource);
    if (!o) {
        return;
    }
    auto it = std::find_if(o->resources.begin(), o->resources.end(), [resource](const ResourceData &r) { return r.resource == resource; });
    if (it != o->resources.end()) {
        o->resources.erase(it);
    }
}

void OutputDeviceInterface::Private::sendMode(wl_resource *resource, const Mode &mode)
{
    int32_t flags = 0;
    if (mode.flags.testFlag(ModeFlag::Current)) {
        flags |= WL_OUTPUT_MODE_CURRENT;
    }
    if (mode.flags.testFlag(ModeFlag::Preferred)) {
        flags |= WL_OUTPUT_MODE_PREFERRED;
    }
    org_kde_kwin_outputdevice_send_mode(resource,
                        flags,
                        mode.size.width(),
                        mode.size.height(),
                        mode.refreshRate,
                        mode.id);

}

void OutputDeviceInterface::Private::sendGeometry(wl_resource *resource)
{
    org_kde_kwin_outputdevice_send_geometry(resource,
                            globalPosition.x(),
                            globalPosition.y(),
                            physicalSize.width(),
                            physicalSize.height(),
                            toSubPixel(),
                            qPrintable(manufacturer),
                            qPrintable(model),
                            toTransform());
}

void OutputDeviceInterface::Private::sendScale(const ResourceData &data)
{
    if (wl_resource_get_version(data.resource) < ORG_KDE_KWIN_OUTPUTDEVICE_SCALEF_SINCE_VERSION) {
        org_kde_kwin_outputdevice_send_scale(data.resource, qRound(scale));
    } else {
        org_kde_kwin_outputdevice_send_scalef(data.resource, wl_fixed_from_double(scale));
    }
}

void OutputDeviceInterface::Private::sendColorCurves(const ResourceData &data)
{
    if (data.version < ORG_KDE_KWIN_OUTPUTDEVICE_COLORCURVES_SINCE_VERSION) {
        return;
    }

    wl_array wlRed, wlGreen, wlBlue;

    auto fillArray = [](const QVector<quint16> &origin, wl_array *dest) {
        wl_array_init(dest);
        const size_t memLength = sizeof(uint16_t) * origin.size();
        void *s = wl_array_add(dest, memLength);
        memcpy(s, origin.data(), memLength);
    };
    fillArray(colorCurves.red, &wlRed);
    fillArray(colorCurves.green, &wlGreen);
    fillArray(colorCurves.blue, &wlBlue);

    org_kde_kwin_outputdevice_send_colorcurves(data.resource, &wlRed, &wlGreen, &wlBlue);

    wl_array_release(&wlRed);
    wl_array_release(&wlGreen);
    wl_array_release(&wlBlue);
}

void KWaylandServer::OutputDeviceInterface::Private::sendSerialNumber(const ResourceData &data)
{
    if (wl_resource_get_version(data.resource) >= ORG_KDE_KWIN_OUTPUTDEVICE_SERIAL_NUMBER_SINCE_VERSION) {
        org_kde_kwin_outputdevice_send_serial_number(data.resource,
                                            qPrintable(serialNumber));
    }
}

void KWaylandServer::OutputDeviceInterface::Private::sendEisaId(const ResourceData &data)
{
    if (wl_resource_get_version(data.resource) >= ORG_KDE_KWIN_OUTPUTDEVICE_EISA_ID_SINCE_VERSION) {
        org_kde_kwin_outputdevice_send_eisa_id(data.resource,
                                            qPrintable(eisaId));
    }
}


void OutputDeviceInterface::Private::sendDone(const ResourceData &data)
{
    org_kde_kwin_outputdevice_send_done(data.resource);
}

void OutputDeviceInterface::Private::updateGeometry()
{
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        sendGeometry((*it).resource);
        sendDone(*it);
    }
}

void OutputDeviceInterface::Private::updateScale()
{
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        sendScale(*it);
        sendDone(*it);
    }
}

void OutputDeviceInterface::Private::updateColorCurves()
{
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        sendColorCurves(*it);
        sendDone(*it);
    }
}

bool OutputDeviceInterface::ColorCurves::operator==(const ColorCurves &cc) const
{
    return red == cc.red && green == cc.green && blue == cc.blue;
}
bool OutputDeviceInterface::ColorCurves::operator!=(const ColorCurves &cc) const {
    return !operator==(cc);
}

#define SETTER(setterName, type, argumentName) \
    void OutputDeviceInterface::setterName(type arg) \
    { \
        Q_D(); \
        if (d->argumentName == arg) { \
            return; \
        } \
        d->argumentName = arg; \
        emit argumentName##Changed(d->argumentName); \
    }

SETTER(setPhysicalSize, const QSize&, physicalSize)
SETTER(setGlobalPosition, const QPoint&, globalPosition)
SETTER(setManufacturer, const QString&, manufacturer)
SETTER(setModel, const QString&, model)
SETTER(setSerialNumber, const QString&, serialNumber)
SETTER(setEisaId, const QString&, eisaId)
SETTER(setSubPixel, SubPixel, subPixel)
SETTER(setTransform, Transform, transform)

#undef SETTER

void OutputDeviceInterface::setScale(int scale)
{
    Q_D();
    if (d->scale == scale) {
        return;
    }
    d->scale = scale;
    emit scaleChanged(d->scale);
    emit scaleFChanged(d->scale);
}

void OutputDeviceInterface::setScaleF(qreal scale)
{
    Q_D();
    if (qFuzzyCompare(d->scale, scale)) {
        return;
    }
    d->scale = scale;
    emit scaleChanged(qRound(d->scale));
    emit scaleFChanged(d->scale);
}

QSize OutputDeviceInterface::physicalSize() const
{
    Q_D();
    return d->physicalSize;
}

QPoint OutputDeviceInterface::globalPosition() const
{
    Q_D();
    return d->globalPosition;
}

QString OutputDeviceInterface::manufacturer() const
{
    Q_D();
    return d->manufacturer;
}

QString OutputDeviceInterface::model() const
{
    Q_D();
    return d->model;
}

QString OutputDeviceInterface::serialNumber() const
{
    Q_D();
    return d->serialNumber;
}

QString OutputDeviceInterface::eisaId() const
{
    Q_D();
    return d->eisaId;
}

int OutputDeviceInterface::scale() const
{
    Q_D();
    return qRound(d->scale);
}

qreal OutputDeviceInterface::scaleF() const
{
    Q_D();
    return d->scale;
}


OutputDeviceInterface::SubPixel OutputDeviceInterface::subPixel() const
{
    Q_D();
    return d->subPixel;
}

OutputDeviceInterface::Transform OutputDeviceInterface::transform() const
{
    Q_D();
    return d->transform;
}

OutputDeviceInterface::ColorCurves OutputDeviceInterface::colorCurves() const
{
    Q_D();
    return d->colorCurves;
}

QList< OutputDeviceInterface::Mode > OutputDeviceInterface::modes() const
{
    Q_D();
    return d->modes;
}

int OutputDeviceInterface::currentModeId() const
{
    Q_D();
    for (const Mode &m: d->modes) {
        if (m.flags.testFlag(OutputDeviceInterface::ModeFlag::Current)) {
            return m.id;
        }
    }
    return -1;
}

OutputDeviceInterface::Private *OutputDeviceInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

void OutputDeviceInterface::setColorCurves(const ColorCurves &colorCurves)
{
    Q_D();

    if (d->colorCurves == colorCurves) {
        return;
    }
    d->colorCurves = colorCurves;
    emit colorCurvesChanged(d->colorCurves);
}

void OutputDeviceInterface::setEdid(const QByteArray &edid)
{
    Q_D();
    d->edid = edid;
    d->updateEdid();
    emit edidChanged();
}

QByteArray OutputDeviceInterface::edid() const
{
    Q_D();
    return d->edid;
}

void OutputDeviceInterface::setEnabled(OutputDeviceInterface::Enablement enabled)
{
    Q_D();
    if (d->enabled != enabled) {
        d->enabled = enabled;
        d->updateEnabled();
        emit enabledChanged();
    }
}

OutputDeviceInterface::Enablement OutputDeviceInterface::enabled() const
{
    Q_D();
    return d->enabled;
}

void OutputDeviceInterface::setUuid(const QByteArray &uuid)
{
    Q_D();
    if (d->uuid != uuid) {
        d->uuid = uuid;
        d->updateUuid();
        emit uuidChanged();
    }
}

QByteArray OutputDeviceInterface::uuid() const
{
    Q_D();
    return d->uuid;
}

void KWaylandServer::OutputDeviceInterface::Private::sendEdid(const ResourceData &data)
{
    org_kde_kwin_outputdevice_send_edid(data.resource,
                                        edid.toBase64().constData());
}

void KWaylandServer::OutputDeviceInterface::Private::sendEnabled(const ResourceData &data)
{
    int _enabled = 0;
    if (enabled == OutputDeviceInterface::Enablement::Enabled) {
        _enabled = 1;
    }
    org_kde_kwin_outputdevice_send_enabled(data.resource, _enabled);
}

void OutputDeviceInterface::Private::sendUuid(const ResourceData &data)
{
    org_kde_kwin_outputdevice_send_uuid(data.resource, uuid.constData());
}

void KWaylandServer::OutputDeviceInterface::Private::updateEnabled()
{
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        sendEnabled(*it);
    }
}

void KWaylandServer::OutputDeviceInterface::Private::updateEdid()
{
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        sendEdid(*it);
    }
}

void KWaylandServer::OutputDeviceInterface::Private::updateUuid()
{
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        sendUuid(*it);
    }
}

void KWaylandServer::OutputDeviceInterface::Private::updateEisaId()
{
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        sendEisaId(*it);
    }
}


}
