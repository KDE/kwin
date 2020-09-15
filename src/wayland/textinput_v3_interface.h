/*
    SPDX-FileCopyrightText: 2018 Roman Glig <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2020 Bhushan Shah <bshah@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_TEXTINPUT_V3_INTERFACE_H
#define KWAYLAND_SERVER_TEXTINPUT_V3_INTERFACE_H

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>
#include "textinput.h"

struct wl_resource;
namespace KWaylandServer
{

class Display;
class SeatInterface;
class SurfaceInterface;
class TextInputV3Interface;
class TextInputV3InterfacePrivate;
class TextInputManagerV3InterfacePrivate;

/**
 * @brief Represent the Global for the interface.
 *
 * The class can represent different interfaces. Which concrete interface is represented
 * can be determined through {@link interfaceVersion}.
 *
 * To create a TextInputManagerV3Interface use {@link Display::createTextInputManager}
 *
 * @since 5.21
 **/
class KWAYLANDSERVER_EXPORT TextInputManagerV3Interface : public QObject
{
    Q_OBJECT
public:
    explicit TextInputManagerV3Interface(Display *display, QObject *parent = nullptr);
    ~TextInputManagerV3Interface() override;

private:
    QScopedPointer<TextInputManagerV3InterfacePrivate> d;
};

/**
 * @brief Represents a generic Resource for a text input object.
 * *
 * A TextInputV3Interface gets created by the {@link TextInputManagerV3Interface}. The individual
 * instances are not exposed directly. The SeatInterface provides access to the currently active
 * TextInputV3Interface. This is evaluated automatically based on which SurfaceInterface has
 * keyboard focus.
 *
 * @see TextInputManagerV3Interface
 * @see SeatInterface
 * @since 5.21
 **/
class KWAYLANDSERVER_EXPORT TextInputV3Interface : public QObject
{
    Q_OBJECT
public:
    ~TextInputV3Interface() override;

    /**
     * @see cursorRectangleChanged
     **/
    QRect cursorRectangle() const;

    /**
     * @see contentTypeChanged
     **/
    TextInputContentPurpose contentPurpose() const;

    /**
     *@see contentTypeChanged
     **/
    TextInputContentHints contentHints() const;

    /**
     * @returns The plain surrounding text around the input position.
     * @see surroundingTextChanged
     * @see surroundingTextCursorPosition
     * @see surroundingTextSelectionAnchor
     **/
    QString surroundingText() const;
    /**
     * @returns The byte offset of current cursor position within the {@link surroundingText}
     * @see surroundingText
     * @see surroundingTextChanged
     **/
    qint32 surroundingTextCursorPosition() const;
    /**
     * The byte offset of the selection anchor within the {@link surroundingText}.
     *
     * If there is no selected text this is the same as cursor.
     * @return The byte offset of the selection anchor
     * @see surroundingText
     * @see surroundingTextChanged
     **/
    qint32 surroundingTextSelectionAnchor() const;

    /**
     * @return The surface the TextInputV3Interface is enabled on
     * @see isEnabled
     * @see enabledChanged
     **/
    QPointer<SurfaceInterface> surface() const;

    /**
     * @return Whether the TextInputV3Interface is currently enabled for a SurfaceInterface.
     * @see surface
     * @see enabledChanged
     **/
    bool isEnabled() const;

    /**
     * Notify when the text around the current cursor position should be deleted.
     *
     * The Client processes this event together with the commit string
     *
     * @param beforeLength length of text before current cursor position.
     * @param afterLength length of text after current cursor position.
     * @see commit
     **/
    void deleteSurroundingText(quint32 beforeLength, quint32 afterLength);
    
    /**
     * Send preEditString to the client
     *
     * @param text pre-edit string
     * @param cursorBegin
     * @param cursorEnd
     **/
    void sendPreEditString(const QString &text, quint32 cursorBegin, quint32 cursorEnd);
    
    /**
     * Notify when @p text should be inserted into the editor widget.
     * The text to commit could be either just a single character after a key press or the
     * result of some composing ({@link preEdit}). It could be also an empty text
     * when some text should be removed (see {@link deleteSurroundingText}) or when
     * the input cursor should be moved (see {@link cursorPosition}).
     *
     * Any previously set composing text should be removed.
     * @param text The utf8-encoded text to be inserted into the editor widget
     * @see preEdit
     * @see deleteSurroundingText
     **/
    void commitString(const QString &text);
    
    /**
     * Notify when changes and state requested by sendPreEditString, commitString and deleteSurroundingText
     * should be applied.
     **/
    void done();


Q_SIGNALS:

    /**
     * @see cursorRectangle
     **/
    void cursorRectangleChanged(const QRect &rect);
    /**
     * Emitted when the {@link contentPurpose} and/or {@link contentHints} changes.
     * @see contentPurpose
     * @see contentHints
     **/
    void contentTypeChanged();

    /**
     * Emitted when the {@link surroundingText}, {@link surroundingTextCursorPosition}
     * and/or {@link surroundingTextSelectionAnchor} changed.
     * @see surroundingText
     * @see surroundingTextCursorPosition
     * @see surroundingTextSelectionAnchor
     **/
    void surroundingTextChanged();

    /**
     * Emitted whenever this TextInputV3Interface gets enabled or disabled for a SurfaceInterface.
     * @see isEnabled
     * @see surface
     **/
    void enabledChanged();

    /**
     * Emitted when state should be committed
     **/
    void stateCommitted(quint32 serial);

private:
    friend class TextInputManagerV3InterfacePrivate;
    friend class SeatInterface;
    friend class TextInputV3InterfacePrivate;
    explicit TextInputV3Interface(SeatInterface *seat);

    QScopedPointer<TextInputV3InterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::TextInputV3Interface *)

#endif
