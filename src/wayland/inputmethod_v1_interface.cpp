/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "inputmethod_v1_interface.h"
#include "display.h"
#include "keyboard_interface.h"
#include "keyboard_interface_p.h"
#include "output_interface.h"
#include "seat_interface.h"
#include "surface_interface.h"
#include "surfacerole_p.h"
#include "utils/common.h"
#include "utils/ramfile.h"

#include <QHash>

#include <unistd.h>

#include "qwayland-server-input-method-unstable-v1.h"
#include "qwayland-server-text-input-unstable-v1.h"
#include "qwayland-server-wayland.h"

namespace KWaylandServer
{
static int s_version = 1;

class InputKeyboardV1InterfacePrivate : public QtWaylandServer::wl_keyboard
{
public:
    InputKeyboardV1InterfacePrivate()
    {
    }
};

InputMethodGrabV1::InputMethodGrabV1(QObject *parent)
    : QObject(parent)
    , d(new InputKeyboardV1InterfacePrivate)
{
}

InputMethodGrabV1::~InputMethodGrabV1()
{
}

void InputMethodGrabV1::sendKeymap(const QByteArray &keymap)
{
    KWin::RamFile keymapFile("kwin-xkb-input-method-grab-keymap", keymap.constData(), keymap.size() + 1); // include QByteArray null terminator

    const auto resources = d->resourceMap();
    for (auto r : resources) {
        d->send_keymap(r->handle, QtWaylandServer::wl_keyboard::keymap_format::keymap_format_xkb_v1, keymapFile.fd(), keymapFile.size());
    }
}

void InputMethodGrabV1::sendKey(quint32 serial, quint32 timestamp, quint32 key, KeyboardKeyState state)
{
    const auto resources = d->resourceMap();
    for (auto r : resources) {
        d->send_key(r->handle, serial, timestamp, key, quint32(state));
    }
}

void InputMethodGrabV1::sendModifiers(quint32 serial, quint32 depressed, quint32 latched, quint32 locked, quint32 group)
{
    const auto resources = d->resourceMap();
    for (auto r : resources) {
        d->send_modifiers(r->handle, serial, depressed, latched, locked, group);
    }
}

class InputMethodContextV1InterfacePrivate : public QtWaylandServer::zwp_input_method_context_v1
{
public:
    InputMethodContextV1InterfacePrivate(InputMethodContextV1Interface *q)
        : zwp_input_method_context_v1()
        , q(q)
    {
    }

    ~InputMethodContextV1InterfacePrivate()
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
        const auto mods = QByteArray(static_cast<const char *>(map->data), map->size);

