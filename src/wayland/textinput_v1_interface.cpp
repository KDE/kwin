/*
    SPDX-FileCopyrightText: 2022 Xuetian Weng <wengxt@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "wayland/textinput_v1_interface.h"
#include "display.h"
#include "seat_interface_p.h"
#include "surface_interface_p.h"
#include "textinput_v1_interface_p.h"
#include "wayland/display_p.h"
#include "wayland/seat_interface.h"
#include "wayland/surface_interface.h"
#include <algorithm>

namespace KWaylandServer
{

namespace
{
const quint32 s_version = 1;

// helpers
TextInputContentHints convertContentHint(uint32_t hint)
{
    const auto hints = QtWaylandServer::zwp_text_input_v1::content_hint(hint);
    TextInputContentHints ret = TextInputContentHint::None;

    if (hints & QtWaylandServer::zwp_text_input_v1::content_hint_auto_completion) {
        ret |= TextInputContentHint::AutoCompletion;
    }
    if (hints & QtWaylandServer::zwp_text_input_v1::content_hint_auto_correction) {
        ret |= TextInputContentHint::AutoCorrection;
    }
    if (hints & QtWaylandServer::zwp_text_input_v1::content_hint_auto_capitalization) {
        ret |= TextInputContentHint::AutoCapitalization;
    }
    if (hints & QtWaylandServer::zwp_text_input_v1::content_hint_lowercase) {
        ret |= TextInputContentHint::LowerCase;
    }
    if (hints & QtWaylandServer::zwp_text_input_v1::content_hint_uppercase) {
        ret |= TextInputContentHint::UpperCase;
    }
    if (hints & QtWaylandServer::zwp_text_input_v1::content_hint_titlecase) {
        ret |= TextInputContentHint::TitleCase;
    }
    if (hints & QtWaylandServer::zwp_text_input_v1::content_hint_hidden_text) {
        ret |= TextInputContentHint::HiddenText;
    }
    if (hints & QtWaylandServer::zwp_text_input_v1::content_hint_sensitive_data) {
        ret |= TextInputContentHint::SensitiveData;
    }
    if (hints & QtWaylandServer::zwp_text_input_v1::content_hint_latin) {
        ret |= TextInputContentHint::Latin;
    }
    if (hints & QtWaylandServer::zwp_text_input_v1::content_hint_multiline) {
        ret |= TextInputContentHint::MultiLine;
    }
    return ret;
}

TextInputContentPurpose convertContentPurpose(uint32_t purpose)
{
    const auto wlPurpose = QtWaylandServer::zwp_text_input_v1::content_purpose(purpose);

    switch (wlPurpose) {
    case QtWaylandServer::zwp_text_input_v1::content_purpose_alpha:
        return TextInputContentPurpose::Alpha;
    case QtWaylandServer::zwp_text_input_v1::content_purpose_digits:
        return TextInputContentPurpose::Digits;
    case QtWaylandServer::zwp_text_input_v1::content_purpose_number:
        return TextInputContentPurpose::Number;
    case QtWaylandServer::zwp_text_input_v1::content_purpose_phone:
        return TextInputContentPurpose::Phone;
    case QtWaylandServer::zwp_text_input_v1::content_purpose_url:
        return TextInputContentPurpose::Url;
    case QtWaylandServer::zwp_text_input_v1::content_purpose_email:
        return TextInputContentPurpose::Email;
    case QtWaylandServer::zwp_text_input_v1::content_purpose_name:
        return TextInputContentPurpose::Name;
    case QtWaylandServer::zwp_text_input_v1::content_purpose_password:
        return TextInputContentPurpose::Password;
    case QtWaylandServer::zwp_text_input_v1::content_purpose_date:
        return TextInputContentPurpose::Date;
    case QtWaylandServer::zwp_text_input_v1::content_purpose_time:
        return TextInputContentPurpose::Time;
    case QtWaylandServer::zwp_text_input_v1::content_purpose_datetime:
        return TextInputContentPurpose::DateTime;
    case QtWaylandServer::zwp_text_input_v1::content_purpose_terminal:
        return TextInputContentPurpose::Terminal;
    case QtWaylandServer::zwp_text_input_v1::content_purpose_normal:
        return TextInputContentPurpose::Normal;
    default:
        return TextInputContentPurpose::Normal;
    }
}

class EnabledEmitter
{
public:
    EnabledEmitter(TextInputV1Interface *q)
        : q(q)
        , m_wasEnabled(q->isEnabled())
    {
    }
    ~EnabledEmitter()
    {
        if (m_wasEnabled != q->isEnabled()) {
            Q_EMIT q->enabledChanged();
        }
    }

private:
    TextInputV1Interface *q;
    const bool m_wasEnabled;
};

}

TextInputManagerV1InterfacePrivate::TextInputManagerV1InterfacePrivate(TextInputManagerV1Interface *_q, Display *_display)
    : QtWaylandServer::zwp_text_input_manager_v1(*_display, s_version)
    , q(_q)
    , display(_display)
{
}

void TextInputManagerV1InterfacePrivate::zwp_text_input_manager_v1_create_text_input(Resource *resource, uint32_t id)
{
    auto seats = display->seats();
    if (seats.isEmpty()) {
        wl_resource_post_error(resource->handle, 0, "No seat on display.");
        return;
    }
    // KWin only has one seat, so it's ok to make textInputV1 belongs to the SeatInterface.
    TextInputV1InterfacePrivate *textInputPrivate = TextInputV1InterfacePrivate::get(seats[0]->textInputV1());
    textInputPrivate->add(resource->client(), id, resource->version());
}

TextInputManagerV1Interface::TextInputManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new TextInputManagerV1InterfacePrivate(this, display))
{
}

TextInputManagerV1Interface::~TextInputManagerV1Interface() = default;

void TextInputV1InterfacePrivate::sendEnter(SurfaceInterface *newSurface)
{
    EnabledEmitter emitter(q);
    // It should be always synchronized with SeatInterface::focusedTextInputSurface.
    Q_ASSERT(newSurface);
    surface = newSurface;

    if (auto resource = activated.value(newSurface)) {
        send_enter(resource->handle, newSurface->resource());
    }
}

void TextInputV1InterfacePrivate::sendLeave(SurfaceInterface *leavingSurface)
{
    // It should be always synchronized with SeatInterface::focusedTextInputSurface.
    Q_ASSERT(leavingSurface && surface == leavingSurface);
    EnabledEmitter emitter(q);
    surface = nullptr;

    if (auto resource = activated.value(leavingSurface)) {
        send_leave(resource->handle);
    }
}

void TextInputV1InterfacePrivate::preEdit(const QString &text, const QString &commit)
{
    forActivatedResource([this, &text, &commit](Resource *resource) {
        send_preedit_string(resource->handle, serialHash.value(resource), text, commit);
    });
}

void TextInputV1InterfacePrivate::preEditStyling(uint32_t index, uint32_t length, uint32_t style)
{
    forActivatedResource([this, index, length, style](Resource *resource) {
        send_preedit_styling(resource->handle, index, length, style);
    });
}

void TextInputV1InterfacePrivate::commitString(const QString &text)
{
    forActivatedResource([this, text](Resource *resource) {
        send_commit_string(resource->handle, serialHash.value(resource), text);
    });
}

void TextInputV1InterfacePrivate::keysymPressed(quint32 time, quint32 keysym, quint32 modifiers)
{
    forActivatedResource([this, time, keysym, modifiers](Resource *resource) {
        send_keysym(resource->handle, serialHash.value(resource), time, keysym, WL_KEYBOARD_KEY_STATE_PRESSED, modifiers);
    });
}

void TextInputV1InterfacePrivate::keysymReleased(quint32 time, quint32 keysym, quint32 modifiers)
{
    forActivatedResource([this, time, keysym, modifiers](Resource *resource) {
        send_keysym(resource->handle, serialHash.value(resource), time, keysym, WL_KEYBOARD_KEY_STATE_RELEASED, modifiers);
    });
}

void TextInputV1InterfacePrivate::deleteSurroundingText(qint32 index, quint32 length)
{
    forActivatedResource([this, index, length](Resource *resource) {
        send_delete_surrounding_text(resource->handle, index, length);
    });
}

void TextInputV1InterfacePrivate::setCursorPosition(qint32 index, qint32 anchor)
{
    forActivatedResource([this, index, anchor](Resource *resource) {
        send_cursor_position(resource->handle, index, anchor);
    });
}

void TextInputV1InterfacePrivate::setTextDirection(Qt::LayoutDirection direction)
{
    text_direction wlDirection;
    switch (direction) {
    case Qt::LeftToRight:
        wlDirection = text_direction::text_direction_ltr;
        break;
    case Qt::RightToLeft:
        wlDirection = text_direction::text_direction_rtl;
        break;
    case Qt::LayoutDirectionAuto:
        wlDirection = text_direction::text_direction_auto;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    forActivatedResource([this, wlDirection](Resource *resource) {
        send_text_direction(resource->handle, serialHash.value(resource), wlDirection);
    });
}

void TextInputV1InterfacePrivate::setPreEditCursor(qint32 index)
{
    forActivatedResource([this, index](Resource *resource) {
        send_preedit_cursor(resource->handle, index);
    });
}

void TextInputV1InterfacePrivate::sendInputPanelState()
{
    forActivatedResource([this](Resource *resource) {
        send_input_panel_state(resource->handle,
                               inputPanelVisible);
    });
}

void TextInputV1InterfacePrivate::sendLanguage()
{
    forActivatedResource([this](Resource *resource) {
        send_language(resource->handle, serialHash.value(resource), language);
    });
}

void TextInputV1InterfacePrivate::sendModifiersMap()
{
    forActivatedResource([this](Resource *resource) {
        send_modifiers_map(resource->handle, modifiersMap);
    });
}

TextInputV1InterfacePrivate::TextInputV1InterfacePrivate(SeatInterface *seat, TextInputV1Interface *_q)
    : seat(seat)
    , q(_q)
{
}

void TextInputV1InterfacePrivate::zwp_text_input_v1_bind_resource(Resource *resource)
{
    serialHash.insert(resource, 0);
}

void TextInputV1InterfacePrivate::zwp_text_input_v1_destroy_resource(Resource *resource)
{
    serialHash.remove(resource);
    for (auto iter = activated.begin(); iter != activated.end();) {
        if (iter.value() == resource) {
            iter = activated.erase(iter);
        } else {
            ++iter;
        }
    }
}

void TextInputV1InterfacePrivate::zwp_text_input_v1_activate(Resource *resource, struct wl_resource *seat, struct wl_resource *surface)
{
    auto *s = SeatInterface::get(seat);
    if (!s || this->seat != s) {
        wl_resource_post_error(resource->handle, 0, "Invalid seat");
        return;
    }
    EnabledEmitter emitter(q);
    auto enabledSurface = SurfaceInterface::get(surface);
    auto *oldResource = activated.value(enabledSurface);
    if (oldResource == resource) {
        return;
    }
    activated[enabledSurface] = resource;
    if (this->surface == enabledSurface) {
        if (oldResource) {
            send_leave(oldResource->handle);
        }
        send_enter(resource->handle, surface);
    }
    QObject::connect(enabledSurface, &SurfaceInterface::aboutToBeDestroyed, q, [this, enabledSurface] {
        EnabledEmitter emitter(q);
        activated.remove(enabledSurface);
    });
}

void TextInputV1InterfacePrivate::zwp_text_input_v1_deactivate(Resource *resource, wl_resource *seat)
{
    auto s = SeatInterface::get(seat);
    if (!s || this->seat != s) {
        wl_resource_post_error(resource->handle, 0, "Invalid seat");
        return;
    }
    EnabledEmitter emitter(q);

    for (auto iter = activated.begin(); iter != activated.end();) {
        if (iter.value() == resource) {
            if (this->surface && this->surface == iter.key()) {
                send_leave(resource->handle);
            }
            iter = activated.erase(iter);
        } else {
            ++iter;
        }
    }
}

void TextInputV1InterfacePrivate::zwp_text_input_v1_reset(Resource *resource)
{
    Q_EMIT q->reset();
}

void TextInputV1InterfacePrivate::zwp_text_input_v1_commit_state(Resource *resource, uint32_t serial)
{
    serialHash[resource] = serial;
    Q_EMIT q->stateUpdated(serial);
}

void TextInputV1InterfacePrivate::zwp_text_input_v1_invoke_action(Resource *resource, uint32_t button, uint32_t index)
{
    Q_EMIT q->invokeAction(button, index);
}

void TextInputV1InterfacePrivate::zwp_text_input_v1_hide_input_panel(Resource *resource)
{
    Q_EMIT q->requestHideInputPanel();
}

void TextInputV1InterfacePrivate::zwp_text_input_v1_set_surrounding_text(Resource *resource, const QString &text, uint32_t cursor, uint32_t anchor)
{
    surroundingText = text;
    surroundingTextCursorPosition = cursor;
    surroundingTextSelectionAnchor = anchor;
    Q_EMIT q->surroundingTextChanged();
}

void TextInputV1InterfacePrivate::zwp_text_input_v1_set_content_type(Resource *resource, uint32_t hint, uint32_t purpose)
{
    const auto contentHints = convertContentHint(hint);
    const auto contentPurpose = convertContentPurpose(purpose);
    if (this->contentHints != contentHints || this->contentPurpose != contentPurpose) {
        this->contentHints = contentHints;
        this->contentPurpose = contentPurpose;
        Q_EMIT q->contentTypeChanged();
    }
}

void TextInputV1InterfacePrivate::zwp_text_input_v1_set_cursor_rectangle(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    const QRect rect = QRect(x, y, width, height);
    if (cursorRectangle != rect) {
        cursorRectangle = rect;
        Q_EMIT q->cursorRectangleChanged(cursorRectangle);
    }
}

void TextInputV1InterfacePrivate::zwp_text_input_v1_set_preferred_language(Resource *resource, const QString &language)
{
    if (preferredLanguage != language) {
        preferredLanguage = language;
        Q_EMIT q->preferredLanguageChanged(preferredLanguage);
    }
}

void TextInputV1InterfacePrivate::zwp_text_input_v1_show_input_panel(Resource *resource)
{
    Q_EMIT q->requestShowInputPanel();
}

QList<TextInputV1InterfacePrivate::Resource *> TextInputV1InterfacePrivate::textInputsForClient(ClientConnection *client) const
{
    return resourceMap().values(client->client());
}

TextInputV1Interface::TextInputV1Interface(SeatInterface *seat)
    : QObject(seat)
    , d(new TextInputV1InterfacePrivate(seat, this))
{
}

TextInputV1Interface::~TextInputV1Interface()
{
}

QString TextInputV1Interface::preferredLanguage() const
{
    return d->preferredLanguage;
}

TextInputContentHints TextInputV1Interface::contentHints() const
{
    return d->contentHints;
}

TextInputContentPurpose KWaylandServer::TextInputV1Interface::contentPurpose() const
{
    return d->contentPurpose;
}

QString TextInputV1Interface::surroundingText() const
{
    return d->surroundingText;
}

qint32 TextInputV1Interface::surroundingTextCursorPosition() const
{
    return d->surroundingTextCursorPosition;
}

qint32 TextInputV1Interface::surroundingTextSelectionAnchor() const
{
    return d->surroundingTextSelectionAnchor;
}

void TextInputV1Interface::preEdit(const QString &text, const QString &commit)
{
    d->preEdit(text, commit);
}

void TextInputV1Interface::preEditStyling(uint32_t index, uint32_t length, uint32_t style)
{
    d->preEditStyling(index, length, style);
}

void TextInputV1Interface::commitString(const QString &text)
{
    d->commitString(text);
}

void TextInputV1Interface::keysymPressed(quint32 time, quint32 keysym, quint32 modifiers)
{
    d->keysymPressed(time, keysym, modifiers);
}

void TextInputV1Interface::keysymReleased(quint32 time, quint32 keysym, quint32 modifiers)
{
    d->keysymReleased(time, keysym, modifiers);
}

void TextInputV1Interface::deleteSurroundingText(quint32 beforeLength, quint32 afterLength)
{
    d->deleteSurroundingText(beforeLength, afterLength);
}

void TextInputV1Interface::setCursorPosition(qint32 index, qint32 anchor)
{
    d->setCursorPosition(index, anchor);
}

void TextInputV1Interface::setTextDirection(Qt::LayoutDirection direction)
{
    d->setTextDirection(direction);
}

void TextInputV1Interface::setPreEditCursor(qint32 index)
{
    d->setPreEditCursor(index);
}

void TextInputV1Interface::setInputPanelState(bool visible)
{
    if (d->inputPanelVisible == visible) {
        // not changed
        return;
    }
    d->inputPanelVisible = visible;
    d->sendInputPanelState();
}

void TextInputV1Interface::setLanguage(const QString &languageTag)
{
    if (d->language == languageTag) {
        // not changed
        return;
    }
    d->language = languageTag;
    d->sendLanguage();
}

void TextInputV1Interface::setModifiersMap(const QByteArray &modifiersMap)
{
    if (d->modifiersMap == modifiersMap) {
        // not changed
        return;
    }
    d->modifiersMap = modifiersMap;
    d->sendModifiersMap();
}

QPointer<SurfaceInterface> TextInputV1Interface::surface() const
{
    if (!d->resourceMap().contains(d->surface->client()->client())) {
        return nullptr;
    }

    return d->surface;
}

QRect TextInputV1Interface::cursorRectangle() const
{
    return d->cursorRectangle;
}

bool TextInputV1Interface::isEnabled() const
{
    if (!d->surface) {
        return false;
    }
    return d->activated.contains(d->surface);
}

bool TextInputV1Interface::clientSupportsTextInput(ClientConnection *client) const
{
    return client && d->resourceMap().contains(*client);
}
}
