/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "inputmethod_v1_interface.h"
#include "seat_interface.h"
#include "display.h"
#include "surface_interface.h"
#include "output_interface.h"
#include "surfacerole_p.h"

#include <QHash>
#include "qwayland-server-input-method-unstable-v1.h"
#include "wayland-text-server-protocol.h"

namespace KWaylandServer
{

static int s_version = 1;

class InputMethodContextV1InterfacePrivate : public QtWaylandServer::zwp_input_method_context_v1
{
public:
    InputMethodContextV1InterfacePrivate(InputMethodContextV1Interface *q)
        : zwp_input_method_context_v1()
        , q(q)
    {
    }

    void zwp_input_method_context_v1_commit_string(Resource *, uint32_t serial, const QString &text) override
    {
        Q_EMIT q->commitString(serial, text);
    }
    void zwp_input_method_context_v1_preedit_string(Resource *, uint32_t serial, const QString &text, const QString &commit) override
    {
        Q_EMIT q->preeditString(serial, text, commit);
    }

    void zwp_input_method_context_v1_preedit_styling(Resource *, uint32_t index, uint32_t length, uint32_t style) override
    {
        Q_EMIT q->preeditStyling(index, length, style);
    }
    void zwp_input_method_context_v1_preedit_cursor(Resource *, int32_t index) override
    {
        Q_EMIT q->preeditCursor(index);
    }
    void zwp_input_method_context_v1_delete_surrounding_text(Resource *, int32_t index, uint32_t length) override
    {
        Q_EMIT q->deleteSurroundingText(index, length);
    }
    void zwp_input_method_context_v1_cursor_position(Resource *, int32_t index, int32_t anchor) override
    {
        Q_EMIT q->cursorPosition(index, anchor);
    }
    void zwp_input_method_context_v1_modifiers_map(Resource *, wl_array *map) override
    {
        const QList<QByteArray> modifiersMap = QByteArray::fromRawData(static_cast<const char *>(map->data), map->size).split('\0');

        mods.clear();
        for (const QByteArray &modifier : modifiersMap) {
            if (modifier == "Shift") {
                mods << Qt::ShiftModifier;
            } else if (modifier == "Alt") {
                mods << Qt::AltModifier;
            } else if (modifier == "Control") {
                mods << Qt::ControlModifier;
            } else if (modifier == "Mod1") {
                mods << Qt::AltModifier;
            } else if (modifier == "Mod4") {
                mods << Qt::MetaModifier;
            } else {
                mods << Qt::NoModifier;
            }
        }
    }
    void zwp_input_method_context_v1_keysym(Resource *, uint32_t serial, uint32_t time, uint32_t sym, uint32_t state, uint32_t modifiers) override
    {
        Q_EMIT q->keysym(serial, time, sym, state == WL_KEYBOARD_KEY_STATE_PRESSED, toQtModifiers(modifiers));
    }
    void zwp_input_method_context_v1_grab_keyboard(Resource *, uint32_t keyboard) override
    {
        Q_EMIT q->grabKeyboard(keyboard);
    }
    void zwp_input_method_context_v1_key(Resource *, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) override
    {
        Q_EMIT q->key(serial, time, key, state == WL_KEYBOARD_KEY_STATE_PRESSED);
    }
    void zwp_input_method_context_v1_modifiers(Resource *, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) override
    {
        Q_EMIT q->modifiers(serial, toQtModifiers(mods_depressed), toQtModifiers(mods_latched), toQtModifiers(mods_locked), group);
    }
    void zwp_input_method_context_v1_language(Resource *, uint32_t serial, const QString &language) override
    {
        Q_EMIT q->language(serial, language);
    }
    void zwp_input_method_context_v1_text_direction(Resource *, uint32_t serial, uint32_t direction) override
    {
        Qt::LayoutDirection qtDirection;
        switch (direction) {
        case WL_TEXT_INPUT_TEXT_DIRECTION_LTR:
            qtDirection = Qt::LeftToRight;
            break;
        case WL_TEXT_INPUT_TEXT_DIRECTION_RTL:
            qtDirection = Qt::RightToLeft;
            break;
        case WL_TEXT_INPUT_TEXT_DIRECTION_AUTO:
            qtDirection = Qt::LayoutDirectionAuto;
            break;
        }
        Q_EMIT q->textDirection(serial, qtDirection);
    }

    Qt::KeyboardModifiers toQtModifiers(uint32_t modifiers)
    {
        Qt::KeyboardModifiers ret = Qt::NoModifier;
        for (int i = 0; modifiers >>= 1; ++i) {
            ret |= mods[i];
        }
        return ret;
    }

    void zwp_input_method_context_v1_destroy_resource(Resource *resource) override
    {
        Q_UNUSED(resource)
        if (resourceMap().isEmpty()) {
            delete q;
        }
    }

