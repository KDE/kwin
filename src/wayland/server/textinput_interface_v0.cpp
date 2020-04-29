/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "textinput_interface_p.h"
#include "display.h"
#include "resource_p.h"
#include "seat_interface_p.h"
#include "surface_interface.h"

#include <wayland-text-server-protocol.h>

namespace KWaylandServer
{

class TextInputUnstableV0Interface::Private : public TextInputInterface::Private
{
public:
    Private(TextInputInterface *q, TextInputManagerUnstableV0Interface *c, wl_resource *parentResource);
    ~Private();

    void activate(SeatInterface *seat, SurfaceInterface *surface);
    void deactivate();

    void sendEnter(SurfaceInterface *surface, quint32 serial) override;
    void sendLeave(quint32 serial, SurfaceInterface *surface) override;
    void preEdit(const QByteArray &text, const QByteArray &commit) override;
    void commit(const QByteArray &text) override;
    void deleteSurroundingText(quint32 beforeLength, quint32 afterLength) override;
    void setTextDirection(Qt::LayoutDirection direction) override;
    void setPreEditCursor(qint32 index) override;
    void setCursorPosition(qint32 index, qint32 anchor) override;
    void keysymPressed(quint32 keysym, Qt::KeyboardModifiers modifiers) override;
    void keysymReleased(quint32 keysym, Qt::KeyboardModifiers modifiers) override;
    TextInputInterfaceVersion interfaceVersion() const override {
        return TextInputInterfaceVersion::UnstableV0;
    }
    void sendInputPanelState() override;
    void sendLanguage() override;

private:
    static const struct wl_text_input_interface s_interface;
    TextInputUnstableV0Interface *q_func() {
        return reinterpret_cast<TextInputUnstableV0Interface *>(q);
    }

    static void activateCallback(wl_client *client, wl_resource *resource, wl_resource * seat, wl_resource * surface);
    static void deactivateCallback(wl_client *client, wl_resource *resource, wl_resource * seat);
    static void resetCallback(wl_client *client, wl_resource *resource);
    static void setSurroundingTextUintCallback(wl_client *client, wl_resource *resource, const char * text, uint32_t cursor, uint32_t anchor);
    static void commitStateCallback(wl_client *client, wl_resource *resource, uint32_t serial);
    static void invokeActionCallback(wl_client *client, wl_resource *resource, uint32_t button, uint32_t index);

    // helpers
    TextInputInterface::ContentHints convertContentHint(uint32_t hint) const override;
    TextInputInterface::ContentPurpose convertContentPurpose(uint32_t purpose) const override;

