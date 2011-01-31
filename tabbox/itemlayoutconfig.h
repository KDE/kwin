/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef TABBOXITEMLAYOUTCONFIG_H
#define TABBOXITEMLAYOUTCONFIG_H

#include <QList>
#include <QSize>
#include <QString>
/**
* @file
* This file defines the classes ItemLayoutConfig, ItemLayoutConfigRow and
* ItemLayoutConfigRowElement for laying out items in the TabBox.
* Each ItemLayoutConfig contains one to many ItemLayoutConfigRows. Each
* row can again contain one to many ItemLayoutConfigRowElement.
*
* @author Martin Gräßlin <kde@martin-graesslin.com>
* @since 4.4
*/
#include "tabboxconfig.h"

namespace KWin
{
namespace TabBox
{

/**
* This class describes one element of a ItemLayoutConfigRow.
* The element has a type and settings depending on the type.
*
* @author Martin Gräßlin <kde@martin-graesslin.com>
* @since 4.4
*/
class ItemLayoutConfigRowElement
{
public:
    /**
    * ElementType defines the type of the current element
    */
    enum ElementType {
        ElementClientName, ///< The element is a TabBoxClient name
        ElementDesktopName, ///< The element is a desktop name
        ElementIcon, ///< The element is an icon
        ElementEmpty, ///< The element is empty, that is a dummy
        ElementClientList ///< A complete client list for the desktop
    };
    ItemLayoutConfigRowElement();
    ~ItemLayoutConfigRowElement();

    /**
    * @return The ElementType of this element
    */
    ElementType type() const {
        return m_type;
    }
    /**
    * @param type Set the ElementType of this element
    */
    void setType(ElementType type) {
        m_type = type;
    }

    // TODO: iconSize should be small, normal, large
    /**
    * @return The size of the icon.
    * This option only applies if ElementType is ElementIcon.
    */
    QSizeF iconSize() const {
        return m_iconSize;
    }
    /**
    * @param size Set the size of the icon.
    * This option only applies if ElementType is ElementIcon.
    */
    void setIconSize(const QSizeF& size) {
        m_iconSize = size;
    }

    /**
    * @returns The text alignment for text elements.
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    Qt::Alignment alignment() const {
        return m_alignment;
    }
    /**
    * @param alignment Set the text alignment for text elements.
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    void setAlignment(Qt::Alignment alignment) {
        m_alignment = alignment;
    }

    /**
    * Stretch defines if a text element uses the maximum available size.
    * Only one element in a row may have set a stretch and it should be
    * the last element.
    * @return The element is set to stretch.
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    bool isStretch() const {
        return m_stretch;
    }
    /**
    * @param stretch Set stretch for the element
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    * @see isStretch
    */
    void setStretch(bool stretch) {
        m_stretch = stretch;
    }

    /**
    * @return True if smallest readable font should be used, false
    * for a normal font.
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    bool isSmallTextSize() const {
        return m_smallTextSize;
    }
    /**
    * @param small Set text element to use smallest readable font.
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    void setSmallTextSize(bool small) {
        m_smallTextSize = small;
    }

    /**
    * @return Text should be rendered with a bold font.
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    bool isBold() const {
        return m_bold;
    }
    /**
    * @param bold Set text element to use a bold font or not.
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    void setBold(bool bold) {
        m_bold = bold;
    }

    /**
    * @return Text should be rendered with an italic font.
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    bool isItalic() const {
        return m_italic;
    }
    /**
    * @param italic Set text element to use an italic font or not
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    void setItalic(bool italic) {
        m_italic = italic;
    }

    /**
    * @return True if an italic font should be used for a minimized client.
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    bool isItalicMinimized() const {
        return m_italicMinimized;
    }
    /**
    * @param italic Set text element to use an italic font for minimized clients.
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    void setItalicMinimized(bool italic) {
        m_italicMinimized = italic;
    }

    /**
    * @return A prefix added to the text element.
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    QString prefix() const {
        return m_prefix;
    }
    /**
    * @param prefix The prefix to be added to the text element
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    void setPrefix(const QString& prefix) {
        m_prefix = prefix;
    }

    /**
    * @return A suffix added to the text element.
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    QString suffix() const {
        return m_suffix;
    }
    /**
    * @param suffix The suffix to be added to the text element
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    void setSuffix(const QString& suffix) {
        m_suffix = suffix;
    }

    /**
    * @return A prefix added to the text element of minimized clients.
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    QString prefixMinimized() const {
        return m_prefixMinimized;
    }
    /**
    * @param prefix The prefix to be added to the text element of minimized clients.
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    void setPrefixMinimized(const QString& prefix) {
        m_prefixMinimized = prefix;
    }

    /**
    * @return A suffix added to the text element of minimized clients.
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    QString suffixMinimized() const {
        return m_suffixMinimized;
    }
    /**
    * @param suffix The suffix to be added to the text element of minimized clients
    * This option only applies if ElementType is ElementClientName or ElementDesktopName
    */
    void setSuffixMinimzed(const QString& suffix) {
        m_suffixMinimized = suffix;
    }

