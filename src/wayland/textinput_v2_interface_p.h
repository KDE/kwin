/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "clientconnection.h"
#include "textinput_v2_interface.h"

#include <QPointer>
#include <QRect>
#include <QSet>
#include <QVector>

#include <qwayland-server-text-input-unstable-v2.h>

namespace KWaylandServer
{
class TextInputManagerV2InterfacePrivate : public QtWaylandServer::zwp_text_input_manager_v2
{
public:
    TextInputManagerV2InterfacePrivate(TextInputManagerV2Interface *_q, Display *display);

    TextInputManagerV2Interface *q;

protected:
    void zwp_text_input_manager_v2_destroy(Resource *resource) override;
    void zwp_text_input_manager_v2_get_text_input(Resource *resource, uint32_t id, wl_resource *seat) override;
};

class TextInputV2InterfacePrivate : public QtWaylandServer::zwp_text_input_v2
{
public:
    TextInputV2InterfacePrivate(SeatInterface *seat, TextInputV2Interface *_q);

    void sendEnter(SurfaceInterface *surface, quint32 serial);
    void sendLeave(quint32 serial, SurfaceInterface *surface);
    void preEdit(const QString &text, const QString &commit);
    void preEditStyling(uint32_t index, uint32_t length, uint32_t style);
    void commitString(const QString &text);
    void deleteSurroundingText(quint32 beforeLength, quint32 afterLength);
    void setTextDirection(Qt::LayoutDirection direction);
    void setPreEditCursor(qint32 index);
    void setCursorPosition(qint32 index, qint32 anchor);
    void keysymPressed(quint32 keysym, quint32 modifiers);
    void keysymReleased(quint32 keysym, quint32 modifiers);
    void sendInputPanelState();
    void sendLanguage();
    void sendModifiersMap();

    QList<Resource *> textInputsForClient(ClientConnection *client) const;
    static TextInputV2InterfacePrivate *get(TextInputV2Interface *inputInterface)
    {
        return inputInterface->d.get();
    }

    QString preferredLanguage;
    QRect cursorRectangle;
    TextInputContentHints contentHints = TextInputContentHint::None;
    TextInputContentPurpose contentPurpose = TextInputContentPurpose::Normal;
    SeatInterface *seat = nullptr;
    QPointer<SurfaceInterface> surface;
    QString surroundingText;
    qint32 surroundingTextCursorPosition = 0;
    qint32 surroundingTextSelectionAnchor = 0;
    bool inputPanelVisible = false;
    QRect overlappedSurfaceArea;
    QString language;
    QByteArray modifiersMap;
    TextInputV2Interface *q;
    QSet<SurfaceInterface *> m_enabledSurfaces;

protected:
    void zwp_text_input_v2_enable(Resource *resource, wl_resource *surface) override;
    void zwp_text_input_v2_disable(Resource *resource, wl_resource *surface) override;
    void zwp_text_input_v2_show_input_panel(Resource *resource) override;
    void zwp_text_input_v2_hide_input_panel(Resource *resource) override;
    void zwp_text_input_v2_set_surrounding_text(Resource *resource, const QString &text, int32_t cursor, int32_t anchor) override;
    void zwp_text_input_v2_set_content_type(Resource *resource, uint32_t hint, uint32_t purpose) override;
    void zwp_text_input_v2_set_cursor_rectangle(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void zwp_text_input_v2_set_preferred_language(Resource *resource, const QString &language) override;
    void zwp_text_input_v2_update_state(Resource *resource, uint32_t serial, uint32_t reason) override;
};

}
