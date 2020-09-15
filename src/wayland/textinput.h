/*
    SPDX-FileCopyrightText: 2020 Bhushan Shah <bshah@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef TEXTINPUT_H_INCLUDED
#define TEXTINPUT_H_INCLUDED

#include <KWaylandServer/kwaylandserver_export.h>

#include <QtGlobal>

#include <inttypes.h>

namespace KWaylandServer
{

KWAYLANDSERVER_EXPORT Q_NAMESPACE
/**
 * ContentHint allows to modify the behavior of the text input.
 **/
enum class TextInputContentHint {
    /**
     * no special behaviour
     */
    None = 0,
    /**
     * suggest word completions
     */
    AutoCompletion = 1 << 0,
    /**
     * suggest word corrections
     */
    AutoCorrection = 1 << 1,
    /**
     * switch to uppercase letters at the start of a sentence
     */
    AutoCapitalization = 1 << 2,
    /**
     * prefer lowercase letters
     */
    LowerCase = 1 << 3,
    /**
     * prefer uppercase letters
     */
    UpperCase = 1 << 4,
    /**
     * prefer casing for titles and headings (can be language dependent)
     */
    TitleCase = 1 << 5,
    /**
     * characters should be hidden
     */
    HiddenText = 1 << 6,
    /**
     * typed text should not be stored
     */
    SensitiveData = 1 << 7,
    /**
     * just latin characters should be entered
     */
    Latin = 1 << 8,
    /**
     * the text input is multi line
     */
    MultiLine = 1 << 9
};

Q_DECLARE_FLAGS(TextInputContentHints, TextInputContentHint)

/**
 * The ContentPurpose allows to specify the primary purpose of a text input.
 *
 * This allows an input method to show special purpose input panels with
 * extra characters or to disallow some characters.
 */
enum class TextInputContentPurpose {
    /**
     * default input, allowing all characters
     */
    Normal,
    /**
     * allow only alphabetic characters
     **/
    Alpha,
    /**
     * allow only digits
     */
    Digits,
    /**
     * input a number (including decimal separator and sign)
     */
    Number,
    /**
     * input a phone number
     */
    Phone,
    /**
     * input an URL
     */
    Url,
    /**
     * input an email address
     **/
    Email,
    /**
     * input a name of a person
     */
    Name,
    /**
     * input a password
     */
    Password,
    /**
     * input a date
     */
    Date,
    /**
     * input a time
     */
    Time,
    /**
     * input a date and time
     */
    DateTime,
    /**
     * input for a terminal
     */
    Terminal,
    /**
     * input is numeric password
     */
    Pin
};

enum class TextInputChangeCause {
    /**
     * Change caused by input method
     */
    InputMethod,

    /**
     * Something else other than input method caused change
     */
    Other
};

}

Q_DECLARE_METATYPE(KWaylandServer::TextInputContentHint)
Q_DECLARE_METATYPE(KWaylandServer::TextInputContentHints)
Q_DECLARE_OPERATORS_FOR_FLAGS(KWaylandServer::TextInputContentHints)
Q_DECLARE_METATYPE(KWaylandServer::TextInputContentPurpose)
Q_DECLARE_METATYPE(KWaylandServer::TextInputChangeCause)

#endif // TEXTINPUT_H_INCLUDED
