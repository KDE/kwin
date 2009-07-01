/********************************************************************
Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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
#ifndef THEMECONFIG_H
#define THEMECONFIG_H
// This class encapsulates all theme config values
// it's a separate class as it's needed by both deco and config dialog

#include <QString>
#include <QColor>

class KConfig;

namespace Aurorae
{
class ThemeConfig
{
public:
    ThemeConfig();
    void load(KConfig *conf);
    ~ThemeConfig() {};
    // active window
    QColor activeTextColor() const {
        return m_activeTextColor;
    }
    // inactive window
    QColor inactiveTextColor() const {
        return m_inactiveTextColor;
    }
    // Alignment
    Qt::Alignment alignment() const {
        return m_alignment;
    };
    Qt::Alignment verticalAlignment() const {
        return m_verticalAlignment;
    }
    int animationTime() const {
        return m_animationTime;
    }
    // Borders
    int borderLeft() const {
        return m_borderLeft;
    }
    int borderRight() const {
        return m_borderRight;
    }
    int borderBottom() const {
        return m_borderBottom;
    }

    int titleEdgeTop() const {
        return m_titleEdgeTop;
    }
    int titleEdgeBottom() const {
        return m_titleEdgeBottom;
    }
    int titleEdgeLeft() const {
        return m_titleEdgeLeft;
    }
    int titleEdgeRight() const {
        return m_titleEdgeRight;
    }
    int titleBorderLeft() const {
        return m_titleBorderLeft;
    }
    int titleBorderRight() const {
        return m_titleBorderRight;
    }
    int titleHeight() const {
        return m_titleHeight;
    }

    int buttonWidth() const {
        return m_buttonWidth;
    }
    int buttonHeight() const {
        return m_buttonHeight;
    }
    int buttonSpacing() const {
        return m_buttonSpacing;
    }
    int buttonMarginTop() const {
        return m_buttonMarginTop;
    }
    int explicitButtonSpacer() const {
        return m_explicitButtonSpacer;
    }

    int paddingLeft() const {
        return m_paddingLeft;
    }
    int paddingRight() const {
        return m_paddingRight;
    }
    int paddingTop() const {
        return m_paddingTop;
    }
    int paddingBottom() const {
        return m_paddingBottom;
    }

    QString defaultButtonsLeft() const {
        return m_defaultButtonsLeft;
    }
    QString defaultButtonsRight() const {
        return m_defaultButtonsRight;
    }
    bool shadow() const {
        return m_shadow;
    }

private:
    QColor m_activeTextColor;
    QColor m_inactiveTextColor;
    Qt::Alignment m_alignment;
    Qt::Alignment m_verticalAlignment;
    // borders
    int m_borderLeft;
    int m_borderRight;
    int m_borderBottom;

    // title
    int m_titleEdgeTop;
    int m_titleEdgeBottom;
    int m_titleEdgeLeft;
    int m_titleEdgeRight;
    int m_titleBorderLeft;
    int m_titleBorderRight;
    int m_titleHeight;

    // buttons
    int m_buttonWidth;
    int m_buttonHeight;
    int m_buttonSpacing;
    int m_buttonMarginTop;
    int m_explicitButtonSpacer;

    // padding
    int m_paddingLeft;
    int m_paddingRight;
    int m_paddingTop;
    int m_paddingBottom;

    int m_animationTime;

    QString m_defaultButtonsLeft;
    QString m_defaultButtonsRight;
    bool m_shadow;
};

}

#endif
