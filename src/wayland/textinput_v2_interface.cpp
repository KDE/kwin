/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "display.h"
#include "seat_interface_p.h"
#include "surface_interface_p.h"
#include "textinput_v2_interface_p.h"

namespace KWaylandServer
{

namespace
{
const quint32 s_version = 1;

// helpers
TextInputContentHints convertContentHint(uint32_t hint)
{
    const auto hints = zwp_text_input_v2_content_hint(hint);
    TextInputContentHints ret = TextInputContentHint::None;

    if (hints & QtWaylandServer::zwp_text_input_v2::content_hint_auto_completion) {
        ret |= TextInputContentHint::AutoCompletion;
    }
    if (hints & QtWaylandServer::zwp_text_input_v2::content_hint_auto_correction) {
        ret |= TextInputContentHint::AutoCorrection;
    }
    if (hints & QtWaylandServer::zwp_text_input_v2::content_hint_auto_capitalization) {
        ret |= TextInputContentHint::AutoCapitalization;
    }
    if (hints & QtWaylandServer::zwp_text_input_v2::content_hint_lowercase) {
        ret |= TextInputContentHint::LowerCase;
    }
    if (hints & QtWaylandServer::zwp_text_input_v2::content_hint_uppercase) {
        ret |= TextInputContentHint::UpperCase;
    }
    if (hints & QtWaylandServer::zwp_text_input_v2::content_hint_titlecase) {
        ret |= TextInputContentHint::TitleCase;
    }
    if (hints & QtWaylandServer::zwp_text_input_v2::content_hint_hidden_text) {
        ret |= TextInputContentHint::HiddenText;
    }
    if (hints & QtWaylandServer::zwp_text_input_v2::content_hint_sensitive_data) {
        ret |= TextInputContentHint::SensitiveData;
    }
    if (hints & QtWaylandServer::zwp_text_input_v2::content_hint_latin) {
        ret |= TextInputContentHint::Latin;
    }
    if (hints & QtWaylandServer::zwp_text_input_v2::content_hint_multiline) {
        ret |= TextInputContentHint::MultiLine;
    }
    return ret;
}

TextInputContentPurpose convertContentPurpose(uint32_t purpose)
{
    const auto wlPurpose = QtWaylandServer::zwp_text_input_v2::content_purpose(purpose);

    switch (wlPurpose) {
    case QtWaylandServer::zwp_text_input_v2::content_purpose_alpha:
        return TextInputContentPurpose::Alpha;
    case QtWaylandServer::zwp_text_input_v2::content_purpose_digits:
        return TextInputContentPurpose::Digits;
    case QtWaylandServer::zwp_text_input_v2::content_purpose_number:
        return TextInputContentPurpose::Number;
    case QtWaylandServer::zwp_text_input_v2::content_purpose_phone:
        return TextInputContentPurpose::Phone;
    case QtWaylandServer::zwp_text_input_v2::content_purpose_url:
        return TextInputContentPurpose::Url;
    case QtWaylandServer::zwp_text_input_v2::content_purpose_email:
        return TextInputContentPurpose::Email;
    case QtWaylandServer::zwp_text_input_v2::content_purpose_name:
        return TextInputContentPurpose::Name;
    case QtWaylandServer::zwp_text_input_v2::content_purpose_password:
        return TextInputContentPurpose::Password;
    case QtWaylandServer::zwp_text_input_v2::content_purpose_date:
        return TextInputContentPurpose::Date;
    case QtWaylandServer::zwp_text_input_v2::content_purpose_time:
        return TextInputContentPurpose::Time;
    case QtWaylandServer::zwp_text_input_v2::content_purpose_datetime:
        return TextInputContentPurpose::DateTime;
    case QtWaylandServer::zwp_text_input_v2::content_purpose_terminal:
        return TextInputContentPurpose::Terminal;
    case QtWaylandServer::zwp_text_input_v2::content_purpose_normal:
        return TextInputContentPurpose::Normal;
    default:
        return TextInputContentPurpose::Normal;
    }
}

class EnabledEmitter
{
public:
    EnabledEmitter(TextInputV2Interface *q)
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
    TextInputV2Interface *q;
    const bool m_wasEnabled;
};

}

TextInputManagerV2InterfacePrivate::TextInputManagerV2InterfacePrivate(TextInputManagerV2Interface *_q, Display *display)
    : QtWaylandServer::zwp_text_input_manager_v2(*display, s_version)
    , q(_q)
{
}

void TextInputManagerV2InterfacePrivate::zwp_text_input_manager_v2_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void TextInputManagerV2InterfacePrivate::zwp_text_input_manager_v2_get_text_input(Resource *resource, uint32_t id, wl_resource *seat)
{
    SeatInterface *s = SeatInterface::get(seat);
    if (!s) {
        wl_resource_post_error(resource->handle, 0, "Invalid  seat");
        return;
    }

    TextInputV2InterfacePrivate *textInputPrivate = TextInputV2InterfacePrivate::get(s->textInputV2());
    auto *textInputResource = textInputPrivate->add(resource->client(), id, resource->version());
    // Send enter to this new text input object if the surface is already focused.
    const quint32 serial = s->display()->nextSerial();
    if (textInputPrivate->surface && textInputPrivate->surface->client()->client() == resource->client()) {
        textInputPrivate->send_enter(textInputResource->handle, serial, textInputPrivate->surface->resource());
    }
}

TextInputManagerV2Interface::TextInputManagerV2Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new TextInputManagerV2InterfacePrivate(this, display))
{
}

