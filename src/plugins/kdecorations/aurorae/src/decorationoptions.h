/*
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <KDecoration2/Decoration>

#include <QColor>
#include <QFont>
#include <QObject>
#include <QPalette>

namespace KWin
{

// TODO: move to deco API
class ColorSettings
{
public:
    ColorSettings(const QPalette &pal);

    void update(const QPalette &pal);

    const QColor &titleBarColor(bool active) const
    {
        return active ? m_activeTitleBarColor : m_inactiveTitleBarColor;
    }
    const QColor &activeTitleBarColor() const
    {
        return m_activeTitleBarColor;
    }
    const QColor &inactiveTitleBarColor() const
    {
        return m_inactiveTitleBarColor;
    }
    const QColor &activeTitleBarBlendColor() const
    {
        return m_activeTitleBarBlendColor;
    }
    const QColor &inactiveTitleBarBlendColor() const
    {
        return m_inactiveTitleBarBlendColor;
    }
    const QColor &frame(bool active) const
    {
        return active ? m_activeFrameColor : m_inactiveFrameColor;
    }
    const QColor &activeFrame() const
    {
        return m_activeFrameColor;
    }
    const QColor &inactiveFrame() const
    {
        return m_inactiveFrameColor;
    }
    const QColor &font(bool active) const
    {
        return active ? m_activeFontColor : m_inactiveFontColor;
    }
    const QColor &activeFont() const
    {
        return m_activeFontColor;
    }
    const QColor &inactiveFont() const
    {
        return m_inactiveFontColor;
    }
    const QColor &activeButtonColor() const
    {
        return m_activeButtonColor;
    }
    const QColor &inactiveButtonColor() const
    {
        return m_inactiveButtonColor;
    }
    const QColor &activeHandle() const
    {
        return m_activeHandle;
    }
    const QColor &inactiveHandle() const
    {
        return m_inactiveHandle;
    }
    const QPalette &palette() const
    {
        return m_palette;
    }

private:
    void init(const QPalette &pal);
    QColor m_activeTitleBarColor;
    QColor m_inactiveTitleBarColor;
    QColor m_activeTitleBarBlendColor;
    QColor m_inactiveTitleBarBlendColor;
    QColor m_activeFrameColor;
    QColor m_inactiveFrameColor;
    QColor m_activeFontColor;
    QColor m_inactiveFontColor;
    QColor m_activeButtonColor;
    QColor m_inactiveButtonColor;
    QColor m_activeHandle;
    QColor m_inactiveHandle;
    QPalette m_palette;
};

/**
 * @short Common Window Decoration Options.
 *
 * This Class provides common window decoration options which can be used, but do not have to
 * be used by a window decoration. The class provides properties for global settings such as
 * color, font and decoration button position.
 *
 * If a window decoration wants to follow the global color scheme it should honor the values
 * provided by the properties.
 *
 * In any case it makes sense to respect the font settings for the decoration as this is also
 * an accessibility feature.
 *
 * In order to use the options in a QML based window decoration an instance of this object needs
 * to be created and the as a context property available "decoration" needs to be passed to the
 * DecorationOptions instance:
 *
 * @code
 * DecorationOptions {
 *    id: options
 *    deco: decoration
 * }
 * @endcode
 */
