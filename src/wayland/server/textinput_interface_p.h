/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_TEXTINPUT_INTERFACE_P_H
#define KWAYLAND_SERVER_TEXTINPUT_INTERFACE_P_H
#include "textinput_interface.h"
#include "resource_p.h"
#include "global_p.h"

#include <QPointer>
#include <QRect>
#include <QVector>

namespace KWayland
{
namespace Server
{

class TextInputManagerUnstableV0Interface;
class TextInputManagerUnstableV2Interface;

class TextInputManagerInterface::Private : public Global::Private
{
public:
    QVector<TextInputInterface*> inputs;
    TextInputInterfaceVersion interfaceVersion;

protected:
    Private(TextInputInterfaceVersion interfaceVersion, TextInputManagerInterface *q, Display *d, const wl_interface *interface, quint32 version);
    TextInputManagerInterface *q;
};

class TextInputInterface::Private : public Resource::Private
{
public:
    ~Private();

    virtual void sendEnter(SurfaceInterface *surface, quint32 serial) = 0;
    virtual void sendLeave(quint32 serial, SurfaceInterface *surface) = 0;

    virtual void preEdit(const QByteArray &text, const QByteArray &commit) = 0;
    virtual void commit(const QByteArray &text) = 0;
    virtual void deleteSurroundingText(quint32 beforeLength, quint32 afterLength) = 0;
    virtual void setTextDirection(Qt::LayoutDirection direction) = 0;
    virtual void setPreEditCursor(qint32 index) = 0;
    virtual void setCursorPosition(qint32 index, qint32 anchor) = 0;
    virtual void keysymPressed(quint32 keysym, Qt::KeyboardModifiers modifiers) = 0;
    virtual void keysymReleased(quint32 keysym, Qt::KeyboardModifiers modifiers) = 0;
    virtual TextInputInterfaceVersion interfaceVersion() const = 0;
    virtual void sendInputPanelState() = 0;
    virtual void sendLanguage() = 0;

    virtual TextInputInterface::ContentHints convertContentHint(uint32_t hint) const = 0;
    virtual TextInputInterface::ContentPurpose convertContentPurpose(uint32_t purpose) const = 0;

    QByteArray preferredLanguage;
    QRect cursorRectangle;
    TextInputInterface::ContentHints contentHints = TextInputInterface::ContentHint::None;
    TextInputInterface::ContentPurpose contentPurpose = TextInputInterface::ContentPurpose::Normal;
    SeatInterface *seat = nullptr;
    QPointer<SurfaceInterface> surface;
    bool enabled = false;
    QByteArray surroundingText;
    qint32 surroundingTextCursorPosition = 0;
    qint32 surroundingTextSelectionAnchor = 0;
    bool inputPanelVisible = false;
    QRect overlappedSurfaceArea;
    QByteArray language;

protected:
    Private(TextInputInterface *q, Global *c, wl_resource *parentResource, const wl_interface *interface, const void *implementation);

    static void showInputPanelCallback(wl_client *client, wl_resource *resource);
    static void hideInputPanelCallback(wl_client *client, wl_resource *resource);
    static void setSurroundingTextCallback(wl_client *client, wl_resource *resource, const char * text, int32_t cursor, int32_t anchor);
    static void setContentTypeCallback(wl_client *client, wl_resource *resource, uint32_t hint, uint32_t purpose);
    static void setCursorRectangleCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height);
    static void setPreferredLanguageCallback(wl_client *client, wl_resource *resource, const char * language);

private:
    TextInputInterface *q_func() {
        return reinterpret_cast<TextInputInterface *>(q);
    }
};

class TextInputUnstableV0Interface : public TextInputInterface
{
    Q_OBJECT
public:
    virtual ~TextInputUnstableV0Interface();

Q_SIGNALS:
    /**
     * @internal
     **/
    void requestActivate(KWayland::Server::SeatInterface *seat, KWayland::Server::SurfaceInterface *surface);

private:
    explicit TextInputUnstableV0Interface(TextInputManagerUnstableV0Interface *parent, wl_resource *parentResource);
    friend class TextInputManagerUnstableV0Interface;
    class Private;
};

class TextInputUnstableV2Interface : public TextInputInterface
{
    Q_OBJECT
public:
    virtual ~TextInputUnstableV2Interface();

private:
    explicit TextInputUnstableV2Interface(TextInputManagerUnstableV2Interface *parent, wl_resource *parentResource);
    friend class TextInputManagerUnstableV2Interface;
    class Private;
};

class TextInputManagerUnstableV0Interface : public TextInputManagerInterface
{
    Q_OBJECT
public:
    explicit TextInputManagerUnstableV0Interface(Display *display, QObject *parent = nullptr);
    virtual ~TextInputManagerUnstableV0Interface();

private:
    class Private;
};

class TextInputManagerUnstableV2Interface : public TextInputManagerInterface
{
    Q_OBJECT
public:
    explicit TextInputManagerUnstableV2Interface(Display *display, QObject *parent = nullptr);
    virtual ~TextInputManagerUnstableV2Interface();

private:
    class Private;
};

}
}

#endif
