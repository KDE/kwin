/*
    SPDX-FileCopyrightText: 2020 Bhushan Shah <bshah@kde.org>
    SPDX-FileCopyrightText: 2018 Roman Glig <subdiff@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "display.h"
#include "seat.h"
#include "surface_p.h"
#include "textinput_v3_p.h"

namespace KWin
{
namespace
{
const quint32 s_version = 2;

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
    if (hints & QtWaylandServer::zwp_text_input_v3::content_hint_on_screen_input_provided) {
        ret |= TextInputContentHint::OnScreenInputProvided;
    }
    if (hints & QtWaylandServer::zwp_text_input_v3::content_hint_no_emoji) {
        ret |= TextInputContentHint::NoEmoji;
    }
    if (hints & QtWaylandServer::zwp_text_input_v3::content_hint_preedit_shown) {
        ret |= TextInputContentHint::PreeditShown;
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

}

void TextInputV3State::resetPending()
{
    pending.cursorRectangle = RectF();
    pending.surroundingTextChangeCause = TextInputChangeCause::InputMethod;
    pending.contentHints = TextInputContentHints(TextInputContentHint::None);
    pending.contentPurpose = TextInputContentPurpose::Normal;
    pending.enabled = false;
    pending.surroundingText = QString();
    pending.surroundingTextCursorPosition = 0;
    pending.surroundingTextSelectionAnchor = 0;
    pending.availableActions.clear();
    resetPendingPreedit();
}

void TextInputV3State::resetPendingPreedit()
{
    pending.preeditText = QString();
    pending.preeditCursorBegin = 0;
    pending.preeditCursorEnd = 0;
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
    stateMap.insert(resource, TextInputV3State());
    actionSerialHash.insert(resource, 0);
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_destroy_resource(Resource *resource)
{
    // drop resource from the serial hash
    stateMap.remove(resource);
    actionSerialHash.remove(resource);
    updateEnabled();
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void TextInputV3InterfacePrivate::sendEnter(SurfaceInterface *newSurface)
{
    // It should be always synchronized with SeatInterface::focusedTextInputSurface.
    Q_ASSERT(!surface && newSurface);
    surface = newSurface;
    if (surfaceCommitConnection) {
        QObject::disconnect(surfaceCommitConnection);
    }
    // Apply committed cursor rectangle with next surface commit for v2
    surfaceCommitConnection = QObject::connect(newSurface, &SurfaceInterface::committed, q, [this]() {
        if (!surface) {
            return;
        }
        const auto clientResources = textInputsForClient(surface->client());
        for (auto resource : clientResources) {
            auto state = stateForResource(resource);
            if (!state || !state->committedCursorRectangle.has_value()) {
                continue;
            }
            const bool changed = (state->cursorRectangle != *state->committedCursorRectangle);
            state->cursorRectangle = *state->committedCursorRectangle;
            state->committedCursorRectangle.reset();
            if (state->enabled && changed) {
                Q_EMIT q->cursorRectangleChanged(state->cursorRectangle);
            }
        }
    });
    const auto clientResources = textInputsForClient(newSurface->client());
    for (auto resource : clientResources) {
        send_enter(resource->handle, newSurface->resource());
    }
    updateEnabled();
}

void TextInputV3InterfacePrivate::sendLeave(SurfaceInterface *leavingSurface)
{
    // It should be always synchronized with SeatInterface::focusedTextInputSurface.
    Q_ASSERT(leavingSurface && surface == leavingSurface);
    const auto clientResources = textInputsForClient(leavingSurface->client());
    for (auto resource : clientResources) {
        if (auto state = stateForResource(resource)) {
            state->committedCursorRectangle.reset();
        }
    }
    surface.clear();
    if (surfaceCommitConnection) {
        QObject::disconnect(surfaceCommitConnection);
        surfaceCommitConnection = {};
    }
    for (auto resource : clientResources) {
        send_leave(resource->handle, leavingSurface->resource());
    }
    updateEnabled();
}

void TextInputV3InterfacePrivate::sendPreEdit(const QString &text, const quint32 cursorBegin, const quint32 cursorEnd)
{
    if (!surface) {
        return;
    }

    const QList<Resource *> textInputs = enabledTextInputsForClient(surface->client());
    for (auto resource : textInputs) {
        auto state = stateForResource(resource);
        if (!state) {
            continue;
        }

        state->pending.preeditText = text;
        state->pending.preeditCursorBegin = cursorBegin;
        state->pending.preeditCursorEnd = cursorEnd;

        // Special handling for empty preedit, send null instead of empty string.
        // text in preedit_string is defined as allow-null, however qt wayland can't generate a
        // null ptr call from either null QString() or empty QString().
        if (text.isEmpty()) {
            zwp_text_input_v3_send_preedit_string(resource->handle, nullptr, cursorBegin, cursorEnd);
        } else {
            send_preedit_string(resource->handle, text, cursorBegin, cursorEnd);
        }
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

    for (auto resource : textInputs) {
        auto state = stateForResource(resource);
        if (!state) {
            continue;
        }
        state->preeditText = state->pending.preeditText;
        state->preeditCursorBegin = state->pending.preeditCursorBegin;
        state->preeditCursorEnd = state->pending.preeditCursorEnd;
        state->resetPendingPreedit();
        // zwp_text_input_v3.done takes the serial argument which is equal to number of commit requests issued
        send_done(resource->handle, state->serial);
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
        auto state = stateForResource(*it);
        if (state && state->enabled) {
            result.append(*it);
        }
    }
    return result;
}

void TextInputV3InterfacePrivate::updateEnabled()
{
    bool newEnabled = false;
    if (surface) {
        const auto clientResources = textInputsForClient(surface->client());
        newEnabled = std::any_of(clientResources.begin(), clientResources.end(), [this](Resource *resource) {
            auto state = stateForResource(resource);
            return state && state->enabled;
        });
    }

    if (isEnabled != newEnabled) {
        isEnabled = newEnabled;
        Q_EMIT q->enabledChanged();
    }
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_enable(Resource *resource)
{
    auto state = stateForResource(resource);
    if (!state) {
        return;
    }
    // reset pending state to default
    state->resetPending();
    state->pending.enabled = true;
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_disable(Resource *resource)
{
    auto state = stateForResource(resource);
    if (!state) {
        return;
    }
    // reset pending state to default
    state->resetPending();
    state->pending.enabled = false;
    state->preeditText = QString();
    state->preeditCursorBegin = 0;
    state->preeditCursorEnd = 0;
    state->committedCursorRectangle.reset();
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_set_surrounding_text(Resource *resource, const QString &text, int32_t cursor, int32_t anchor)
{
    auto state = stateForResource(resource);
    if (!state) {
        return;
    }
    // zwp_text_input_v3_set_surrounding_text is no-op if enabled request is not pending
    if (!state->pending.enabled) {
        return;
    }
    state->pending.surroundingText = text;
    state->pending.surroundingTextCursorPosition = cursor;
    state->pending.surroundingTextSelectionAnchor = anchor;
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_set_content_type(Resource *resource, uint32_t hint, uint32_t purpose)
{
    auto state = stateForResource(resource);
    if (!state) {
        return;
    }
    // zwp_text_input_v3_set_content_type is no-op if enabled request is not pending
    if (!state->pending.enabled) {
        return;
    }
    state->pending.contentHints = convertContentHint(hint);
    state->pending.contentPurpose = convertContentPurpose(purpose);
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_set_cursor_rectangle(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    auto state = stateForResource(resource);
    if (!state) {
        return;
    }
    if (!surface) {
        return;
    }
    // zwp_text_input_v3_set_cursor_rectangle is no-op if enabled request is not pending
    if (!state->pending.enabled) {
        return;
    }
    state->pending.cursorRectangle = Rect(x, y, width, height).scaled(1.0 / surface->clientToCompositorScale());
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_set_text_change_cause(Resource *resource, uint32_t cause)
{
    auto state = stateForResource(resource);
    if (!state) {
        return;
    }
    // zwp_text_input_v3_set_text_change_cause is no-op if enabled request is not pending
    if (!state->pending.enabled) {
        return;
    }
    state->pending.surroundingTextChangeCause = convertChangeCause(cause);
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_commit(Resource *resource)
{
    auto state = stateForResource(resource);
    if (!state) {
        return;
    }
    state->serial++;

    const auto oldResourceEnabled = state->enabled;
    // state->pending.enabled is not reset.
    state->enabled = state->pending.enabled;

    if (state->surroundingTextChangeCause != state->pending.surroundingTextChangeCause) {
        state->surroundingTextChangeCause = state->pending.surroundingTextChangeCause;
        state->pending.surroundingTextChangeCause = TextInputChangeCause::InputMethod;
    }

    if (state->contentHints != state->pending.contentHints || state->contentPurpose != state->pending.contentPurpose) {
        state->contentHints = state->pending.contentHints;
        state->contentPurpose = state->pending.contentPurpose;
        if (state->enabled) {
            Q_EMIT q->contentTypeChanged();
        }
    }

    if (resource->version() >= 2) {
        state->committedCursorRectangle = state->pending.cursorRectangle;
    } else {
        if (state->cursorRectangle != state->pending.cursorRectangle) {
            state->cursorRectangle = state->pending.cursorRectangle;
            if (state->enabled) {
                Q_EMIT q->cursorRectangleChanged(state->cursorRectangle);
            }
        }
    }

    if (state->surroundingText != state->pending.surroundingText || state->surroundingTextCursorPosition != state->pending.surroundingTextCursorPosition
        || state->surroundingTextSelectionAnchor != state->pending.surroundingTextSelectionAnchor) {
        state->surroundingText = state->pending.surroundingText;
        state->surroundingTextCursorPosition = state->pending.surroundingTextCursorPosition;
        state->surroundingTextSelectionAnchor = state->pending.surroundingTextSelectionAnchor;
        if (state->enabled) {
            Q_EMIT q->surroundingTextChanged();
        }
    }

    if (state->availableActions != state->pending.availableActions) {
        state->availableActions = state->pending.availableActions;
        if (state->enabled) {
            Q_EMIT q->availableActionsChanged();
        }
    }
    Q_EMIT q->stateCommitted(state->serial);

    // Gtk text input implementation expect done to be sent after every commit to synchronize the serial value between commit() and done().
    // So we need to send the current preedit text with done().
    // If current preedit is empty, there is no need to send it.
    if (!state->preeditText.isEmpty() || state->preeditCursorBegin != 0 || state->preeditCursorEnd != 0) {
        send_preedit_string(resource->handle, state->preeditText, state->preeditCursorBegin, state->preeditCursorEnd);
    }
    send_done(resource->handle, state->serial);

    if (state->enabled && oldResourceEnabled) {
        Q_EMIT q->enableRequested();
    }

    updateEnabled();
}

TextInputV3Interface::TextInputV3Interface(SeatInterface *seat)
    : QObject(seat)
    , d(new TextInputV3InterfacePrivate(seat, this))
{
}

TextInputV3Interface::~TextInputV3Interface() = default;

TextInputContentHints TextInputV3Interface::contentHints() const
{
    if (auto state = d->stateForFocusedSurface()) {
        return state->contentHints;
    }
    return TextInputContentHint::None;
}

TextInputContentPurpose TextInputV3Interface::contentPurpose() const
{
    if (auto state = d->stateForFocusedSurface()) {
        return state->contentPurpose;
    }
    return TextInputContentPurpose::Normal;
}

QString TextInputV3Interface::surroundingText() const
{
    if (auto state = d->stateForFocusedSurface()) {
        return state->surroundingText;
    }
    return {};
}

qint32 TextInputV3Interface::surroundingTextCursorPosition() const
{
    if (auto state = d->stateForFocusedSurface()) {
        return state->surroundingTextCursorPosition;
    }
    return 0;
}

qint32 TextInputV3Interface::surroundingTextSelectionAnchor() const
{
    if (auto state = d->stateForFocusedSurface()) {
        return state->surroundingTextSelectionAnchor;
    }
    return 0;
}

void TextInputV3Interface::deleteSurroundingText(quint32 beforeLength, quint32 afterLength)
{
    d->deleteSurroundingText(beforeLength, afterLength);
}

void TextInputV3Interface::sendPreEditString(const QString &text, quint32 cursorBegin, quint32 cursorEnd)
{
    d->sendPreEdit(text, cursorBegin, cursorEnd);
}

void TextInputV3Interface::sendPreEditHint(quint32 start, quint32 end, PreeditHint hint)
{
    d->sendPreeditHint(start, end, hint);
}

void TextInputV3Interface::commitString(const QString &text)
{
    d->commitString(text);
}

void TextInputV3Interface::done()
{
    d->done();
}

void TextInputV3Interface::setLanguage(const QString &languageTag)
{
    d->sendLanguage(languageTag);
}

void TextInputV3Interface::performAction(Action action)
{
    d->sendAction(action);
}

QSet<TextInputV3Interface::Action> TextInputV3Interface::availableActions() const
{
    if (auto state = d->stateForFocusedSurface()) {
        return state->availableActions;
    }
    return {};
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

RectF TextInputV3Interface::cursorRectangle() const
{
    if (auto state = d->stateForFocusedSurface()) {
        return state->cursorRectangle;
    }
    return {};
}

bool TextInputV3Interface::isEnabled() const
{
    return d->isEnabled;
}

bool TextInputV3Interface::clientSupportsTextInput(ClientConnection *client) const
{
    return client && d->resourceMap().contains(*client);
}

bool TextInputV3Interface::implicitShowPanelRequired() const
{
    if (!d->surface) {
        return false;
    }
    const auto clientResources = d->enabledTextInputsForClient(d->surface->client());
    for (auto resource : clientResources) {
        if (resource->version() >= 2) {
            return false;
        }
    }
    return true;
}

void TextInputV3InterfacePrivate::sendPreeditHint(uint32_t start, uint32_t end, TextInputV3Interface::PreeditHint hint)
{
    if (!surface) {
        return;
    }
    const QList<Resource *> textInputs = enabledTextInputsForClient(surface->client());
    for (auto resource : textInputs) {
        if (resource->version() >= 2) {
            send_preedit_hint(resource->handle, start, end, static_cast<uint32_t>(hint));
        }
    }
}

void TextInputV3InterfacePrivate::sendLanguage(const QString &language)
{
    if (!surface) {
        return;
    }
    const QList<Resource *> textInputs = enabledTextInputsForClient(surface->client());
    for (auto resource : textInputs) {
        if (resource->version() >= 2) {
            send_language(resource->handle, language);
        }
    }
}

void TextInputV3InterfacePrivate::sendAction(TextInputV3Interface::Action action)
{
    if (!surface) {
        return;
    }
    const QList<Resource *> textInputs = enabledTextInputsForClient(surface->client());
    for (auto resource : textInputs) {
        if (resource->version() >= 2) {
            quint32 &serial = actionSerialHash[resource];
            serial++;
            send_action(resource->handle, static_cast<uint32_t>(action), serial);
        }
    }
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_set_available_actions(Resource *resource, wl_array *available_actions)
{
    auto state = stateForResource(resource);
    if (!state) {
        return;
    }
    // zwp_text_input_v3_set_available_actions is no-op if enabled request is not pending
    if (!state->pending.enabled) {
        return;
    }
    const auto *data = static_cast<uint32_t *>(available_actions->data);
    const size_t count = available_actions->size / sizeof(uint32_t);
    state->pending.availableActions.clear();
    for (size_t i = 0; i < count; ++i) {
        state->pending.availableActions.insert(static_cast<TextInputV3Interface::Action>(data[i]));
    }
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_show_input_panel(Resource *resource)
{
    Q_EMIT q->requestShowInputPanel();
}

void TextInputV3InterfacePrivate::zwp_text_input_v3_hide_input_panel(Resource *resource)
{
    Q_EMIT q->requestHideInputPanel();
}
}

#include "moc_textinput_v3.cpp"