class DecorationOptions : public QObject
{
    Q_OBJECT
    /**
     * The decoration Object for which this set of options should be used. The decoration is
     * required to get the correct colors and fonts depending on whether the decoration represents
     * an active or inactive window.
     *
     * Best pass the decoration object available as a context property to this property.
     */
    Q_PROPERTY(KDecoration2::Decoration *deco READ decoration WRITE setDecoration NOTIFY decorationChanged)
    /**
     * The color for the titlebar depending on the decoration's active state.
     */
    Q_PROPERTY(QColor titleBarColor READ titleBarColor NOTIFY colorsChanged)
    /**
     * The blend color for the titlebar depending on the decoration's active state.
     */
    Q_PROPERTY(QColor titleBarBlendColor READ titleBarBlendColor NOTIFY colorsChanged)
    /**
     * The titlebar text color depending on the decoration's active state.
     */
    Q_PROPERTY(QColor fontColor READ fontColor NOTIFY colorsChanged)
    /**
     * The color to use for titlebar buttons depending on the decoration's active state.
     */
    Q_PROPERTY(QColor buttonColor READ buttonColor NOTIFY colorsChanged)
    /**
     * The color for the window frame (border) depending on the decoration's active state.
     */
    Q_PROPERTY(QColor borderColor READ borderColor NOTIFY colorsChanged)
    /**
     * The color for the resize handle depending on the decoration's active state.
     */
    Q_PROPERTY(QColor resizeHandleColor READ resizeHandleColor NOTIFY colorsChanged)
    /**
     * The font to be used for the decoration caption depending on the decoration's active state.
     */
    Q_PROPERTY(QFont titleFont READ titleFont NOTIFY fontChanged)
    /**
     * The buttons to be positioned on the left side of the titlebar from left to right.
     */
    Q_PROPERTY(QList<int> titleButtonsLeft READ titleButtonsLeft NOTIFY titleButtonsChanged)
    /**
     * The buttons to be positioned on the right side of the titlebar from left to right.
     */
    Q_PROPERTY(QList<int> titleButtonsRight READ titleButtonsRight NOTIFY titleButtonsChanged)
    Q_PROPERTY(int mousePressAndHoldInterval READ mousePressAndHoldInterval CONSTANT)
public:
    enum BorderSize {
        BorderNone, ///< No borders except title
        BorderNoSides, ///< No borders on sides
        BorderTiny, ///< Minimal borders
        BorderNormal, ///< Standard size borders, the default setting
        BorderLarge, ///< Larger borders
        BorderVeryLarge, ///< Very large borders
        BorderHuge, ///< Huge borders
        BorderVeryHuge, ///< Very huge borders
        BorderOversized ///< Oversized borders
    };
    Q_ENUM(BorderSize)
    /**
     * Enum values to identify the decorations buttons which should be used
     * by the decoration.
     *
     */
    enum DecorationButton {
        /**
         * Invalid button value. A decoration should not create a button for
         * this type.
         */
        DecorationButtonNone,
        DecorationButtonMenu,
        DecorationButtonApplicationMenu,
        DecorationButtonOnAllDesktops,
        DecorationButtonQuickHelp,
        DecorationButtonMinimize,
        DecorationButtonMaximizeRestore,
        DecorationButtonClose,
        DecorationButtonKeepAbove,
        DecorationButtonKeepBelow,
        DecorationButtonShade,
        DecorationButtonResize,
        /**
         * The decoration should create an empty spacer instead of a button for
         * this type.
         */
        DecorationButtonExplicitSpacer
    };
    Q_ENUM(DecorationButton)
    explicit DecorationOptions(QObject *parent = nullptr);
    ~DecorationOptions() override;

    QColor titleBarColor() const;
    QColor titleBarBlendColor() const;
    QColor fontColor() const;
    QColor buttonColor() const;
    QColor borderColor() const;
    QColor resizeHandleColor() const;
    QFont titleFont() const;
    QList<int> titleButtonsLeft() const;
    QList<int> titleButtonsRight() const;
    KDecoration2::Decoration *decoration() const;
    void setDecoration(KDecoration2::Decoration *decoration);

    int mousePressAndHoldInterval() const;

Q_SIGNALS:
    void colorsChanged();
    void fontChanged();
    void decorationChanged();
    void titleButtonsChanged();

private Q_SLOTS:
    void slotActiveChanged();

private:
    bool m_active;
    KDecoration2::Decoration *m_decoration;
    ColorSettings m_colors;
    QMetaObject::Connection m_paletteConnection;
};

class Borders : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int left READ left WRITE setLeft NOTIFY leftChanged)
    Q_PROPERTY(int right READ right WRITE setRight NOTIFY rightChanged)
    Q_PROPERTY(int top READ top WRITE setTop NOTIFY topChanged)
    Q_PROPERTY(int bottom READ bottom WRITE setBottom NOTIFY bottomChanged)
public:
    Borders(QObject *parent = nullptr);
    ~Borders() override;
    int left() const;
    int right() const;
    int top() const;
    int bottom() const;

    void setLeft(int left);
    void setRight(int right);
    void setTop(int top);
    void setBottom(int bottom);

    operator QMargins() const;

public Q_SLOTS:
    /**
     * Sets all four borders to @p value.
     */
    void setAllBorders(int value);
    /**
     * Sets all borders except the title border to @p value.
     */
    void setBorders(int value);
    /**
     * Sets the side borders (e.g. if title is on top, the left and right borders)
     * to @p value.
     */
    void setSideBorders(int value);
    /**
     * Sets the title border to @p value.
     */
    void setTitle(int value);

Q_SIGNALS:
    void leftChanged();
    void rightChanged();
    void topChanged();
    void bottomChanged();

private:
    int m_left;
    int m_right;
    int m_top;
    int m_bottom;
};

#define GETTER(name)                 \
    inline int Borders::name() const \
    {                                \
        return m_##name;             \
    }

GETTER(left)
GETTER(right)
GETTER(top)
GETTER(bottom)

#undef GETTER

} // namespace