TextInputManagerV2Interface::~TextInputManagerV2Interface() = default;

void TextInputV2InterfacePrivate::sendEnter(SurfaceInterface *newSurface, quint32 serial)
{
    EnabledEmitter emitter(q);
    // It should be always synchronized with SeatInterface::focusedTextInputSurface.
    Q_ASSERT(!surface && newSurface);
    surface = newSurface;
    const auto clientResources = textInputsForClient(newSurface->client());
    for (auto resource : clientResources) {
        send_enter(resource->handle, serial, newSurface->resource());
    }
}

void TextInputV2InterfacePrivate::sendLeave(quint32 serial, SurfaceInterface *leavingSurface)
{
    // It should be always synchronized with SeatInterface::focusedTextInputSurface.
    Q_ASSERT(leavingSurface && surface == leavingSurface);
    EnabledEmitter emitter(q);
    surface.clear();
    const auto clientResources = textInputsForClient(leavingSurface->client());
    for (auto resource : clientResources) {
        send_leave(resource->handle, serial, leavingSurface->resource());
    }
}

void TextInputV2InterfacePrivate::preEdit(const QString &text, const QString &commit)
{
    if (!surface) {
        return;
    }

    const auto clientResources = textInputsForClient(surface->client());
    for (auto resource : clientResources) {
        send_preedit_string(resource->handle, text, commit);
    }
}

void TextInputV2InterfacePrivate::preEditStyling(uint32_t index, uint32_t length, uint32_t style)
{
    if (!surface) {
        return;
    }

    const auto clientResources = textInputsForClient(surface->client());
    for (auto resource : clientResources) {
        send_preedit_styling(resource->handle, index, length, style);
    }
}

void TextInputV2InterfacePrivate::commitString(const QString &text)
{
    if (!surface) {
        return;
    }
    const QList<Resource *> textInputs = textInputsForClient(surface->client());
    for (auto resource : textInputs) {
        send_commit_string(resource->handle, text);
    }
}

void TextInputV2InterfacePrivate::keysymPressed(quint32 keysym, quint32 modifiers)
{
    if (!surface) {
        return;
    }

    const QList<Resource *> textInputs = textInputsForClient(surface->client());
    for (auto resource : textInputs) {
        send_keysym(resource->handle, seat ? seat->timestamp().count() : 0, keysym, WL_KEYBOARD_KEY_STATE_PRESSED, modifiers);
    }
}

void TextInputV2InterfacePrivate::keysymReleased(quint32 keysym, quint32 modifiers)
{
    if (!surface) {
        return;
    }

    const QList<Resource *> textInputs = textInputsForClient(surface->client());
    for (auto resource : textInputs) {
        send_keysym(resource->handle, seat ? seat->timestamp().count() : 0, keysym, WL_KEYBOARD_KEY_STATE_RELEASED, modifiers);
    }
}

void TextInputV2InterfacePrivate::deleteSurroundingText(quint32 beforeLength, quint32 afterLength)
{
    if (!surface) {
        return;
    }
    const QList<Resource *> textInputs = textInputsForClient(surface->client());
    for (auto resource : textInputs) {
        send_delete_surrounding_text(resource->handle, beforeLength, afterLength);
    }
}

void TextInputV2InterfacePrivate::setCursorPosition(qint32 index, qint32 anchor)
{
    if (!surface) {
        return;
    }
    const QList<Resource *> textInputs = textInputsForClient(surface->client());
    for (auto resource : textInputs) {
        send_cursor_position(resource->handle, index, anchor);
    }
}

