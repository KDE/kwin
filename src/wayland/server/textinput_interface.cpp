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
#include "global_p.h"
#include "resource_p.h"
#include "seat_interface_p.h"
#include "surface_interface.h"

#include <QVector>

#include <wayland-text-server-protocol.h>
#include <wayland-text-input-unstable-v2-server-protocol.h>

namespace KWayland
{
namespace Server
{

void TextInputInterface::Private::activateCallback(wl_client *client, wl_resource *resource, wl_resource *seat, wl_resource *surface)
{
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
    p->requestActivate(SeatInterface::get(seat), SurfaceInterface::get(surface));
}

void TextInputInterface::Private::deactivateCallback(wl_client *client, wl_resource *resource, wl_resource *seat)
{
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
    p->requestDeactivate(SeatInterface::get(seat));
}

void TextInputInterface::Private::enableCallback(wl_client *client, wl_resource *resource, wl_resource *surface)
{
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
    p->requestActivate(nullptr, SurfaceInterface::get(surface));
}

void TextInputInterface::Private::disableCallback(wl_client *client, wl_resource *resource, wl_resource *surface)
{
    Q_UNUSED(surface)
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
    p->requestDeactivate(nullptr);
}

void TextInputInterface::Private::showInputPanelCallback(wl_client *client, wl_resource *resource)
{
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
    emit p->q_func()->requestShowInputPanel();
}

void TextInputInterface::Private::hideInputPanelCallback(wl_client *client, wl_resource *resource)
{
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
    emit p->q_func()->requestHideInputPanel();
}

void TextInputInterface::Private::setSurroundingTextCallback(wl_client *client, wl_resource *resource, const char * text, int32_t cursor, int32_t anchor)
{
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
    p->surroundingText = QByteArray(text);
    // TODO: make qint32
    p->surroundingTextCursorPosition = cursor;
    p->surroundingTextSelectionAnchor = anchor;
    emit p->q_func()->surroundingTextChanged();
}

namespace {
static TextInputInterface::ContentHints waylandHintsToKWayland(wl_text_input_content_hint wlHints)
{
    TextInputInterface::ContentHints hints = TextInputInterface::ContentHint::None;

    if (wlHints & WL_TEXT_INPUT_CONTENT_HINT_AUTO_COMPLETION) {
        hints |= TextInputInterface::ContentHint::AutoCompletion;
    }
    if (wlHints & WL_TEXT_INPUT_CONTENT_HINT_AUTO_CORRECTION) {
        hints |= TextInputInterface::ContentHint::AutoCorrection;
    }
    if (wlHints & WL_TEXT_INPUT_CONTENT_HINT_AUTO_CAPITALIZATION) {
        hints |= TextInputInterface::ContentHint::AutoCapitalization;
    }
    if (wlHints & WL_TEXT_INPUT_CONTENT_HINT_LOWERCASE) {
        hints |= TextInputInterface::ContentHint::LowerCase;
    }
    if (wlHints & WL_TEXT_INPUT_CONTENT_HINT_UPPERCASE) {
        hints |= TextInputInterface::ContentHint::UpperCase;
    }
    if (wlHints & WL_TEXT_INPUT_CONTENT_HINT_TITLECASE) {
        hints |= TextInputInterface::ContentHint::TitleCase;
    }
    if (wlHints & WL_TEXT_INPUT_CONTENT_HINT_HIDDEN_TEXT) {
        hints |= TextInputInterface::ContentHint::HiddenText;
    }
    if (wlHints & WL_TEXT_INPUT_CONTENT_HINT_SENSITIVE_DATA) {
        hints |= TextInputInterface::ContentHint::SensitiveData;
    }
    if (wlHints & WL_TEXT_INPUT_CONTENT_HINT_LATIN) {
        hints |= TextInputInterface::ContentHint::Latin;
    }
    if (wlHints & WL_TEXT_INPUT_CONTENT_HINT_MULTILINE) {
        hints |= TextInputInterface::ContentHint::MultiLine;
    }

    return hints;
}

static TextInputInterface::ContentPurpose waylandPurposeToKWayland(wl_text_input_content_purpose purpose)
{
    switch (purpose) {
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

}

void TextInputInterface::Private::setContentTypeCallback(wl_client *client, wl_resource *resource, uint32_t hint, uint32_t wlPurpose)
{
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
    // TODO: pass through Private impl
    const auto hints = waylandHintsToKWayland(wl_text_input_content_hint(hint));
    const auto purpose = waylandPurposeToKWayland(wl_text_input_content_purpose(wlPurpose));
    if (hints != p->contentHints || purpose != p->contentPurpose) {
        p->contentHints = hints;
        p->contentPurpose = purpose;
        emit p->q_func()->contentTypeChanged();
    }
}

void TextInputInterface::Private::setCursorRectangleCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
    const QRect rect = QRect(x, y, width, height);
    if (p->cursorRectangle != rect) {
        p->cursorRectangle = rect;
        emit p->q_func()->cursorRectangleChanged(p->cursorRectangle);
    }
}

void TextInputInterface::Private::setPreferredLanguageCallback(wl_client *client, wl_resource *resource, const char * language)
{
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
    const QByteArray preferredLanguage = QByteArray(language);
    if (p->preferredLanguage != preferredLanguage) {
        p->preferredLanguage = preferredLanguage;
        emit p->q_func()->preferredLanguageChanged(p->preferredLanguage);
    }
}

TextInputInterface::Private::Private(TextInputInterface *q, Global *c, wl_resource *parentResource, const wl_interface *interface, const void *implementation)
    : Resource::Private(q, c, parentResource, interface, implementation)
{
}

TextInputInterface::Private::~Private()
{
    if (resource) {
        wl_resource_destroy(resource);
        resource = nullptr;
    }
}

QByteArray TextInputInterface::preferredLanguage() const
{
    Q_D();
    return d->preferredLanguage;
}

TextInputInterface::ContentHints TextInputInterface::contentHints() const
{
    Q_D();
    return d->contentHints;
}

TextInputInterface::ContentPurpose TextInputInterface::contentPurpose() const
{
    Q_D();
    return d->contentPurpose;
}

QByteArray TextInputInterface::surroundingText() const
{
    Q_D();
    return d->surroundingText;
}

qint32 TextInputInterface::surroundingTextCursorPosition() const
{
    Q_D();
    return d->surroundingTextCursorPosition;
}

qint32 TextInputInterface::surroundingTextSelectionAnchor() const
{
    Q_D();
    return d->surroundingTextSelectionAnchor;
}

void TextInputInterface::preEdit(const QByteArray &text, const QByteArray &commit)
{
    Q_D();
    d->preEdit(text, commit);
}

void TextInputInterface::commit(const QByteArray &text)
{
    Q_D();
    d->commit(text);
}

void TextInputInterface::keysymPressed(quint32 keysym, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    Q_D();
    d->keysymPressed(keysym, modifiers);
}

void TextInputInterface::keysymReleased(quint32 keysym, Qt::KeyboardModifiers modifiers)
{
    Q_D();
    d->keysymReleased(keysym, modifiers);
}

void TextInputInterface::deleteSurroundingText(quint32 beforeLength, quint32 afterLength)
{
    Q_D();
    d->deleteSurroundingText(beforeLength, afterLength);
}

void TextInputInterface::setCursorPosition(qint32 index, qint32 anchor)
{
    Q_D();
    d->setCursorPosition(index, anchor);
}

void TextInputInterface::setTextDirection(Qt::LayoutDirection direction)
{
    Q_D();
    d->setTextDirection(direction);
}

void TextInputInterface::setPreEditCursor(qint32 index)
{
    Q_D();
    d->setPreEditCursor(index);
}

void TextInputInterface::setInputPanelState(bool visible, const QRect &overlappedSurfaceArea)
{
    Q_D();
    if (d->inputPanelVisible == visible && d->overlappedSurfaceArea == overlappedSurfaceArea) {
        // not changed
        return;
    }
    d->inputPanelVisible = visible;
    d->overlappedSurfaceArea = overlappedSurfaceArea;
    d->sendInputPanelState();
}

void TextInputInterface::setLanguage(const QByteArray &languageTag)
{
    Q_D();
    if (d->language == languageTag) {
        // not changed
        return;
    }
    d->language = languageTag;
    d->sendLanguage();
}

TextInputInterfaceVersion TextInputInterface::interfaceVersion() const
{
    Q_D();
    return d->interfaceVersion();
}

QPointer<SurfaceInterface> TextInputInterface::surface() const
{
    Q_D();
    return d->surface;
}

QRect TextInputInterface::cursorRectangle() const
{
    Q_D();
    return d->cursorRectangle;
}

bool TextInputInterface::isEnabled() const
{
    Q_D();
    return d->enabled;
}

TextInputInterface::Private *TextInputInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

TextInputInterface::TextInputInterface(Private *p, QObject *parent)
    : Resource(p, parent)
{
}

TextInputInterface::~TextInputInterface() = default;

TextInputManagerInterface::TextInputManagerInterface(Private *d, QObject *parent)
    : Global(d, parent)
{
}

TextInputManagerInterface::~TextInputManagerInterface() = default;

TextInputInterfaceVersion TextInputManagerInterface::interfaceVersion() const
{
    Q_D();
    return d->interfaceVersion;
}

TextInputManagerInterface::Private *TextInputManagerInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
}
