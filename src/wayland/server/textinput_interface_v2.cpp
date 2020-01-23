/****************************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
****************************************************************************/
#include "textinput_interface_p.h"
#include "display.h"
#include "resource_p.h"
#include "seat_interface_p.h"
#include "surface_interface.h"

#include <wayland-text-input-unstable-v2-server-protocol.h>

namespace KWayland
{
namespace Server
{

class TextInputUnstableV2Interface::Private : public TextInputInterface::Private
{
public:
    Private(TextInputInterface *q, TextInputManagerUnstableV2Interface *c, wl_resource *parentResource);
    ~Private();

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
        return TextInputInterfaceVersion::UnstableV2;
    }
    void sendInputPanelState() override;
    void sendLanguage() override;

private:
    static const struct zwp_text_input_v2_interface s_interface;
    TextInputUnstableV2Interface *q_func() {
        return reinterpret_cast<TextInputUnstableV2Interface *>(q);
    }

    static void enableCallback(wl_client *client, wl_resource *resource, wl_resource * surface);
    static void disableCallback(wl_client *client, wl_resource *resource, wl_resource * surface);
    static void updateStateCallback(wl_client *client, wl_resource *resource, uint32_t serial, uint32_t reason);

    // helpers
    TextInputInterface::ContentHints convertContentHint(uint32_t hint) const override;
    TextInputInterface::ContentPurpose convertContentPurpose(uint32_t purpose) const override;

