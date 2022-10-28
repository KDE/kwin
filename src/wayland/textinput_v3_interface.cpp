/*
    SPDX-FileCopyrightText: 2020 Bhushan Shah <bshah@kde.org>
    SPDX-FileCopyrightText: 2018 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "display.h"
#include "seat_interface.h"
#include "surface_interface_p.h"
#include "textinput_v3_interface_p.h"

namespace KWaylandServer
{
namespace
{
const quint32 s_version = 1;

TextInputContentHints convertContentHint(uint32_t hint)
{
    const auto hints = QtWaylandServer::zwp_text_input_v3::content_hint(hint);
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

TextInputContentPurpose convertContentPurpose(uint32_t purpose)
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

TextInputChangeCause convertChangeCause(uint32_t cause)
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

class EnabledEmitter
{
public:
    EnabledEmitter(TextInputV3Interface *q)
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
    TextInputV3Interface *q;
    const bool m_wasEnabled;
};

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
    auto *textInputResource = textInputPrivate->add(resource->client(), id, resource->version());
    // Send enter to this new text input object if the surface is already focused.
    if (textInputPrivate->surface && textInputPrivate->surface->client()->client() == resource->client()) {
        textInputPrivate->send_enter(textInputResource->handle, textInputPrivate->surface->resource());
    }
}

TextInputManagerV3Interface::TextInputManagerV3Interface(Display *display, QObject *parent)
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
    enabled.insert(resource, false);
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_destroy(Resource *resource)
{
    // drop resource from the serial hash
    serialHash.remove(resource);
    enabled.remove(resource);
}

void TextInputV3InterfacePrivate::sendEnter(SurfaceInterface *newSurface)
{
    EnabledEmitter emitter(q);
    // It should be always synchronized with SeatInterface::focusedTextInputSurface.
    Q_ASSERT(!surface && newSurface);
    surface = newSurface;
    const auto clientResources = textInputsForClient(newSurface->client());
    for (auto resource : clientResources) {
        send_enter(resource->handle, newSurface->resource());
    }
}

void TextInputV3InterfacePrivate::sendLeave(SurfaceInterface *leavingSurface)
{
    EnabledEmitter emitter(q);
    // It should be always synchronized with SeatInterface::focusedTextInputSurface.
    Q_ASSERT(leavingSurface && surface == leavingSurface);
    surface.clear();
    const auto clientResources = textInputsForClient(leavingSurface->client());
    for (auto resource : clientResources) {
        send_leave(resource->handle, leavingSurface->resource());
    }
}

void TextInputV3InterfacePrivate::sendPreEdit(const QString &text, const quint32 cursorBegin, const quint32 cursorEnd)
{
    if (!surface) {
        return;
    }

    pending.preeditText = text;
    pending.preeditCursorBegin = cursorBegin;
    pending.preeditCursorEnd = cursorEnd;

    const QList<Resource *> textInputs = enabledTextInputsForClient(surface->client());
    for (auto resource : textInputs) {
        send_preedit_string(resource->handle, text, cursorBegin, cursorEnd);
    }
}

void TextInputV3InterfacePrivate::commitString(const QString &text)
{
    if (!surface) {
        return;
    }
    const QList<Resource *> textInputs = enabledTextInputsForClient(surface->client());
    for (auto resource : textInputs) {
        send_commit_string(resource->handle, text);
    }
}

void TextInputV3InterfacePrivate::deleteSurroundingText(quint32 before, quint32 after)
{
    if (!surface) {
        return;
    }
    const QList<Resource *> textInputs = enabledTextInputsForClient(surface->client());
    for (auto resource : textInputs) {
        send_delete_surrounding_text(resource->handle, before, after);
    }
}

void TextInputV3InterfacePrivate::done()
{
    if (!surface) {
        return;
    }
    const QList<Resource *> textInputs = enabledTextInputsForClient(surface->client());

    preeditText = pending.preeditText;
    preeditCursorBegin = pending.preeditCursorBegin;
    preeditCursorEnd = pending.preeditCursorEnd;
    defaultPendingPreedit();

    for (auto resource : textInputs) {
        // zwp_text_input_v3.done takes the serial argument which is equal to number of commit requests issued
        send_done(resource->handle, serialHash[resource]);
    }
}

QList<TextInputV3InterfacePrivate::Resource *> TextInputV3InterfacePrivate::textInputsForClient(ClientConnection *client) const
{
    return resourceMap().values(client->client());
}

QList<TextInputV3InterfacePrivate::Resource *> TextInputV3InterfacePrivate::enabledTextInputsForClient(ClientConnection *client) const
{
    QList<TextInputV3InterfacePrivate::Resource *> result;
    const auto [start, end] = resourceMap().equal_range(client->client());
    for (auto it = start; it != end; ++it) {
        if (enabled[*it]) {
            result.append(*it);
        }
    }
    return result;
}

bool TextInputV3InterfacePrivate::isEnabled() const
{
    if (!surface) {
        return false;
    }
    const auto clientResources = textInputsForClient(surface->client());
    return std::any_of(clientResources.begin(), clientResources.end(), [this](Resource *resource) {
        return enabled[resource];
    });
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_enable(Resource *resource)
{
    // reset pending state to default
    defaultPending();
    pending.enabled = true;
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_disable(Resource *resource)
{
    // reset pending state to default
    defaultPending();
    preeditText = QString();
    preeditCursorBegin = 0;
    preeditCursorEnd = 0;
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_set_surrounding_text(Resource *resource, const QString &text, int32_t cursor, int32_t anchor)
{
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
    // zwp_text_input_v3_set_content_type is no-op if enabled request is not pending
    if (!pending.enabled) {
        return;
    }
    pending.contentHints = convertContentHint(hint);
    pending.contentPurpose = convertContentPurpose(purpose);
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_set_cursor_rectangle(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    // zwp_text_input_v3_set_cursor_rectangle is no-op if enabled request is not pending
    if (!pending.enabled) {
        return;
    }
    pending.cursorRectangle = QRect(x, y, width, height);
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_set_text_change_cause(Resource *resource, uint32_t cause)
{
    // zwp_text_input_v3_set_text_change_cause is no-op if enabled request is not pending
    if (!pending.enabled) {
        return;
    }
    pending.surroundingTextChangeCause = convertChangeCause(cause);
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_commit(Resource *resource)
{
    EnabledEmitter emitter(q);
    serialHash[resource]++;

    auto &resourceEnabled = enabled[resource];
    const auto oldResourceEnabled = resourceEnabled;
    if (resourceEnabled != pending.enabled) {
        resourceEnabled = pending.enabled;
    }

    if (surroundingTextChangeCause != pending.surroundingTextChangeCause) {
        surroundingTextChangeCause = pending.surroundingTextChangeCause;
        pending.surroundingTextChangeCause = TextInputChangeCause::InputMethod;
    }

    if (contentHints != pending.contentHints || contentPurpose != pending.contentPurpose) {
        contentHints = pending.contentHints;
        contentPurpose = pending.contentPurpose;
        if (resourceEnabled) {
            Q_EMIT q->contentTypeChanged();
        }
    }

    if (cursorRectangle != pending.cursorRectangle) {
        cursorRectangle = pending.cursorRectangle;
        if (resourceEnabled) {
            Q_EMIT q->cursorRectangleChanged(cursorRectangle);
        }
    }

    if (surroundingText != pending.surroundingText || surroundingTextCursorPosition != pending.surroundingTextCursorPosition
        || surroundingTextSelectionAnchor != pending.surroundingTextSelectionAnchor) {
        surroundingText = pending.surroundingText;
        surroundingTextCursorPosition = pending.surroundingTextCursorPosition;
        surroundingTextSelectionAnchor = pending.surroundingTextSelectionAnchor;
        if (resourceEnabled) {
            Q_EMIT q->surroundingTextChanged();
        }
    }

    Q_EMIT q->stateCommitted(serialHash[resource]);

    // Gtk text input implementation expect done to be sent after every commit to synchronize the serial value between commit() and done().
    // So we need to send the current preedit text with done().
    // If current preedit is empty, there is no need to send it.
    if (!preeditText.isEmpty() || preeditCursorBegin != 0 || preeditCursorEnd != 0) {
        send_preedit_string(resource->handle, preeditText, preeditCursorBegin, preeditCursorEnd);
    }
    send_done(resource->handle, serialHash[resource]);

    if (resourceEnabled && oldResourceEnabled) {
        Q_EMIT q->enableRequested();
    }
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
    defaultPendingPreedit();
}

void TextInputV3InterfacePrivate::defaultPendingPreedit()
{
    pending.preeditText = QString();
    pending.preeditCursorBegin = 0;
    pending.preeditCursorEnd = 0;
}

TextInputV3Interface::TextInputV3Interface(SeatInterface *seat)
    : QObject(seat)
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
    if (!d->surface) {
        return nullptr;
    }

    if (!d->resourceMap().contains(d->surface->client()->client())) {
        return nullptr;
    }

    return d->surface;
}

QRect TextInputV3Interface::cursorRectangle() const
{
    return d->cursorRectangle;
}

bool TextInputV3Interface::isEnabled() const
{
    return d->isEnabled();
}

bool TextInputV3Interface::clientSupportsTextInput(ClientConnection *client) const
{
    return client && d->resourceMap().contains(*client);
}
}
