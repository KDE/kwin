/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
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

void TextInputInterface::Private::setContentTypeCallback(wl_client *client, wl_resource *resource, uint32_t hint, uint32_t purpose)
{
    auto p = cast<Private>(resource);
    Q_ASSERT(*p->client == client);
    const auto contentHints = p->convertContentHint(hint);
    const auto contentPurpose = p->convertContentPurpose(purpose);
    if (contentHints != p->contentHints || contentPurpose != p->contentPurpose) {
        p->contentHints = contentHints;
        p->contentPurpose = contentPurpose;
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
