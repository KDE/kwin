/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Thomas Lübking <thomas.luebking@web.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowgeometry.h"
// KConfigSkeleton
#include "windowgeometryconfig.h"

#include <QAction>
#include <QLocale>
#include <QStringBuilder>
#include <kwinconfig.h>
#include <kconfiggroup.h>
#include <KGlobalAccel>
#include <KLocalizedString>

namespace KWin
{

WindowGeometry::WindowGeometry()
{
    initConfig<WindowGeometryConfiguration>();
    iAmActivated = true;
    iAmActive = false;
    myResizeWindow = nullptr;
#define myResizeString "Window geometry display, %1 and %2 are the new size," \
                       " %3 and %4 are pixel increments - avoid reformatting or suffixes like 'px'", \
                       "Width: %1 (%3)\nHeight: %2 (%4)"
#define myCoordString_0 "Window geometry display, %1 and %2 are the cartesian x and y coordinates" \
                        " - avoid reformatting or suffixes like 'px'", \
                        "X: %1\nY: %2"
#define myCoordString_1 "Window geometry display, %1 and %2 are the cartesian x and y coordinates," \
                        " %3 and %4 are the resp. increments - avoid reformatting or suffixes like 'px'", \
                        "X: %1 (%3)\nY: %2 (%4)"
    reconfigure(ReconfigureAll);
    QAction* a = new QAction(this);
    a->setObjectName(QStringLiteral("WindowGeometry"));
    a->setText(i18n("Toggle window geometry display (effect only)"));

    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::SHIFT + Qt::Key_F11);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::SHIFT + Qt::Key_F11);
    effects->registerGlobalShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_F11, a);

    connect(a, &QAction::triggered, this, &WindowGeometry::toggle);

    connect(effects, &EffectsHandler::windowStartUserMovedResized, this, &WindowGeometry::slotWindowStartUserMovedResized);
    connect(effects, &EffectsHandler::windowFinishUserMovedResized, this, &WindowGeometry::slotWindowFinishUserMovedResized);
    connect(effects, &EffectsHandler::windowStepUserMovedResized, this, &WindowGeometry::slotWindowStepUserMovedResized);
}

WindowGeometry::~WindowGeometry()
{
    for (int i = 0; i < 3; ++i)
        delete myMeasure[i];
}

void WindowGeometry::createFrames()
{
    if (myMeasure[0]) {
        return;
    }
    QFont fnt; fnt.setBold(true); fnt.setPointSize(12);
    for (int i = 0; i < 3; ++i) {
        myMeasure[i] = effects->effectFrame(EffectFrameUnstyled, false);
        myMeasure[i]->setFont(fnt);
    }
    myMeasure[0]->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    myMeasure[1]->setAlignment(Qt::AlignCenter);
    myMeasure[2]->setAlignment(Qt::AlignRight | Qt::AlignBottom);

}

void WindowGeometry::reconfigure(ReconfigureFlags)
{
    WindowGeometryConfiguration::self()->read();
    iHandleMoves = WindowGeometryConfiguration::move();
    iHandleResizes = WindowGeometryConfiguration::resize();
}

void WindowGeometry::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);
    if (iAmActivated && iAmActive) {
        for (int i = 0; i < 3; ++i)
            myMeasure[i]->render(infiniteRegion(), 1.0, .66);
    }
}

void WindowGeometry::toggle()
{
    iAmActivated = !iAmActivated;
    createFrames();
}

void WindowGeometry::slotWindowStartUserMovedResized(EffectWindow *w)
{
    if (!iAmActivated)
        return;
    if (w->isUserResize() && !iHandleResizes)
        return;
    if (w->isUserMove() && !iHandleMoves)
        return;

    createFrames();
    iAmActive = true;
    myResizeWindow = w;
    myOriginalGeometry = w->geometry();
    myCurrentGeometry = w->geometry();
    slotWindowStepUserMovedResized(w, w->geometry());
}

void WindowGeometry::slotWindowFinishUserMovedResized(EffectWindow *w)
{
    if (iAmActive && w == myResizeWindow) {
        iAmActive = false;
        myResizeWindow = nullptr;
        w->addRepaintFull();
        if (myExtraDirtyArea.isValid())
            w->addLayerRepaint(myExtraDirtyArea);
        myExtraDirtyArea = QRect();
    }
}