    quint32 latestState = 0;
};

#ifndef K_DOXYGEN
const struct wl_text_input_interface TextInputUnstableV0Interface::Private::s_interface = {
    activateCallback,
    deactivateCallback,
    showInputPanelCallback,
    hideInputPanelCallback,
    resetCallback,
    setSurroundingTextUintCallback,
    setContentTypeCallback,
    setCursorRectangleCallback,
    setPreferredLanguageCallback,
    commitStateCallback,
    invokeActionCallback
};
#endif

void TextInputUnstableV0Interface::Private::activate(SeatInterface *seat, SurfaceInterface *s)
{
    surface = QPointer<SurfaceInterface>(s);
    enabled = true;
    emit q_func()->enabledChanged();
    emit q_func()->requestActivate(seat, surface);
}

void TextInputUnstableV0Interface::Private::deactivate()
{
    surface.clear();
    enabled = false;
    emit q_func()->enabledChanged();
}

void TextInputUnstableV0Interface::Private::sendEnter(SurfaceInterface *surface, quint32 serial)
{
    Q_UNUSED(serial)
    if (!resource) {
        return;
    }
    wl_text_input_send_enter(resource, surface->resource());
}

void TextInputUnstableV0Interface::Private::sendLeave(quint32 serial, SurfaceInterface *surface)
{
    Q_UNUSED(serial)
    Q_UNUSED(surface)
    if (!resource) {
        return;
    }
    wl_text_input_send_leave(resource);
}

void TextInputUnstableV0Interface::Private::preEdit(const QByteArray &text, const QByteArray &commit)
{
    if (!resource) {
        return;
    }
    wl_text_input_send_preedit_string(resource, latestState, text.constData(), commit.constData());
}

void TextInputUnstableV0Interface::Private::commit(const QByteArray &text)
{
    if (!resource) {
        return;
    }
    wl_text_input_send_commit_string(resource, latestState, text.constData());
}

void TextInputUnstableV0Interface::Private::keysymPressed(quint32 keysym, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    if (!resource) {
        return;
    }
    wl_text_input_send_keysym(resource, latestState, seat ? seat->timestamp() : 0, keysym, WL_KEYBOARD_KEY_STATE_PRESSED, 0);
}

void TextInputUnstableV0Interface::Private::keysymReleased(quint32 keysym, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    if (!resource) {
        return;
    }
    wl_text_input_send_keysym(resource, latestState, seat ? seat->timestamp() : 0, keysym, WL_KEYBOARD_KEY_STATE_RELEASED, 0);
}

void TextInputUnstableV0Interface::Private::deleteSurroundingText(quint32 beforeLength, quint32 afterLength)
{
    if (!resource) {
        return;
    }
    wl_text_input_send_delete_surrounding_text(resource, -1 * beforeLength, beforeLength + afterLength);
}

void TextInputUnstableV0Interface::Private::setCursorPosition(qint32 index, qint32 anchor)
{
    if (!resource) {
        return;
    }
    wl_text_input_send_cursor_position(resource, index, anchor);
}

void TextInputUnstableV0Interface::Private::setTextDirection(Qt::LayoutDirection direction)
{
    if (!resource) {
        return;
    }
    wl_text_input_text_direction wlDirection;
    switch (direction) {
    case Qt::LeftToRight:
        wlDirection = WL_TEXT_INPUT_TEXT_DIRECTION_LTR;
        break;
    case Qt::RightToLeft:
        wlDirection = WL_TEXT_INPUT_TEXT_DIRECTION_RTL;
        break;
    case Qt::LayoutDirectionAuto:
        wlDirection = WL_TEXT_INPUT_TEXT_DIRECTION_AUTO;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    wl_text_input_send_text_direction(resource, latestState, wlDirection);
}

void TextInputUnstableV0Interface::Private::setPreEditCursor(qint32 index)
{
    if (!resource) {
        return;
    }
    wl_text_input_send_preedit_cursor(resource, index);
}

void TextInputUnstableV0Interface::Private::sendInputPanelState()
{
    if (!resource) {
        return;
    }
    wl_text_input_send_input_panel_state(resource, inputPanelVisible);
}

void TextInputUnstableV0Interface::Private::sendLanguage()
{
    if (!resource) {
        return;
    }
    wl_text_input_send_language(resource, latestState, language.constData());
}

TextInputUnstableV0Interface::Private::Private(TextInputInterface *q, TextInputManagerUnstableV0Interface *c, wl_resource *parentResource)
    : TextInputInterface::Private(q, c, parentResource, &wl_text_input_interface, &s_interface)
{
}

TextInputUnstableV0Interface::Private::~Private() = default;

void TextInputUnstableV0Interface::Private::activateCallback(wl_client *client, wl_resource *resource, wl_resource *seat, wl_resource *surface)
{
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
    p->activate(SeatInterface::get(seat), SurfaceInterface::get(surface));
}

void TextInputUnstableV0Interface::Private::deactivateCallback(wl_client *client, wl_resource *resource, wl_resource *seat)
{
    Q_UNUSED(seat)
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
    p->deactivate();
}

void TextInputUnstableV0Interface::Private::resetCallback(wl_client *client, wl_resource *resource)
{
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
    emit p->q_func()->requestReset();
}

void TextInputUnstableV0Interface::Private::setSurroundingTextUintCallback(wl_client *client, wl_resource *resource, const char * text, uint32_t cursor, uint32_t anchor)
{
    setSurroundingTextCallback(client, resource, text, cursor, anchor);
}

void TextInputUnstableV0Interface::Private::commitStateCallback(wl_client *client, wl_resource *resource, uint32_t serial)
{
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
    p->latestState = serial;
}

void TextInputUnstableV0Interface::Private::invokeActionCallback(wl_client *client, wl_resource *resource, uint32_t button, uint32_t index)
{
    Q_UNUSED(button)
    Q_UNUSED(index)
    // TODO: implement
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
}

TextInputInterface::ContentHints TextInputUnstableV0Interface::Private::convertContentHint(uint32_t hint) const
{
    const auto hints = wl_text_input_content_hint(hint);
    TextInputInterface::ContentHints ret = TextInputInterface::ContentHint::None;

    if (hints & WL_TEXT_INPUT_CONTENT_HINT_AUTO_COMPLETION) {
        ret |= TextInputInterface::ContentHint::AutoCompletion;
    }
    if (hints & WL_TEXT_INPUT_CONTENT_HINT_AUTO_CORRECTION) {
        ret |= TextInputInterface::ContentHint::AutoCorrection;
    }
    if (hints & WL_TEXT_INPUT_CONTENT_HINT_AUTO_CAPITALIZATION) {
        ret |= TextInputInterface::ContentHint::AutoCapitalization;
    }
    if (hints & WL_TEXT_INPUT_CONTENT_HINT_LOWERCASE) {
        ret |= TextInputInterface::ContentHint::LowerCase;
    }
    if (hints & WL_TEXT_INPUT_CONTENT_HINT_UPPERCASE) {
        ret |= TextInputInterface::ContentHint::UpperCase;
    }
    if (hints & WL_TEXT_INPUT_CONTENT_HINT_TITLECASE) {
        ret |= TextInputInterface::ContentHint::TitleCase;
    }
    if (hints & WL_TEXT_INPUT_CONTENT_HINT_HIDDEN_TEXT) {
        ret |= TextInputInterface::ContentHint::HiddenText;
    }
    if (hints & WL_TEXT_INPUT_CONTENT_HINT_SENSITIVE_DATA) {
        ret |= TextInputInterface::ContentHint::SensitiveData;
    }
    if (hints & WL_TEXT_INPUT_CONTENT_HINT_LATIN) {
        ret |= TextInputInterface::ContentHint::Latin;
    }
    if (hints & WL_TEXT_INPUT_CONTENT_HINT_MULTILINE) {
        ret |= TextInputInterface::ContentHint::MultiLine;
    }
    return ret;
}

TextInputInterface::ContentPurpose TextInputUnstableV0Interface::Private::convertContentPurpose(uint32_t purpose) const
{
    const auto wlPurpose = wl_text_input_content_purpose(purpose);

    switch (wlPurpose) {
    case WL_TEXT_INPUT_CONTENT_PURPOSE_ALPHA:
        return TextInputInterface::ContentPurpose::Alpha;
    case WL_TEXT_INPUT_CONTENT_PURPOSE_DIGITS:
        return TextInputInterface::ContentPurpose::Digits;
    case WL_TEXT_INPUT_CONTENT_PURPOSE_NUMBER:
        return TextInputInterface::ContentPurpose::Number;
    case WL_TEXT_INPUT_CONTENT_PURPOSE_PHONE:
        return TextInputInterface::ContentPurpose::Phone;
    case WL_TEXT_INPUT_CONTENT_PURPOSE_URL:
        return TextInputInterface::ContentPurpose::Url;
    case WL_TEXT_INPUT_CONTENT_PURPOSE_EMAIL:
        return TextInputInterface::ContentPurpose::Email;
    case WL_TEXT_INPUT_CONTENT_PURPOSE_NAME:
        return TextInputInterface::ContentPurpose::Name;
    case WL_TEXT_INPUT_CONTENT_PURPOSE_PASSWORD:
        return TextInputInterface::ContentPurpose::Password;
    case WL_TEXT_INPUT_CONTENT_PURPOSE_DATE:
        return TextInputInterface::ContentPurpose::Date;
    case WL_TEXT_INPUT_CONTENT_PURPOSE_TIME:
        return TextInputInterface::ContentPurpose::Time;
    case WL_TEXT_INPUT_CONTENT_PURPOSE_DATETIME:
        return TextInputInterface::ContentPurpose::DateTime;
    case WL_TEXT_INPUT_CONTENT_PURPOSE_TERMINAL:
        return TextInputInterface::ContentPurpose::Terminal;
    case WL_TEXT_INPUT_CONTENT_PURPOSE_NORMAL:
    default:
        return TextInputInterface::ContentPurpose::Normal;
    }
}

TextInputUnstableV0Interface::TextInputUnstableV0Interface(TextInputManagerUnstableV0Interface *parent, wl_resource *parentResource)
    : TextInputInterface(new Private(this, parent, parentResource))
{
}

TextInputUnstableV0Interface::~TextInputUnstableV0Interface() = default;

class TextInputManagerUnstableV0Interface::Private : public TextInputManagerInterface::Private
{
public:
    Private(TextInputManagerUnstableV0Interface *q, Display *d);

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    static void createTextInputCallback(wl_client *client, wl_resource *resource, uint32_t id);

    TextInputManagerUnstableV0Interface *q;
    static const struct wl_text_input_manager_interface s_interface;
    static const quint32 s_version;
};
const quint32 TextInputManagerUnstableV0Interface::Private::s_version = 1;

#ifndef K_DOXYGEN
const struct wl_text_input_manager_interface TextInputManagerUnstableV0Interface::Private::s_interface = {
    createTextInputCallback
};
#endif

void TextInputManagerUnstableV0Interface::Private::createTextInputCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    auto m = cast(resource);
    auto *t = new TextInputUnstableV0Interface(m->q, resource);
    m->inputs << t;
    QObject::connect(t, &QObject::destroyed, m->q,
        [t, m] {
            m->inputs.removeAll(t);
        }
    );
    QObject::connect(t, &TextInputUnstableV0Interface::requestActivate, m->q,
        [t] (SeatInterface *seat) {
            // TODO: disallow for other seat
            seat->d_func()->registerTextInput(t);
            t->d_func()->seat = seat;
        }
    );
    t->d->create(m->display->getConnection(client), version, id);
}

TextInputManagerInterface::Private::Private(TextInputInterfaceVersion interfaceVersion, TextInputManagerInterface *q, Display *d, const wl_interface *interface, quint32 version)
    : Global::Private(d, interface, version)
    , interfaceVersion(interfaceVersion)
    , q(q)
{
}

TextInputManagerUnstableV0Interface::Private::Private(TextInputManagerUnstableV0Interface *q, Display *d)
    : TextInputManagerInterface::Private(TextInputInterfaceVersion::UnstableV0, q, d, &wl_text_input_manager_interface, s_version)
    , q(q)
{
}

void TextInputManagerUnstableV0Interface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&wl_text_input_manager_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    // TODO: should we track?
}

void TextInputManagerUnstableV0Interface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
    // TODO: implement?
}

TextInputManagerUnstableV0Interface::TextInputManagerUnstableV0Interface(Display *display, QObject *parent)
    : TextInputManagerInterface(new Private(this, display), parent)
{
}

TextInputManagerUnstableV0Interface::~TextInputManagerUnstableV0Interface() = default;

}