    void zwp_input_method_context_v1_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

private:
    InputMethodContextV1Interface *const q;
    QVector<Qt::KeyboardModifiers> mods;
};

InputMethodContextV1Interface::InputMethodContextV1Interface(InputMethodV1Interface *parent)
    : QObject(parent)
    , d(new InputMethodContextV1InterfacePrivate(this))
{
}

InputMethodContextV1Interface::~InputMethodContextV1Interface() = default;

void InputMethodContextV1Interface::sendCommitState(uint32_t serial)
{
    for (auto r : d->resourceMap()) {
        d->send_commit_state(r->handle, serial);
    }
}

void InputMethodContextV1Interface::sendContentType(uint32_t hint, uint32_t purpose)
{
    for (auto r : d->resourceMap()) {
        d->send_content_type(r->handle, hint, purpose);
    }
}

void InputMethodContextV1Interface::sendInvokeAction(uint32_t button, uint32_t index)
{
    for (auto r : d->resourceMap()) {
        d->send_invoke_action(r->handle, button, index);
    }
}

void InputMethodContextV1Interface::sendPreferredLanguage(const QString &language)
{
    for (auto r : d->resourceMap()) {
        d->send_preferred_language(r->handle, language);
    }
}

void InputMethodContextV1Interface::sendReset()
{
    for (auto r : d->resourceMap()) {
        d->send_reset(r->handle);
    }
}

void InputMethodContextV1Interface::sendSurroundingText(const QString &text, uint32_t cursor, uint32_t anchor)
{
    for (auto r : d->resourceMap()) {
        d->send_surrounding_text(r->handle, text, cursor, anchor);
    }
}

class InputPanelSurfaceV1InterfacePrivate : public QtWaylandServer::zwp_input_panel_surface_v1, public SurfaceRole
{
    friend class InputPanelSurfaceV1Interface;
public:
    InputPanelSurfaceV1InterfacePrivate(SurfaceInterface *surface, quint32 id, InputPanelSurfaceV1Interface *q)
        : zwp_input_panel_surface_v1()
        , SurfaceRole(surface)
        , q(q)
        , m_id(id)
    {
    }

    void zwp_input_panel_surface_v1_set_overlay_panel(Resource *) override
    {
        Q_EMIT q->overlayPanel();
    }

    void zwp_input_panel_surface_v1_set_toplevel(Resource *, struct ::wl_resource *output, uint32_t position) override
    {
        Q_EMIT q->topLevel(OutputInterface::get(output), InputPanelSurfaceV1Interface::Position(position));
    }

    void commit() override {}

    void zwp_input_panel_surface_v1_destroy_resource(Resource *) override {
        delete q;
    }

    InputPanelSurfaceV1Interface *const q;
    const quint32 m_id;
};

InputPanelSurfaceV1Interface::InputPanelSurfaceV1Interface(SurfaceInterface *surface, quint32 id, QObject *parent)
    : QObject(parent)
    , d(new InputPanelSurfaceV1InterfacePrivate(surface, id, this))
{
}

InputPanelSurfaceV1Interface::~InputPanelSurfaceV1Interface()
{
}

class InputPanelV1InterfacePrivate : public QtWaylandServer::zwp_input_panel_v1
{
public:
    InputPanelV1InterfacePrivate(InputPanelV1Interface *q, Display *d)
        : zwp_input_panel_v1(*d, s_version)
        , q(q)
    {
    }

    void zwp_input_panel_v1_get_input_panel_surface(Resource *resource, uint32_t id, struct ::wl_resource *surfaceResource) override
    {
        auto surface = SurfaceInterface::get(surfaceResource);
        auto interface = new InputPanelSurfaceV1Interface(surface, id, nullptr);
        interface->d->init(resource->client(), id, resource->version());

        Q_EMIT q->inputPanelSurfaceAdded(interface);
    }

    InputPanelV1Interface *const q;
};

InputPanelV1Interface::InputPanelV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new InputPanelV1InterfacePrivate(this, display))
{
}

InputPanelV1Interface::~InputPanelV1Interface() = default;

quint32 InputPanelSurfaceV1Interface::id() const
{
    return d->m_id;
}

SurfaceInterface *InputPanelSurfaceV1Interface::surface() const
{
    return d->surface();
}

class InputMethodV1InterfacePrivate : public QtWaylandServer::zwp_input_method_v1
{
public:
    InputMethodV1InterfacePrivate(Display *d, InputMethodV1Interface *q)
        : zwp_input_method_v1(*d, s_version)
        , q(q)
        , m_display(d)
    {
    }

    void zwp_input_method_v1_bind_resource(Resource *resource) override
    {
        if (!m_context) {
            return;
        }

        auto addedResource = m_context->d->add(resource->client(), resource->version());

        if (m_enabled) {
            send_activate(resource->handle, addedResource->handle);
        }
    }

    QPointer<InputMethodContextV1Interface> m_context;
    InputMethodV1Interface *const q;
    Display *const m_display;

    bool m_enabled = false;
};

InputMethodV1Interface::InputMethodV1Interface(Display *d, QObject *parent)
    : QObject(parent)
    , d(new InputMethodV1InterfacePrivate(d, this))
{
}

InputMethodV1Interface::~InputMethodV1Interface() = default;

void InputMethodV1Interface::sendActivate()
{
    if (d->m_enabled) {
        return;
    }

    Q_ASSERT(!d->m_context);
    d->m_context = new InputMethodContextV1Interface(this);

    d->m_enabled = true;
    for (auto resource : d->resourceMap()) {
        auto connection = d->m_context->d->add(resource->client(), resource->version());
        d->send_activate(resource->handle, connection->handle);
    }
}

void InputMethodV1Interface::sendDeactivate()
{
    if (!d->m_enabled) {
        return;
    }

    d->m_enabled = false;
    if (d->m_context) {
        for (auto resource : d->resourceMap()) {
            auto connection = d->m_context->d->resourceMap().value(resource->client());
            d->send_deactivate(resource->handle, connection->handle);
        }
        d->m_context = nullptr;
    }
}

InputMethodContextV1Interface *InputMethodV1Interface::context() const
{
    return d->m_context;
}

}
