/********************************************************************
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_DECORATION_OPTIONS_H
#define KWIN_DECORATION_OPTIONS_H

#include <QObject>
#include <QColor>
#include <QFont>

namespace KWin
{

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
 **/
class DecorationOptions : public QObject
{
    Q_OBJECT
    Q_ENUMS(BorderSize)
    /**
     * The decoration Object for which this set of options should be used. The decoration is
     * required to get the correct colors and fonts depending on whether the decoration represents
     * an active or inactive window.
     *
     * Best pass the decoration object available as a context property to this property.
     **/
    Q_PROPERTY(QObject *deco READ decoration WRITE setDecoration NOTIFY decorationChanged)
    /**
     * The color for the titlebar depending on the decoration's active state.
     **/
    Q_PROPERTY(QColor titleBarColor READ titleBarColor NOTIFY colorsChanged)
    /**
     * The blend color for the titlebar depending on the decoration's active state.
     **/
    Q_PROPERTY(QColor titleBarBlendColor READ titleBarBlendColor NOTIFY colorsChanged)
    /**
     * The titlebar text color depending on the decoration's active state.
     **/
    Q_PROPERTY(QColor fontColor READ fontColor NOTIFY colorsChanged)
    /**
     * The color to use for titlebar buttons depending on the decoration's active state.
     **/
    Q_PROPERTY(QColor buttonColor READ buttonColor NOTIFY colorsChanged)
    /**
     * The color for the window frame (border) depending on the decoration's active state.
     **/
    Q_PROPERTY(QColor borderColor READ borderColor NOTIFY colorsChanged)
    /**
     * The color for the resize handle depending on the decoration's active state.
     **/
    Q_PROPERTY(QColor resizeHandleColor READ resizeHandleColor NOTIFY colorsChanged)
    /**
     * The font to be used for the decoration caption depending on the decoration's active state.
     **/
    Q_PROPERTY(QFont titleFont READ titleFont NOTIFY fontChanged)
    /**
     * The buttons to be positioned on the left side of the titlebar from left to right.
     *
     * Characters in the returned string have the following meaning:
     * <ul>
     * <li>'M' menu button</li>
     * <li>'S' on all desktops button</li>
     * <li>'H' quickhelp button</li>
     * <li>'I' minimize button</li>
     * <li>'A' maximize button</li>
     * <li>'X' close button</li>
     * <li>'F' keep above button</li>
     * <li>'B' keep below button</li>
     * <li>'L' shade button</li>
     * <li>'_' explicit spacer</li>
     * </ul>
     * @todo: make this a list of enum values
     **/
    Q_PROPERTY(QString titleButtonsLeft READ titleButtonsLeft NOTIFY titleButtonsChanged)
    /**
     * The buttons to be positioned on the right side of the titlebar from left to right.
     * @see titleButtonsRight
     **/
    Q_PROPERTY(QString titleButtonsRight READ titleButtonsRight NOTIFY titleButtonsChanged)
public:
    enum BorderSize {
        BorderTiny,      ///< Minimal borders
        BorderNormal,    ///< Standard size borders, the default setting
        BorderLarge,     ///< Larger borders
        BorderVeryLarge, ///< Very large borders
        BorderHuge,      ///< Huge borders
        BorderVeryHuge,  ///< Very huge borders
        BorderOversized, ///< Oversized borders
        BorderNoSides,   ///< No borders on sides
        BorderNone       ///< No borders except title
    };
    explicit DecorationOptions(QObject *parent = 0);
    virtual ~DecorationOptions();

    QColor titleBarColor() const;
    QColor titleBarBlendColor() const;
    QColor fontColor() const;
    QColor buttonColor() const;
    QColor borderColor() const;
    QColor resizeHandleColor() const;
    QFont titleFont() const;
    QString titleButtonsLeft() const;
    QString titleButtonsRight() const;
    QObject *decoration() const;
    void setDecoration(QObject *decoration);

signals:
    void colorsChanged();
    void fontChanged();
    void decorationChanged();
    void titleButtonsChanged();

private slots:
    void slotActiveChanged();

private:
    bool m_active;
    QObject *m_decoration;
};

class Borders : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int left READ left WRITE setLeft NOTIFY leftChanged)
    Q_PROPERTY(int right READ right WRITE setRight NOTIFY rightChanged)
    Q_PROPERTY(int top READ top WRITE setTop NOTIFY topChanged)
    Q_PROPERTY(int bottom READ bottom WRITE setBottom NOTIFY bottomChanged)
public:
    Borders(QObject *parent = NULL);
    virtual ~Borders();
    int left() const;
    int right() const;
    int top() const;
    int bottom() const;

    void setLeft(int left);
    void setRight(int right);
    void setTop(int top);
    void setBottom(int bottom);

public Q_SLOTS:
    /**
     * Sets all four borders to @p value.
     **/
    void setAllBorders(int value);
    /**
     * Sets all borders except the title border to @p value.
     **/
    void setBorders(int value);
    /**
     * Sets the side borders (e.g. if title is on top, the left and right borders)
     * to @p value.
     **/
    void setSideBorders(int value);
    /**
     * Sets the title border to @p value.
     **/
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

#define GETTER( name ) \
inline int Borders::name() const \
{ \
    return m_##name;\
}\

GETTER(left)
GETTER(right)
GETTER(top)
GETTER(bottom)

#undef GETTER

} // namespace
#endif // KWIN_DECORATION_OPTIONS_H