    void enable(SurfaceInterface *s);
    void disable();
};

#ifndef K_DOXYGEN
const struct zwp_text_input_v2_interface TextInputUnstableV2Interface::Private::s_interface = {
    resourceDestroyedCallback,
    enableCallback,
    disableCallback,
    showInputPanelCallback,
    hideInputPanelCallback,
    setSurroundingTextCallback,
    setContentTypeCallback,
    setCursorRectangleCallback,
    setPreferredLanguageCallback,
    updateStateCallback
};
#endif

void TextInputUnstableV2Interface::Private::enable(SurfaceInterface *s)
{
    surface = QPointer<SurfaceInterface>(s);
    enabled = true;
    emit q_func()->enabledChanged();
}

void TextInputUnstableV2Interface::Private::disable()
{
    surface.clear();
    enabled = false;
    emit q_func()->enabledChanged();
}

void TextInputUnstableV2Interface::Private::sendEnter(SurfaceInterface *surface, quint32 serial)
{
    if (!resource || !surface || !surface->resource()) {
        return;
    }
    zwp_text_input_v2_send_enter(resource, serial, surface->resource());
}

void TextInputUnstableV2Interface::Private::sendLeave(quint32 serial, SurfaceInterface *surface)
{
    if (!resource || !surface || !surface->resource()) {
        return;
    }
    zwp_text_input_v2_send_leave(resource, serial, surface->resource());
}

void TextInputUnstableV2Interface::Private::preEdit(const QByteArray &text, const QByteArray &commit)
{
    if (!resource) {
        return;
    }
    zwp_text_input_v2_send_preedit_string(resource, text.constData(), commit.constData());
}

void TextInputUnstableV2Interface::Private::commit(const QByteArray &text)
{
    if (!resource) {
        return;
    }
    zwp_text_input_v2_send_commit_string(resource, text.constData());
}

void TextInputUnstableV2Interface::Private::keysymPressed(quint32 keysym, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    if (!resource) {
        return;
    }
    zwp_text_input_v2_send_keysym(resource, seat ? seat->timestamp() : 0, keysym, WL_KEYBOARD_KEY_STATE_PRESSED, 0);
}

void TextInputUnstableV2Interface::Private::keysymReleased(quint32 keysym, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    if (!resource) {
        return;
    }
    zwp_text_input_v2_send_keysym(resource, seat ? seat->timestamp() : 0, keysym, WL_KEYBOARD_KEY_STATE_RELEASED, 0);
}

void TextInputUnstableV2Interface::Private::deleteSurroundingText(quint32 beforeLength, quint32 afterLength)
{
    if (!resource) {
        return;
    }
    zwp_text_input_v2_send_delete_surrounding_text(resource, beforeLength, afterLength);
}

void TextInputUnstableV2Interface::Private::setCursorPosition(qint32 index, qint32 anchor)
{
    if (!resource) {
        return;
    }
    zwp_text_input_v2_send_cursor_position(resource, index, anchor);
}

void TextInputUnstableV2Interface::Private::setTextDirection(Qt::LayoutDirection direction)
{
    if (!resource) {
        return;
    }
    zwp_text_input_v2_text_direction wlDirection;
    switch (direction) {
    case Qt::LeftToRight:
        wlDirection = ZWP_TEXT_INPUT_V2_TEXT_DIRECTION_LTR;
        break;
    case Qt::RightToLeft:
        wlDirection = ZWP_TEXT_INPUT_V2_TEXT_DIRECTION_RTL;
        break;
    case Qt::LayoutDirectionAuto:
        wlDirection = ZWP_TEXT_INPUT_V2_TEXT_DIRECTION_AUTO;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    zwp_text_input_v2_send_text_direction(resource, wlDirection);
}

void TextInputUnstableV2Interface::Private::setPreEditCursor(qint32 index)
{
    if (!resource) {
        return;
    }
    zwp_text_input_v2_send_preedit_cursor(resource, index);
}

void TextInputUnstableV2Interface::Private::sendInputPanelState()
{
    if (!resource) {
        return;
    }
    zwp_text_input_v2_send_input_panel_state(resource,
                                             inputPanelVisible ? ZWP_TEXT_INPUT_V2_INPUT_PANEL_VISIBILITY_VISIBLE : ZWP_TEXT_INPUT_V2_INPUT_PANEL_VISIBILITY_HIDDEN,
                                             overlappedSurfaceArea.x(), overlappedSurfaceArea.y(), overlappedSurfaceArea.width(), overlappedSurfaceArea.height());
}

void TextInputUnstableV2Interface::Private::sendLanguage()
{
    if (!resource) {
        return;
    }
    zwp_text_input_v2_send_language(resource, language.constData());
}

TextInputUnstableV2Interface::Private::Private(TextInputInterface *q, TextInputManagerUnstableV2Interface *c, wl_resource *parentResource)
    : TextInputInterface::Private(q, c, parentResource, &zwp_text_input_v2_interface, &s_interface)
{
}

TextInputUnstableV2Interface::Private::~Private() = default;

void TextInputUnstableV2Interface::Private::enableCallback(wl_client *client, wl_resource *resource, wl_resource *surface)
{
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
    p->enable(SurfaceInterface::get(surface));
}

void TextInputUnstableV2Interface::Private::disableCallback(wl_client *client, wl_resource *resource, wl_resource *surface)
{
    Q_UNUSED(surface)
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
    p->disable();
}

void TextInputUnstableV2Interface::Private::updateStateCallback(wl_client *client, wl_resource *resource, uint32_t serial, uint32_t reason)
{
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
    Q_UNUSED(serial)
    // TODO: use other reason values reason
    if (reason == ZWP_TEXT_INPUT_V2_UPDATE_STATE_RESET) {
        emit p->q_func()->requestReset();
    }
}

TextInputInterface::ContentHints TextInputUnstableV2Interface::Private::convertContentHint(uint32_t hint) const
{
    const auto hints = zwp_text_input_v2_content_hint(hint);
    TextInputInterface::ContentHints ret = TextInputInterface::ContentHint::None;

    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_AUTO_COMPLETION) {
        ret |= TextInputInterface::ContentHint::AutoCompletion;
    }
    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_AUTO_CORRECTION) {
        ret |= TextInputInterface::ContentHint::AutoCorrection;
    }
    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_AUTO_CAPITALIZATION) {
        ret |= TextInputInterface::ContentHint::AutoCapitalization;
    }
    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_LOWERCASE) {
        ret |= TextInputInterface::ContentHint::LowerCase;
    }
    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_UPPERCASE) {
        ret |= TextInputInterface::ContentHint::UpperCase;
    }
    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_TITLECASE) {
        ret |= TextInputInterface::ContentHint::TitleCase;
    }
    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_HIDDEN_TEXT) {
        ret |= TextInputInterface::ContentHint::HiddenText;
    }
    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_SENSITIVE_DATA) {
        ret |= TextInputInterface::ContentHint::SensitiveData;
    }
    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_LATIN) {
        ret |= TextInputInterface::ContentHint::Latin;
    }
    if (hints & ZWP_TEXT_INPUT_V2_CONTENT_HINT_MULTILINE) {
        ret |= TextInputInterface::ContentHint::MultiLine;
    }
    return ret;
}

