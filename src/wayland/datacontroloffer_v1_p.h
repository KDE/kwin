#pragma once

#include <qwayland-server-ext-data-control-v1.h>
#include <qwayland-server-wlr-data-control-unstable-v1.h>

#include <QPointer>

namespace KWin
{
class AbstractDataSource;
class DataControlOfferV1Interface;

class DataControlOfferV1InterfacePrivate
{
public:
    DataControlOfferV1InterfacePrivate(AbstractDataSource *source);
    virtual ~DataControlOfferV1InterfacePrivate() = default;

    virtual void sendOffer(const QString &mimeType) = 0;

    DataControlOfferV1Interface *q;
    QPointer<AbstractDataSource> source;
};

class ExtDataControlOfferPrivate : public DataControlOfferV1InterfacePrivate, public QtWaylandServer::ext_data_control_offer_v1
{
public:
    ExtDataControlOfferPrivate(AbstractDataSource *source, wl_resource *resource);

    void sendOffer(const QString &mimeType) override
    {
        send_offer(mimeType);
    }

protected:
    void ext_data_control_offer_v1_receive(Resource *resource, const QString &mime_type, int32_t fd) override;
    void ext_data_control_offer_v1_destroy(Resource *resource) override;
    void ext_data_control_offer_v1_destroy_resource(Resource *resource) override;
};

class WlrDataControlOfferPrivate : public DataControlOfferV1InterfacePrivate, public QtWaylandServer::zwlr_data_control_offer_v1
{
public:
    WlrDataControlOfferPrivate(AbstractDataSource *source, wl_resource *resource);

    void sendOffer(const QString &mimeType) override
    {
        send_offer(mimeType);
    }

protected:
    void zwlr_data_control_offer_v1_receive(Resource *resource, const QString &mime_type, int32_t fd) override;
    void zwlr_data_control_offer_v1_destroy(Resource *resource) override;
    void zwlr_data_control_offer_v1_destroy_resource(Resource *resource) override;
};

}