static inline QString number(int n)
{
    QLocale locale;
    QString sign;
    if (n >= 0) {
        sign = locale.positiveSign();
        if (sign.isEmpty()) sign = QStringLiteral("+");
    }
    else {
        n = -n;
        sign = locale.negativeSign();
        if (sign.isEmpty()) sign = QStringLiteral("-");
    }
    return  sign + QString::number(n);
}


void WindowGeometry::slotWindowStepUserMovedResized(EffectWindow *w, const QRect &geometry)
{
    if (iAmActivated && iAmActive && w == myResizeWindow) {
        if (myExtraDirtyArea.isValid())
            effects->addRepaint(myExtraDirtyArea);

        myExtraDirtyArea = QRect();

        myCurrentGeometry = geometry;
        QPoint center = geometry.center();
        const QRect &r = geometry;
        const QRect &r2 = myOriginalGeometry;
        const QRect screen = effects->clientArea(ScreenArea, w);
        QRect expandedGeometry = w->expandedGeometry();
        expandedGeometry = geometry.adjusted(expandedGeometry.x() - w->x(),
                                             expandedGeometry.y() - w->y(),
                                             expandedGeometry.right() - w->geometry().right(),
                                             expandedGeometry.bottom() - w->geometry().bottom());

        // sufficient for moves, resizes calculated otherwise
        int dx = r.x() - r2.x();
        int dy = r.y() - r2.y();

        // upper left ----------------------
        if (w->isUserResize())
            myMeasure[0]->setText( i18nc(myCoordString_1, r.x(), r.y(), number(dx), number(dy) ) );
        else
            myMeasure[0]->setText( i18nc(myCoordString_0, r.x(), r.y() ) );
        QPoint pos = expandedGeometry.topLeft();
        pos = QPoint(qMax(pos.x(), screen.x()), qMax(pos.y(), screen.y()));
        myMeasure[0]->setPosition(pos + QPoint(6,6)); // "6" is magic number because the unstyled effectframe has 5px padding

        // center ----------------------
        if (w->isUserResize()) {
            // calc width for center element, otherwise the current dx/dx remains right
            dx = r.width() - r2.width();
            dy = r.height() - r2.height();

            const QSize baseInc = w->basicUnit();
            if (baseInc != QSize(1,1)) {
                Q_ASSERT(baseInc.width() && baseInc.height());
                const QSize csz = w->contentsRect().size();
                myMeasure[1]->setText( i18nc(myResizeString, csz.width()/baseInc.width(), csz.height()/baseInc.height(), number(dx/baseInc.width()), number(dy/baseInc.height()) ) );
            } else
                myMeasure[1]->setText( i18nc(myResizeString, r.width(), r.height(), number(dx), number(dy) ) );

            // calc width for bottomright element, superfluous otherwise
            dx = r.right() - r2.right();
            dy = r.bottom() - r2.bottom();
        } else
            myMeasure[1]->setText( i18nc(myCoordString_0, number(dx), number(dy) ) );
        const int cdx = myMeasure[1]->geometry().width() / 2 + 3; // "3" = 6/2 is magic number because
        const int cdy = myMeasure[1]->geometry().height() / 2 + 3; // the unstyled effectframe has 5px padding
        center = QPoint(qMax(center.x(), screen.x() + cdx),
                        qMax(center.y(), screen.y() + cdy));
        center = QPoint(qMin(center.x(), screen.right() - cdx),
                        qMin(center.y(), screen.bottom() - cdy));
        myMeasure[1]->setPosition(center);

        // lower right ----------------------
        if (w->isUserResize())
            myMeasure[2]->setText( i18nc(myCoordString_1, r.right(), r.bottom(), number(dx), number(dy) ) );
        else
            myMeasure[2]->setText( i18nc(myCoordString_0, r.right(), r.bottom() ) );
        pos = expandedGeometry.bottomRight();
        pos = QPoint(qMin(pos.x(), screen.right()), qMin(pos.y(), screen.bottom()));
        myMeasure[2]->setPosition(pos - QPoint(6,6));  // "6" is magic number because the unstyled effectframe has 5px padding

        myExtraDirtyArea |= myMeasure[0]->geometry();
        myExtraDirtyArea |= myMeasure[1]->geometry();
        myExtraDirtyArea |= myMeasure[2]->geometry();
        myExtraDirtyArea.adjust(-6,-6,6,6);

        if (myExtraDirtyArea.isValid())
            effects->addRepaint(myExtraDirtyArea);
    }
}

bool WindowGeometry::isActive() const
{
    return iAmActive;
}

} // namespace KWin