    /**
    * @return The icon element spans all rows.
    * This option only applies if ElementType is ElementIcon.
    */
    bool isRowSpan() const {
        return m_rowSpan;
    }
    /**
    * @param rowSpan The icon element should span all rows or not.
    * This option only applies if ElementType is ElementIcon.
    */
    void setRowSpan(bool rowSpan) {
        m_rowSpan = rowSpan;
    }

    qreal width() const {
        return m_width;
    }
    void setWidth(qreal width) {
        m_width = width;
    }

    /**
    * @return The layout mode for the included client list
    * This option only applies if ElementType is ElementClientList
    */
    TabBoxConfig::LayoutMode clientListLayoutMode() const {
        return m_clientListLayoutMode;
    }
    /**
    * @param mode The layout mode for the included client list
    * This option only applies if ElementType is ElementClientList
    */
    void setClientListLayoutMode(TabBoxConfig::LayoutMode mode) {
        m_clientListLayoutMode = mode;
    }

    /**
    * @return The name of the layout for the included client list
    * This option only applies if ElementType is ElementClientList
    */
    QString clientListLayoutName() const {
        return m_clientListLayoutName;
    }
    /**
    * @param name The name for the layout of the included client list
    * This option only applies if ElementType is ElementClientList
    */
    void setClientListLayoutName(const QString& name) {
        m_clientListLayoutName = name;
    }

private:
    // type of the current element
    ElementType m_type;
    // size of the icon if m_type == ElementIcon
    QSizeF m_iconSize;
    // Text alignment options for m_type != ElementIcon
    Qt::Alignment m_alignment;
    // element stretches as far as possible
    // only one element per row can have this option
    bool m_stretch;
    /**
    * Use smallest readable font or normal font
    */
    bool m_smallTextSize;
    bool m_bold;
    bool m_italic;
    /**
    * Use italic font if client is minimized
    */
    bool m_italicMinimized;
    QString m_prefix;
    QString m_suffix;
    /**
    * Prefix to use if client is minimized. E.g. "("
    */
    QString m_prefixMinimized;
    /**
    * Suffix to use if client is minimized. E.g. ")"
    */
    QString m_suffixMinimized;
    // indicates that the icon spans all rows
    bool m_rowSpan;
    qreal m_width;

    TabBoxConfig::LayoutMode m_clientListLayoutMode;
    QString m_clientListLayoutName;
};
/**
* This class describes one row of the ItemLayoutConfig.
* One row consists of one to many ItemlayoutConfigRowElements.
*
* @author Martin Gräßlin <kde@martin-graesslin.com>
* @since 4.4
*/
class ItemLayoutConfigRow
{
public:
    ItemLayoutConfigRow();
    ~ItemLayoutConfigRow();
    /**
    * @param element The ItemLayoutConfigRowElement to be added to this row.
    */
    void addElement(ItemLayoutConfigRowElement element);
    /**
    * @return The number of ItemLayoutConfigRowElements in this row.
    */
    int count() const;
    /**
    * @param index The element to retrieve
    * @return The ItemLayoutConfigRowElement at the given index.
    */
    ItemLayoutConfigRowElement element(int index) const;

private:
    QList< ItemLayoutConfigRowElement > m_elements;
};

/**
* This class describes the item layout config. It is used by the item delegates
* to calculate the size and render the items.
*
* @author Martin Gräßlin <kde@martin-graesslin.com>
* @since 4.4
*/
class ItemLayoutConfig
{
public:
    ItemLayoutConfig();
    ~ItemLayoutConfig();

    /**
    * @param row The ItemlayoutConfigRow which should be added to this config
    */
    void addRow(ItemLayoutConfigRow row);
    /**
    * @return The number of ItemLayoutConfigRows.
    */
    int count() const;
    /**
    * @param index The row to retrieve
    * @return The ItemLayoutConfigRow at given index.
    */
    ItemLayoutConfigRow row(int index) const;
    /**
    * @return The icon of a selected item should be rendered with a highlight
    * icon effect
    */
    bool isHighlightSelectedIcons() const {
        return m_highlightSelectedIcons;
    }
    void setHighlightSelectedIcons(bool highlight) {
        m_highlightSelectedIcons = highlight;
    }
    /**
    * @return The icon of a not selected item should be rendered with a
    * grayscale icon effect.
    */
    bool isGrayscaleDeselectedIcons() const {
        return m_grayscaleDeselectedIcons;
    }
    void setGrayscaleDeselectedIcons(bool grayscale) {
        m_grayscaleDeselectedIcons = grayscale;
    }

private:
    QList< ItemLayoutConfigRow > m_rows;
    bool m_highlightSelectedIcons;
    bool m_grayscaleDeselectedIcons;
};

} //namespace TabBox
} //namespace KWin

#endif // TABBOXITEMLAYOUTCONFIG_H
