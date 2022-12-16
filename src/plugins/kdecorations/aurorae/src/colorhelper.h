/*
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QColor>
#include <QObject>

/**
 * @short Helper to manipulate colors.
 *
 * Exports a few functions from KColorScheme.
 */
class ColorHelper : public QObject
{
    Q_OBJECT
    /**
     * Same as KGlobalSettings::contrastF.
     */
    Q_PROPERTY(qreal contrast READ contrast CONSTANT)
public:
    explicit ColorHelper(QObject *parent = nullptr);
    ~ColorHelper() override;
    /**
     * This enumeration describes the color shade being selected from the given
     * set.
     *
     * Color shades are used to draw "3d" elements, such as frames and bevels.
     * They are neither foreground nor background colors. Text should not be
     * painted over a shade, and shades should not be used to draw text.
     */
    enum ShadeRole {
        /**
         * The light color is lighter than dark() or shadow() and contrasts
         * with the base color.
         */
        LightShade,
        /**
         * The midlight color is in between base() and light().
         */
        MidlightShade,
        /**
         * The mid color is in between base() and dark().
         */
        MidShade,
        /**
         * The dark color is in between mid() and shadow().
         */
        DarkShade,
        /**
         * The shadow color is darker than light() or midlight() and contrasts
         * the base color.
         */
        ShadowShade
    };
    Q_ENUM(ShadeRole)
    /**
     * This enumeration describes the background color being selected from the
     * given set.
     *
     * Background colors are suitable for drawing under text, and should never
     * be used to draw text. In combination with one of the overloads of
     * KColorScheme::shade, they may be used to generate colors for drawing
     * frames, bevels, and similar decorations.
     */
    enum BackgroundRole {
        /**
         * Normal background.
         */
        NormalBackground = 0,
        /**
         * Alternate background; for example, for use in lists.
         *
         * This color may be the same as BackgroundNormal, especially in sets
         * other than View and Window.
         */
        AlternateBackground = 1,
        /**
         * Third color; for example, items which are new, active, requesting
         * attention, etc.
         *
         * Alerting the user that a certain field must be filled out would be a
         * good usage (although NegativeBackground could be used to the same
         * effect, depending on what you are trying to achieve). Unlike
         * ActiveText, this should not be used for mouseover effects.
         */
        ActiveBackground = 2,
        /**
         * Fourth color; corresponds to (unvisited) links.
         *
         * Exactly what this might be used for is somewhat harder to qualify;
         * it might be used for bookmarks, as a 'you can click here' indicator,
         * or to highlight recent content (i.e. in a most-recently-accessed
         * list).
         */
        LinkBackground = 3,
        /**
         * Fifth color; corresponds to visited links.
         *
         * This can also be used to indicate "not recent" content, especially
         * when a color is needed to denote content which is "old" or
         * "archival".
         */
        VisitedBackground = 4,
        /**
         * Sixth color; for example, errors, untrusted content, etc.
         */
        NegativeBackground = 5,
        /**
         * Seventh color; for example, warnings, secure/encrypted content.
         */
        NeutralBackground = 6,
        /**
         * Eigth color; for example, success messages, trusted content.
         */
        PositiveBackground = 7
    };
    Q_ENUM(BackgroundRole)

    /**
     * This enumeration describes the foreground color being selected from the
     * given set.
     *
     * Foreground colors are suitable for drawing text or glyphs (such as the
     * symbols on window decoration buttons, assuming a suitable background
     * brush is used), and should never be used to draw backgrounds.
     *
     * For window decorations, the following is suggested, but not set in
     * stone:
     * @li Maximize - PositiveText
     * @li Minimize - NeutralText
     * @li Close - NegativeText
     * @li WhatsThis - LinkText
     * @li Sticky - ActiveText
     */
    enum ForegroundRole {
        /**
         * Normal foreground.
         */
        NormalText = 0,
        /**
         * Second color; for example, comments, items which are old, inactive
         * or disabled. Generally used for things that are meant to be "less
         * important". InactiveText is not the same role as NormalText in the
         * inactive state.
         */
        InactiveText = 1,
        /**
         * Third color; for example items which are new, active, requesting
         * attention, etc. May be used as a hover color for clickable items.
         */
        ActiveText = 2,
        /**
         * Fourth color; use for (unvisited) links. May also be used for other
         * clickable items or content that indicates relationships, items that
         * indicate somewhere the user can visit, etc.
         */
        LinkText = 3,
        /**
         * Fifth color; used for (visited) links. As with LinkText, may be used
         * for items that have already been "visited" or accessed. May also be
         * used to indicate "historical" (i.e. "old") items or information,
         * especially if InactiveText is being used in the same context to
         * express something different.
         */
        VisitedText = 4,
        /**
         * Sixth color; for example, errors, untrusted content, deletions,
         * etc.
         */
        NegativeText = 5,
        /**
         * Seventh color; for example, warnings, secure/encrypted content.
         */
        NeutralText = 6,
        /**
         * Eigth color; for example, additions, success messages, trusted
         * content.
         */
        PositiveText = 7
    };
    Q_ENUM(ForegroundRole)
    /**
     * Retrieve the requested shade color, using the specified color as the
     * base color and the system contrast setting.
     *
     * @note Shades are chosen such that all shades would contrast with the
     * base color. This means that if base is very dark, the 'dark' shades will
     * be lighter than the base color, with midlight() == shadow().
     * Conversely, if the base color is very light, the 'light' shades will be
     * darker than the base color, with light() == mid().
     */
    Q_INVOKABLE QColor shade(const QColor &color, ShadeRole role);
    Q_INVOKABLE QColor shade(const QColor &color, ShadeRole role, qreal contrast);
    /**
     * Retrieve the requested shade color, using the specified color as the
     * base color and the specified contrast.
     *
     * @param contrast Amount roughly specifying the contrast by which to
     * adjust the base color, between -1.0 and 1.0 (values between 0.0 and 1.0
     * correspond to the value from KGlobalSettings::contrastF)
     *
     * @note Shades are chosen such that all shades would contrast with the
     * base color. This means that if base is very dark, the 'dark' shades will
     * be lighter than the base color, with midlight() == shadow().
     * Conversely, if the base color is very light, the 'light' shades will be
     * darker than the base color, with light() == mid().
     *
     * @see KColorUtils::shade
     */
    Q_INVOKABLE QColor multiplyAlpha(const QColor &color, qreal alpha);
    /**
     * Retrieve the requested background brush's color for the @p active button.
     * @param active Whether the active or inactive palette should be used.
     */
    Q_INVOKABLE QColor background(bool active, BackgroundRole role = NormalBackground) const;

    /**
     * Retrieve the requested foreground brush's color for the @p active button.
     * @param active Whether the active or inactive palette should be used.
     */
    Q_INVOKABLE QColor foreground(bool active, ForegroundRole role = NormalText) const;

    qreal contrast() const;
};
