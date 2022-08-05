/*
    SPDX-FileCopyrightText: 2022 MBition GmbH
    SPDX-FileContributor: Kai Uwe Broulik <kai_uwe.broulik@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.15
import QtQuick.Layouts 1.1
import org.kde.plasma.core 2.0 as PlasmaCore

PlasmaCore.SvgItem {
    id: svgItem

    width: glowSvg.elementSize(elementId).width
    height: glowSvg.elementSize(elementId).height

    svg: PlasmaCore.Svg {
        id: glowSvg
        imagePath: "widgets/glowbar"
    }

    elementId: {
        switch (state) {
        case "topleftcorner":
            return "bottomright";
        case "toprightcorner":
            return "bottomleft";
        case "bottomleftcorner":
            return "topright";
        case "bottomrightcorner":
            return "topleft";
        }
    }
}