TextInputInterface::ContentPurpose TextInputUnstableV2Interface::Private::convertContentPurpose(uint32_t purpose) const
{
    const auto wlPurpose = zwp_text_input_v2_content_purpose(purpose);

    switch (wlPurpose) {
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_ALPHA:
        return TextInputInterface::ContentPurpose::Alpha;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_DIGITS:
        return TextInputInterface::ContentPurpose::Digits;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_NUMBER:
        return TextInputInterface::ContentPurpose::Number;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_PHONE:
        return TextInputInterface::ContentPurpose::Phone;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_URL:
        return TextInputInterface::ContentPurpose::Url;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_EMAIL:
        return TextInputInterface::ContentPurpose::Email;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_NAME:
        return TextInputInterface::ContentPurpose::Name;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_PASSWORD:
        return TextInputInterface::ContentPurpose::Password;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_DATE:
        return TextInputInterface::ContentPurpose::Date;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_TIME:
        return TextInputInterface::ContentPurpose::Time;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_DATETIME:
        return TextInputInterface::ContentPurpose::DateTime;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_TERMINAL:
        return TextInputInterface::ContentPurpose::Terminal;
    case ZWP_TEXT_INPUT_V2_CONTENT_PURPOSE_NORMAL:
    default:
        return TextInputInterface::ContentPurpose::Normal;
    }
}

TextInputUnstableV2Interface::TextInputUnstableV2Interface(TextInputManagerUnstableV2Interface *parent, wl_resource *parentResource)
    : TextInputInterface(new Private(this, parent, parentResource))
{
}

TextInputUnstableV2Interface::~TextInputUnstableV2Interface() = default;

class TextInputManagerUnstableV2Interface::Private : public TextInputManagerInterface::Private
{
public:
    Private(TextInputManagerUnstableV2Interface *q, Display *d);

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void getTextInputCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * seat);

    TextInputManagerUnstableV2Interface *q;
    static const struct zwp_text_input_manager_v2_interface s_interface;
    static const quint32 s_version;
};
const quint32 TextInputManagerUnstableV2Interface::Private::s_version = 1;

#ifndef K_DOXYGEN
const struct zwp_text_input_manager_v2_interface TextInputManagerUnstableV2Interface::Private::s_interface = {
    destroyCallback,
    getTextInputCallback
};
#endif

void TextInputManagerUnstableV2Interface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    wl_resource_destroy(resource);
}

void TextInputManagerUnstableV2Interface::Private::getTextInputCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * seat)
{
    SeatInterface *s = SeatInterface::get(seat);
    if (!s) {
        // TODO: send error
        return;
    }
    auto m = cast(resource);
    auto *t = new TextInputUnstableV2Interface(m->q, resource);
    t->d_func()->seat = s;
    m->inputs << t;
    QObject::connect(t, &QObject::destroyed, m->q,
        [t, m] {
            m->inputs.removeAll(t);
        }
    );
    t->d->create(m->display->getConnection(client), version, id);
    s->d_func()->registerTextInput(t);
}

TextInputManagerUnstableV2Interface::Private::Private(TextInputManagerUnstableV2Interface *q, Display *d)
    : TextInputManagerInterface::Private(TextInputInterfaceVersion::UnstableV2, q, d, &zwp_text_input_manager_v2_interface, s_version)
    , q(q)
{
}

void TextInputManagerUnstableV2Interface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&zwp_text_input_manager_v2_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    // TODO: should we track?
}

void TextInputManagerUnstableV2Interface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
    // TODO: implement?
}

TextInputManagerUnstableV2Interface::TextInputManagerUnstableV2Interface(Display *display, QObject *parent)
    : TextInputManagerInterface(new Private(this, display), parent)
{
}

TextInputManagerUnstableV2Interface::~TextInputManagerUnstableV2Interface() = default;

}
}
