/*
    SPDX-FileCopyrightText: 2020 Bhushan Shah <bshah@kde.org>
    SPDX-FileCopyrightText: 2018 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "textinput_v3_interface_p.h"
#include "display.h"
#include "seat_interface.h"
#include "surface_interface_p.h"

namespace KWaylandServer
{

static const quint32 s_version = 1;

static TextInputContentHints convertContentHint(uint32_t hint)
{
    const auto hints = zwp_text_input_v3_content_hint(hint);
    TextInputContentHints ret = TextInputContentHint::None;

    if (hints & QtWaylandServer::zwp_text_input_v3::content_hint_completion) {
        ret |= TextInputContentHint::AutoCompletion;
    }
    if (hints & QtWaylandServer::zwp_text_input_v3::content_hint_auto_capitalization) {
        ret |= TextInputContentHint::AutoCapitalization;
    }
    if (hints & QtWaylandServer::zwp_text_input_v3::content_hint_lowercase) {
        ret |= TextInputContentHint::LowerCase;
    }
    if (hints & QtWaylandServer::zwp_text_input_v3::content_hint_uppercase) {
        ret |= TextInputContentHint::UpperCase;
    }
    if (hints & QtWaylandServer::zwp_text_input_v3::content_hint_titlecase) {
        ret |= TextInputContentHint::TitleCase;
    }
    if (hints & QtWaylandServer::zwp_text_input_v3::content_hint_hidden_text) {
        ret |= TextInputContentHint::HiddenText;
    }
    if (hints & QtWaylandServer::zwp_text_input_v3::content_hint_sensitive_data) {
        ret |= TextInputContentHint::SensitiveData;
    }
    if (hints & QtWaylandServer::zwp_text_input_v3::content_hint_latin) {
        ret |= TextInputContentHint::Latin;
    }
    if (hints & QtWaylandServer::zwp_text_input_v3::content_hint_multiline) {
        ret |= TextInputContentHint::MultiLine;
    }
    if (hints & QtWaylandServer::zwp_text_input_v3::content_hint_spellcheck) {
        ret |= TextInputContentHint::AutoCorrection;
    }
    return ret;
}

static TextInputContentPurpose convertContentPurpose(uint32_t purpose)
{
    const auto wlPurpose = QtWaylandServer::zwp_text_input_v3::content_purpose(purpose);

    switch (wlPurpose) {
    case QtWaylandServer::zwp_text_input_v3::content_purpose_alpha:
        return TextInputContentPurpose::Alpha;
    case QtWaylandServer::zwp_text_input_v3::content_purpose_digits:
        return TextInputContentPurpose::Digits;
    case QtWaylandServer::zwp_text_input_v3::content_purpose_number:
        return TextInputContentPurpose::Number;
    case QtWaylandServer::zwp_text_input_v3::content_purpose_phone:
        return TextInputContentPurpose::Phone;
    case QtWaylandServer::zwp_text_input_v3::content_purpose_url:
        return TextInputContentPurpose::Url;
    case QtWaylandServer::zwp_text_input_v3::content_purpose_email:
        return TextInputContentPurpose::Email;
    case QtWaylandServer::zwp_text_input_v3::content_purpose_name:
        return TextInputContentPurpose::Name;
    case QtWaylandServer::zwp_text_input_v3::content_purpose_password:
        return TextInputContentPurpose::Password;
    case QtWaylandServer::zwp_text_input_v3::content_purpose_pin:
        return TextInputContentPurpose::Pin;
    case QtWaylandServer::zwp_text_input_v3::content_purpose_date:
        return TextInputContentPurpose::Date;
    case QtWaylandServer::zwp_text_input_v3::content_purpose_time:
        return TextInputContentPurpose::Time;
    case QtWaylandServer::zwp_text_input_v3::content_purpose_datetime:
        return TextInputContentPurpose::DateTime;
    case QtWaylandServer::zwp_text_input_v3::content_purpose_terminal:
        return TextInputContentPurpose::Terminal;
    case QtWaylandServer::zwp_text_input_v3::content_purpose_normal:
        return TextInputContentPurpose::Normal;
    default:
        return TextInputContentPurpose::Normal;
    }
}

static TextInputChangeCause convertChangeCause(uint32_t cause)
{
    const auto wlCause = QtWaylandServer::zwp_text_input_v3::change_cause(cause);
    switch (wlCause) {
    case QtWaylandServer::zwp_text_input_v3::change_cause_input_method:
        return TextInputChangeCause::InputMethod;
    case QtWaylandServer::zwp_text_input_v3::change_cause_other:
    default:
        return TextInputChangeCause::Other;
    }
}

TextInputManagerV3InterfacePrivate::TextInputManagerV3InterfacePrivate(TextInputManagerV3Interface *_q, Display *display)
    : QtWaylandServer::zwp_text_input_manager_v3(*display, s_version)
    , q(_q)
{
}

void TextInputManagerV3InterfacePrivate::zwp_text_input_manager_v3_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void TextInputManagerV3InterfacePrivate::zwp_text_input_manager_v3_get_text_input(Resource *resource, uint32_t id, wl_resource *seat)
{
    SeatInterface *s = SeatInterface::get(seat);
    if (!s) {
        wl_resource_post_error(resource->handle, 0, "Invalid seat");
        return;
    }
    TextInputV3InterfacePrivate *textInputPrivate = TextInputV3InterfacePrivate::get(s->textInputV3());
    textInputPrivate->add(resource->client(), id, resource->version());
}

TextInputManagerV3Interface::TextInputManagerV3Interface(Display* display, QObject* parent)
    : QObject(parent)
    , d(new TextInputManagerV3InterfacePrivate(this, display))
{
}

TextInputManagerV3Interface::~TextInputManagerV3Interface() = default;

TextInputV3InterfacePrivate::TextInputV3InterfacePrivate(SeatInterface *seat, TextInputV3Interface *_q)
    : seat(seat)
    , q(_q)
{
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_bind_resource(Resource *resource)
{
    // we initialize the serial for the resource to be 0
    serialHash.insert(resource, 0);
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_destroy(Resource *resource)
{
    // drop resource from the serial hash
    serialHash.remove(resource);
}

void TextInputV3InterfacePrivate::sendEnter(SurfaceInterface *s)
{
    if (!s) {
        return;
    }
    surface = QPointer<SurfaceInterface>(s);
    const auto clientResources = textInputsForClient(s->client());
    for (auto resource : clientResources) {
        send_enter(resource->handle, s->resource());
    }
}

void TextInputV3InterfacePrivate::sendLeave(SurfaceInterface *s)
{
    if (!s) {
        return;
    }
    surface.clear();
    const auto clientResources = textInputsForClient(s->client());
    for (auto resource : clientResources) {
        send_leave(resource->handle, s->resource());
    }
}

void TextInputV3InterfacePrivate::sendPreEdit(const QString &text, const quint32 cursorBegin, const quint32 cursorEnd)
{
    if (!surface) {
        return;
    }
    const QList<Resource *> textInputs = textInputsForClient(surface->client());
    for (auto resource : textInputs) {
        send_preedit_string(resource->handle, text, cursorBegin, cursorEnd);
    }
}

void TextInputV3InterfacePrivate::commitString(const QString &text)
{
    if (!surface) {
        return;
    }
    const QList<Resource *> textInputs = textInputsForClient(surface->client());
    for (auto resource : textInputs) {
        send_commit_string(resource->handle, text);
    }
}

void TextInputV3InterfacePrivate::deleteSurroundingText(quint32 before, quint32 after)
{
    if (!surface) {
        return;
    }
    const QList<Resource *> textInputs = textInputsForClient(surface->client());
    for (auto resource : textInputs) {
        send_delete_surrounding_text(resource->handle, before, after);
    }
}

void TextInputV3InterfacePrivate::done()
{
    if (!surface) {
        return;
    }
    const QList<Resource *> textInputs = textInputsForClient(surface->client());
    for (auto resource : textInputs) {
        // zwp_text_input_v3.done takes the serial argument which is equal to number of commit requests issued
        send_done(resource->handle, serialHash[resource]);
    }
}

QList<TextInputV3InterfacePrivate::Resource *> TextInputV3InterfacePrivate::textInputsForClient(ClientConnection *client) const
{
    return resourceMap().values(client->client());
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_enable(Resource *resource)
{
    // reset pending state to default
    Q_UNUSED(resource)
    defaultPending();
    pending.enabled = true;
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_disable(Resource *resource)
{
    // reset pending state to default
    Q_UNUSED(resource)
    defaultPending();
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_set_surrounding_text(Resource *resource, const QString &text, int32_t cursor, int32_t anchor)
{
    Q_UNUSED(resource)
    // zwp_text_input_v3_set_surrounding_text is no-op if enabled request is not pending
    if (!pending.enabled) {
        return;
    }
    pending.surroundingText = text;
    pending.surroundingTextCursorPosition = cursor;
    pending.surroundingTextSelectionAnchor = anchor;
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_set_content_type(Resource *resource, uint32_t hint, uint32_t purpose)
{
    Q_UNUSED(resource)
    // zwp_text_input_v3_set_content_type is no-op if enabled request is not pending
    if (!pending.enabled) {
        return;
    }
    pending.contentHints = convertContentHint(hint);
    pending.contentPurpose = convertContentPurpose(purpose);
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_set_cursor_rectangle(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    Q_UNUSED(resource)
    // zwp_text_input_v3_set_cursor_rectangle is no-op if enabled request is not pending
    if (!pending.enabled) {
        return;
    }
    pending.cursorRectangle = QRect(x, y, width, height);
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_set_text_change_cause(Resource *resource, uint32_t cause)
{
    Q_UNUSED(resource)
    // zwp_text_input_v3_set_text_change_cause is no-op if enabled request is not pending
    if (!pending.enabled) {
        return;
    }
    pending.surroundingTextChangeCause = convertChangeCause(cause);
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_commit(Resource *resource)
{
    serialHash[resource]++;

    if (enabled != pending.enabled) {
        enabled = pending.enabled;
        emit q->enabledChanged();
    }

    if (surroundingTextChangeCause != pending.surroundingTextChangeCause) {
        surroundingTextChangeCause = pending.surroundingTextChangeCause;
        pending.surroundingTextChangeCause = TextInputChangeCause::InputMethod;
    }

    if (contentHints != pending.contentHints || contentPurpose != pending.contentPurpose) {
        contentHints = pending.contentHints;
        contentPurpose = pending.contentPurpose;
        if (enabled) {
            emit q->contentTypeChanged();
        }
    }

    if (cursorRectangle != pending.cursorRectangle) {
        cursorRectangle = pending.cursorRectangle;
        if (enabled) {
            emit q->cursorRectangleChanged(cursorRectangle);
        }
    }

    if (surroundingText != pending.surroundingText || surroundingTextCursorPosition != pending.surroundingTextCursorPosition || surroundingTextSelectionAnchor != pending.surroundingTextSelectionAnchor) {
        surroundingText = pending.surroundingText;
        surroundingTextCursorPosition = pending.surroundingTextCursorPosition;
        surroundingTextSelectionAnchor = pending.surroundingTextSelectionAnchor;
        if (enabled) {
            emit q->surroundingTextChanged();
        }
    }

    emit q->stateCommitted(serialHash[resource]);
}

void TextInputV3InterfacePrivate::defaultPending()
{
    pending.cursorRectangle = QRect();
    pending.surroundingTextChangeCause = TextInputChangeCause::InputMethod;
    pending.contentHints = TextInputContentHints(TextInputContentHint::None);
    pending.contentPurpose = TextInputContentPurpose::Normal;
    pending.enabled = false;
    pending.surroundingText = QString();
    pending.surroundingTextCursorPosition = 0;
    pending.surroundingTextSelectionAnchor = 0;
}

TextInputV3Interface::TextInputV3Interface(SeatInterface *seat)
    : QObject()
    , d(new TextInputV3InterfacePrivate(seat, this))
{
}

TextInputV3Interface::~TextInputV3Interface() = default;

TextInputContentHints TextInputV3Interface::contentHints() const
{
    return d->contentHints;
}

TextInputContentPurpose TextInputV3Interface::contentPurpose() const
{
    return d->contentPurpose;
}

QString TextInputV3Interface::surroundingText() const
{
    return d->surroundingText;
}

qint32 TextInputV3Interface::surroundingTextCursorPosition() const
{
    return d->surroundingTextCursorPosition;
}

qint32 TextInputV3Interface::surroundingTextSelectionAnchor() const
{
    return d->surroundingTextSelectionAnchor;
}

void TextInputV3Interface::deleteSurroundingText(quint32 beforeLength, quint32 afterLength)
{
    d->deleteSurroundingText(beforeLength, afterLength);
}

void TextInputV3Interface::sendPreEditString(const QString &text, quint32 cursorBegin, quint32 cursorEnd)
{
    d->sendPreEdit(text, cursorBegin, cursorEnd);
}

void TextInputV3Interface::commitString(const QString &text)
{
    d->commitString(text);
}

void TextInputV3Interface::done()
{
    d->done();
}

QPointer<SurfaceInterface> TextInputV3Interface::surface() const
{
    return d->surface;
}

QRect TextInputV3Interface::cursorRectangle() const
{
    return d->cursorRectangle;
}

bool TextInputV3Interface::isEnabled() const
{
    return d->enabled;
}

}