        Q_EMIT q->modifiersMap(mods);
    }
    void zwp_input_method_context_v1_keysym(Resource *, uint32_t serial, uint32_t time, uint32_t sym, uint32_t state, uint32_t modifiers) override
    {
        Q_EMIT q->keysym(serial, time, sym, state == WL_KEYBOARD_KEY_STATE_PRESSED, modifiers);
    }
    void zwp_input_method_context_v1_grab_keyboard(Resource *resource, uint32_t id) override
    {
        m_keyboardGrab.reset(new InputMethodGrabV1(q));
        m_keyboardGrab->d->add(resource->client(), id, 1);
        Q_EMIT q->keyboardGrabRequested(m_keyboardGrab.get());
    }
    void zwp_input_method_context_v1_key(Resource *, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) override
    {
        Q_EMIT q->key(serial, time, key, state == WL_KEYBOARD_KEY_STATE_PRESSED);
    }
    void zwp_input_method_context_v1_modifiers(Resource *,
                                               uint32_t serial,
                                               uint32_t mods_depressed,
                                               uint32_t mods_latched,
                                               uint32_t mods_locked,
                                               uint32_t group) override
    {
        Q_EMIT q->modifiers(serial, mods_depressed, mods_latched, mods_locked, group);
    }
    void zwp_input_method_context_v1_language(Resource *, uint32_t serial, const QString &language) override
    {
        Q_EMIT q->language(serial, language);
    }
    void zwp_input_method_context_v1_text_direction(Resource *, uint32_t serial, uint32_t direction) override
    {
        Qt::LayoutDirection qtDirection;
        switch (direction) {
        case ZWP_TEXT_INPUT_V1_TEXT_DIRECTION_LTR:
            qtDirection = Qt::LeftToRight;
            break;
        case ZWP_TEXT_INPUT_V1_TEXT_DIRECTION_RTL:
            qtDirection = Qt::RightToLeft;
            break;
        case ZWP_TEXT_INPUT_V1_TEXT_DIRECTION_AUTO:
            qtDirection = Qt::LayoutDirectionAuto;
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
        Q_EMIT q->textDirection(serial, qtDirection);
    }

    void zwp_input_method_context_v1_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    InputMethodContextV1Interface *const q;
    std::unique_ptr<InputMethodGrabV1> m_keyboardGrab;
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

void InputMethodContextV1Interface::sendContentType(TextInputContentHints hint, TextInputContentPurpose purpose)
{
    quint32 contentHint = QtWaylandServer::zwp_text_input_v1::content_hint_none;
    quint32 contentPurpose;

    if (hint.testFlag(TextInputContentHint::AutoCapitalization)) {
        contentHint |= QtWaylandServer::zwp_text_input_v1::content_hint_auto_capitalization;
    }
    if (hint.testFlag(TextInputContentHint::AutoCorrection)) {
        contentHint |= QtWaylandServer::zwp_text_input_v1::content_hint_auto_correction;
    }
    if (hint.testFlag(TextInputContentHint::AutoCompletion)) {
        contentHint |= QtWaylandServer::zwp_text_input_v1::content_hint_auto_completion;
    }
    if (hint.testFlag(TextInputContentHint::LowerCase)) {
        contentHint |= QtWaylandServer::zwp_text_input_v1::content_hint_lowercase;
    }
    if (hint.testFlag(TextInputContentHint::UpperCase)) {
        contentHint |= QtWaylandServer::zwp_text_input_v1::content_hint_uppercase;
    }
    if (hint.testFlag(TextInputContentHint::TitleCase)) {
        contentHint |= QtWaylandServer::zwp_text_input_v1::content_hint_titlecase;
    }
    if (hint.testFlag(TextInputContentHint::HiddenText)) {
        contentHint |= QtWaylandServer::zwp_text_input_v1::content_hint_hidden_text;
    }
    if (hint.testFlag(TextInputContentHint::SensitiveData)) {
        contentHint |= QtWaylandServer::zwp_text_input_v1::content_hint_lowercase;
    }
    if (hint.testFlag(TextInputContentHint::Latin)) {
        contentHint |= QtWaylandServer::zwp_text_input_v1::content_hint_latin;
    }
    if (hint.testFlag(TextInputContentHint::MultiLine)) {
        contentHint |= QtWaylandServer::zwp_text_input_v1::content_hint_multiline;
    }
    if (hint.testFlag(TextInputContentHint::None)) {
        contentHint |= QtWaylandServer::zwp_text_input_v1::content_hint_none;
    }

    switch (purpose) {
    case TextInputContentPurpose::Alpha:
        contentPurpose = QtWaylandServer::zwp_text_input_v1::content_purpose_alpha;
        break;
    case TextInputContentPurpose::Digits:
        contentPurpose = QtWaylandServer::zwp_text_input_v1::content_purpose_digits;
        break;
    case TextInputContentPurpose::Number:
        contentPurpose = QtWaylandServer::zwp_text_input_v1::content_purpose_number;
        break;
    case TextInputContentPurpose::Phone:
        contentPurpose = QtWaylandServer::zwp_text_input_v1::content_purpose_phone;
        break;
    case TextInputContentPurpose::Url:
        contentPurpose = QtWaylandServer::zwp_text_input_v1::content_purpose_url;
        break;
    case TextInputContentPurpose::Email:
        contentPurpose = QtWaylandServer::zwp_text_input_v1::content_purpose_email;
        break;
    case TextInputContentPurpose::Name:
        contentPurpose = QtWaylandServer::zwp_text_input_v1::content_purpose_name;
        break;
    case TextInputContentPurpose::Password:
        contentPurpose = QtWaylandServer::zwp_text_input_v1::content_purpose_password;
        break;
    case TextInputContentPurpose::Date:
        contentPurpose = QtWaylandServer::zwp_text_input_v1::content_purpose_date;
        break;
    case TextInputContentPurpose::Time:
        contentPurpose = QtWaylandServer::zwp_text_input_v1::content_purpose_time;
        break;
    case TextInputContentPurpose::DateTime:
        contentPurpose = QtWaylandServer::zwp_text_input_v1::content_purpose_datetime;
        break;
    case TextInputContentPurpose::Terminal:
        contentPurpose = QtWaylandServer::zwp_text_input_v1::content_purpose_terminal;
        break;
    case TextInputContentPurpose::Normal:
    default:
        contentPurpose = QtWaylandServer::zwp_text_input_v1::content_purpose_normal;
    }

    for (auto r : d->resourceMap()) {
        d->send_content_type(r->handle, contentHint, contentPurpose);
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

InputMethodGrabV1 *InputMethodContextV1Interface::keyboardGrab() const
{
    return d->m_keyboardGrab.get();
}

class InputPanelSurfaceV1InterfacePrivate : public QtWaylandServer::zwp_input_panel_surface_v1, public SurfaceRole
{
    friend class InputPanelSurfaceV1Interface;

public:
    InputPanelSurfaceV1InterfacePrivate(SurfaceInterface *surface, quint32 id, InputPanelSurfaceV1Interface *q)
        : zwp_input_panel_surface_v1()
        , SurfaceRole(surface, QByteArrayLiteral("input_panel_surface_v1"))
        , q(q)
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

    void commit() override
    {
    }

    void zwp_input_panel_surface_v1_destroy_resource(Resource *) override
    {
        delete q;
    }

    InputPanelSurfaceV1Interface *const q;
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

        SurfaceRole *surfaceRole = SurfaceRole::get(surface);
        if (surfaceRole) {
            wl_resource_post_error(resource->handle, 0, "the surface already has a role assigned %s", surfaceRole->name().constData());
            return;
        }

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
        send_activate(resource->handle, addedResource->handle);
    }

    std::unique_ptr<InputMethodContextV1Interface> m_context;
    InputMethodV1Interface *const q;
    Display *const m_display;
};

InputMethodV1Interface::InputMethodV1Interface(Display *d, QObject *parent)
    : QObject(parent)
    , d(new InputMethodV1InterfacePrivate(d, this))
{
}

InputMethodV1Interface::~InputMethodV1Interface() = default;

void InputMethodV1Interface::sendActivate()
{
    if (d->m_context) {
        return;
    }

    d->m_context.reset(new InputMethodContextV1Interface(this));

    for (auto resource : d->resourceMap()) {
        auto connection = d->m_context->d->add(resource->client(), resource->version());
        d->send_activate(resource->handle, connection->handle);
    }
}

void InputMethodV1Interface::sendDeactivate()
{
    if (!d->m_context) {
        return;
    }

    for (auto resource : d->resourceMap()) {
        auto connection = d->m_context->d->resourceMap().value(resource->client());
        if (connection) {
            d->send_deactivate(resource->handle, connection->handle);
        }
    }
    d->m_context.reset();
}

InputMethodContextV1Interface *InputMethodV1Interface::context() const
{
    return d->m_context.get();
}

}