void TextInputV2InterfacePrivate::setTextDirection(Qt::LayoutDirection direction)
{
    if (!surface) {
        return;
    }
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
    const QList<Resource *> textInputs = textInputsForClient(surface->client());
    for (auto resource : textInputs) {
        send_text_direction(resource->handle, wlDirection);
    }
}

void TextInputV2InterfacePrivate::setPreEditCursor(qint32 index)
{
    if (!surface) {
        return;
    }
    const QList<Resource *> textInputs = textInputsForClient(surface->client());
    for (auto resource : textInputs) {
        send_preedit_cursor(resource->handle, index);
    }
}

void TextInputV2InterfacePrivate::sendInputPanelState()
{
    if (!surface) {
        return;
    }
    const QList<Resource *> textInputs = textInputsForClient(surface->client());
    for (auto resource : textInputs) {
        send_input_panel_state(resource->handle,
                               inputPanelVisible ? ZWP_TEXT_INPUT_V2_INPUT_PANEL_VISIBILITY_VISIBLE : ZWP_TEXT_INPUT_V2_INPUT_PANEL_VISIBILITY_HIDDEN,
                               overlappedSurfaceArea.x(),
                               overlappedSurfaceArea.y(),
                               overlappedSurfaceArea.width(),
                               overlappedSurfaceArea.height());
    }
}

void TextInputV2InterfacePrivate::sendLanguage()
{
    if (!surface) {
        return;
    }
    const QList<Resource *> textInputs = textInputsForClient(surface->client());
    for (auto resource : textInputs) {
        send_language(resource->handle, language);
    }
}

void TextInputV2InterfacePrivate::sendModifiersMap()
{
    if (!surface) {
        return;
    }
    const QList<Resource *> textInputs = textInputsForClient(surface->client());
    for (auto resource : textInputs) {
        send_modifiers_map(resource->handle, modifiersMap);
    }
}

TextInputV2InterfacePrivate::TextInputV2InterfacePrivate(SeatInterface *seat, TextInputV2Interface *_q)
    : seat(seat)
    , q(_q)
{
}

void TextInputV2InterfacePrivate::zwp_text_input_v2_enable(Resource *resource, wl_resource *s)
{
    EnabledEmitter emitter(q);
    auto enabledSurface = SurfaceInterface::get(s);
    if (m_enabledSurfaces.contains(enabledSurface)) {
        return;
    }
    m_enabledSurfaces.insert(enabledSurface);
    QObject::connect(enabledSurface, &SurfaceInterface::aboutToBeDestroyed, q, [this, enabledSurface] {
        EnabledEmitter emitter(q);
        m_enabledSurfaces.remove(enabledSurface);
    });
}

void TextInputV2InterfacePrivate::zwp_text_input_v2_disable(Resource *resource, wl_resource *s)
{
    EnabledEmitter emitter(q);
    auto disabledSurface = SurfaceInterface::get(s);
    QObject::disconnect(disabledSurface, &SurfaceInterface::aboutToBeDestroyed, q, nullptr);
    m_enabledSurfaces.remove(disabledSurface);
    if (disabledSurface == surface) {
        q->setInputPanelState(false, {0, 0, 0, 0});
    }
}

void TextInputV2InterfacePrivate::zwp_text_input_v2_update_state(Resource *resource, uint32_t serial, uint32_t reason)
{
    Q_EMIT q->stateUpdated(serial, TextInputV2Interface::UpdateReason(reason));
}

void TextInputV2InterfacePrivate::zwp_text_input_v2_hide_input_panel(Resource *resource)
{
    Q_EMIT q->requestHideInputPanel();
}

void TextInputV2InterfacePrivate::zwp_text_input_v2_set_surrounding_text(Resource *resource, const QString &text, int32_t cursor, int32_t anchor)
{
    surroundingText = text;
    surroundingTextCursorPosition = cursor;
    surroundingTextSelectionAnchor = anchor;
    Q_EMIT q->surroundingTextChanged();
}

void TextInputV2InterfacePrivate::zwp_text_input_v2_set_content_type(Resource *resource, uint32_t hint, uint32_t purpose)
{
    const auto contentHints = convertContentHint(hint);
    const auto contentPurpose = convertContentPurpose(purpose);
    if (this->contentHints != contentHints || this->contentPurpose != contentPurpose) {
        this->contentHints = contentHints;
        this->contentPurpose = contentPurpose;
        Q_EMIT q->contentTypeChanged();
    }
}

