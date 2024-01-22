/*
    SPDX-FileCopyrightText: 2023 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "securitycontext_v1.h"
#include "display.h"
#include "utils/filedescriptor.h"
#include "wayland/display_p.h"

#include <qwayland-server-security-context-v1.h>

#include <QDebug>

class QSocketNotifier;

namespace KWin
{
static const quint32 s_version = 1;

class SecurityContextManagerV1InterfacePrivate : public QtWaylandServer::wp_security_context_manager_v1
{
public:
    SecurityContextManagerV1InterfacePrivate(Display *display);

protected:
    void wp_security_context_manager_v1_destroy(Resource *resource) override;
    void wp_security_context_manager_v1_create_listener(Resource *resource, uint32_t id, int32_t listen_fd, int32_t close_fd) override;

private:
    Display *m_display;
};

class SecurityContextV1Interface : public QtWaylandServer::wp_security_context_v1
{
public:
    SecurityContextV1Interface(Display *display, int listenFd, int closeFd, wl_resource *resource);

protected:
    void wp_security_context_v1_set_sandbox_engine(Resource *resource, const QString &name) override;
    void wp_security_context_v1_set_app_id(Resource *resource, const QString &app_id) override;
    void wp_security_context_v1_set_instance_id(Resource *resource, const QString &instance_id) override;
    void wp_security_context_v1_commit(Resource *resource) override;
    void wp_security_context_v1_destroy_resource(Resource *resource) override;
    void wp_security_context_v1_destroy(Resource *resource) override;

private:
    Display *m_display;
    FileDescriptor m_listenFd;
    FileDescriptor m_closeFd;
    bool m_committed = false;
    QString m_sandboxEngine;
    QString m_appId;
    QString m_instanceId;
};

SecurityContextManagerV1Interface::SecurityContextManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new SecurityContextManagerV1InterfacePrivate(display))
{
}

SecurityContextManagerV1Interface::~SecurityContextManagerV1Interface()
{
}

SecurityContextManagerV1InterfacePrivate::SecurityContextManagerV1InterfacePrivate(Display *display)
    : QtWaylandServer::wp_security_context_manager_v1(*display, s_version)
    , m_display(display)
{
}

void SecurityContextManagerV1InterfacePrivate::wp_security_context_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void SecurityContextManagerV1InterfacePrivate::wp_security_context_manager_v1_create_listener(Resource *resource, uint32_t id, int32_t listen_fd, int32_t close_fd)
{
    auto *securityContextResource = wl_resource_create(resource->client(), &wp_security_context_v1_interface, resource->version(), id);
    if (!securityContextResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    new SecurityContextV1Interface(m_display, listen_fd, close_fd, securityContextResource);
}

SecurityContextV1Interface::SecurityContextV1Interface(Display *display, int listenFd, int closeFd, wl_resource *resource)
    : QtWaylandServer::wp_security_context_v1(resource)
    , m_display(display)
    , m_listenFd(listenFd)
    , m_closeFd(closeFd)
{
}

void SecurityContextV1Interface::wp_security_context_v1_set_sandbox_engine(Resource *resource, const QString &name)
{
    if (m_committed) {
        wl_resource_post_error(resource->handle, error_already_used, "Already committed");
        return;
    }
    m_sandboxEngine = name;
}

void SecurityContextV1Interface::wp_security_context_v1_set_app_id(Resource *resource, const QString &app_id)
{
    if (m_committed) {
        wl_resource_post_error(resource->handle, error_already_used, "Already committed");
        return;
    }
    if (app_id.isEmpty()) {
        wl_resource_post_error(resource->handle, error_invalid_metadata, "App ID cannot be empty");
        return;
    }
    m_appId = app_id;
}

void SecurityContextV1Interface::wp_security_context_v1_set_instance_id(Resource *resource, const QString &instance_id)
{
    if (m_committed) {
        wl_resource_post_error(resource->handle, error_already_used, "Already committed");
        return;
    }
    m_instanceId = instance_id;
}

void SecurityContextV1Interface::wp_security_context_v1_commit(Resource *resource)
{
    if (m_committed) {
        wl_resource_post_error(resource->handle, error_already_used, "Already committed");
        return;
    }
    if (m_appId.isEmpty()) {
        wl_resource_post_error(resource->handle, error_invalid_metadata, "App ID cannot be empty");
        return;
    }
    if (m_sandboxEngine.isEmpty()) {
        wl_resource_post_error(resource->handle, error_invalid_metadata, "Sandbox engine cannot be empty");
        return;
    }

    m_committed = true;

    // lifespan is managed by the closeFD's state
    new SecurityContext(m_display, std::move(m_listenFd), std::move(m_closeFd), m_appId);
}

void SecurityContextV1Interface::wp_security_context_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void SecurityContextV1Interface::wp_security_context_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

}