void TextInputV2InterfacePrivate::zwp_text_input_v2_set_cursor_rectangle(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    const QRect rect = QRect(x, y, width, height);
    if (cursorRectangle != rect) {
        cursorRectangle = rect;
        Q_EMIT q->cursorRectangleChanged(cursorRectangle);
    }
}

void TextInputV2InterfacePrivate::zwp_text_input_v2_set_preferred_language(Resource *resource, const QString &language)
{
    if (preferredLanguage != language) {
        preferredLanguage = language;
        Q_EMIT q->preferredLanguageChanged(preferredLanguage);
    }
}

void TextInputV2InterfacePrivate::zwp_text_input_v2_show_input_panel(Resource *resource)
{
    Q_EMIT q->requestShowInputPanel();
}

QList<TextInputV2InterfacePrivate::Resource *> TextInputV2InterfacePrivate::textInputsForClient(ClientConnection *client) const
{
    return resourceMap().values(client->client());
}

TextInputV2Interface::TextInputV2Interface(SeatInterface *seat)
    : QObject(seat)
    , d(new TextInputV2InterfacePrivate(seat, this))
{
}

TextInputV2Interface::~TextInputV2Interface() = default;

QString TextInputV2Interface::preferredLanguage() const
{
    return d->preferredLanguage;
}

TextInputContentHints TextInputV2Interface::contentHints() const
{
    return d->contentHints;
}

TextInputContentPurpose KWaylandServer::TextInputV2Interface::contentPurpose() const
{
    return d->contentPurpose;
}

QString TextInputV2Interface::surroundingText() const
{
    return d->surroundingText;
}

qint32 TextInputV2Interface::surroundingTextCursorPosition() const
{
    return d->surroundingTextCursorPosition;
}

qint32 TextInputV2Interface::surroundingTextSelectionAnchor() const
{
    return d->surroundingTextSelectionAnchor;
}

void TextInputV2Interface::preEdit(const QString &text, const QString &commit)
{
    d->preEdit(text, commit);
}

void TextInputV2Interface::preEditStyling(uint32_t index, uint32_t length, uint32_t style)
{
    d->preEditStyling(index, length, style);
}

void TextInputV2Interface::commitString(const QString &text)
{
    d->commitString(text);
}

void TextInputV2Interface::keysymPressed(quint32 keysym, quint32 modifiers)
{
    d->keysymPressed(keysym, modifiers);
}

void TextInputV2Interface::keysymReleased(quint32 keysym, quint32 modifiers)
{
    d->keysymReleased(keysym, modifiers);
}

void TextInputV2Interface::deleteSurroundingText(quint32 beforeLength, quint32 afterLength)
{
    d->deleteSurroundingText(beforeLength, afterLength);
}

void TextInputV2Interface::setCursorPosition(qint32 index, qint32 anchor)
{
    d->setCursorPosition(index, anchor);
}

void TextInputV2Interface::setTextDirection(Qt::LayoutDirection direction)
{
    d->setTextDirection(direction);
}

void TextInputV2Interface::setPreEditCursor(qint32 index)
{
    d->setPreEditCursor(index);
}

void TextInputV2Interface::setInputPanelState(bool visible, const QRect &overlappedSurfaceArea)
{
    if (d->inputPanelVisible == visible && d->overlappedSurfaceArea == overlappedSurfaceArea) {
        // not changed
        return;
    }
    d->inputPanelVisible = visible;
    d->overlappedSurfaceArea = overlappedSurfaceArea;
    d->sendInputPanelState();
}

void TextInputV2Interface::setLanguage(const QString &languageTag)
{
    if (d->language == languageTag) {
        // not changed
        return;
    }
    d->language = languageTag;
    d->sendLanguage();
}

void TextInputV2Interface::setModifiersMap(const QByteArray &modifiersMap)
{
    if (d->modifiersMap == modifiersMap) {
        // not changed
        return;
    }
    d->modifiersMap = modifiersMap;
    d->sendModifiersMap();
}

QPointer<SurfaceInterface> TextInputV2Interface::surface() const
{
    if (!d->surface) {
        return nullptr;
    }

    if (!d->resourceMap().contains(d->surface->client()->client())) {
        return nullptr;
    }

    return d->surface;
}

QRect TextInputV2Interface::cursorRectangle() const
{
    return d->cursorRectangle;
}

bool TextInputV2Interface::isEnabled() const
{
    return d->surface && d->m_enabledSurfaces.contains(d->surface);
}

bool TextInputV2Interface::clientSupportsTextInput(ClientConnection *client) const
{
    return client && d->resourceMap().contains(*client);
}
}
