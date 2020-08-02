/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "colorpicker.h"
#include <kwinglutils.h>
#include <kwinglutils_funcs.h>
#include <QDBusConnection>
#include <KLocalizedString>
#include <QDBusMetaType>

Q_DECLARE_METATYPE(QColor)

QDBusArgument &operator<< (QDBusArgument &argument, const QColor &color)
{
    argument.beginStructure();
    argument << color.rgba();
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, QColor &color)
{
    argument.beginStructure();
    QRgb rgba;
    argument >> rgba;
    argument.endStructure();
    color = QColor::fromRgba(rgba);
    return argument;
}

namespace KWin
{

bool ColorPickerEffect::supported()
{
    return effects->isOpenGLCompositing();
}

ColorPickerEffect::ColorPickerEffect()
    : m_scheduledPosition(QPoint(-1, -1))
{
    qDBusRegisterMetaType<QColor>();
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/ColorPicker"), this, QDBusConnection::ExportScriptableContents);
}

ColorPickerEffect::~ColorPickerEffect() = default;

void ColorPickerEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    m_cachedOutputGeometry = data.outputGeometry();
    effects->paintScreen(mask, region, data);
}

void ColorPickerEffect::postPaintScreen()
{
    effects->postPaintScreen();

    if (m_scheduledPosition != QPoint(-1, -1) && (m_cachedOutputGeometry.isEmpty() || m_cachedOutputGeometry.contains(m_scheduledPosition))) {
        uint8_t data[3];
        const QRect geo = GLRenderTarget::virtualScreenGeometry();
        const QPoint screenPosition(m_scheduledPosition.x() - geo.x(), m_scheduledPosition.y() - geo.y());
        const QPoint texturePosition(screenPosition.x() * GLRenderTarget::virtualScreenScale(), (geo.height() - screenPosition.y()) * GLRenderTarget::virtualScreenScale());

        glReadnPixels(texturePosition.x(), texturePosition.y(), 1, 1, GL_RGB, GL_UNSIGNED_BYTE, 3, data);
        QDBusConnection::sessionBus().send(m_replyMessage.createReply(QColor(data[0], data[1], data[2])));
        m_picking = false;
        m_scheduledPosition = QPoint(-1, -1);
    }
}

QColor ColorPickerEffect::pick()
{
    if (!calledFromDBus()) {
        return QColor();
    }
    if (m_picking) {
        sendErrorReply(QDBusError::Failed, "Color picking is already in progress");
        return QColor();
    }
    m_picking = true;
    m_replyMessage = message();
    setDelayedReply(true);
    showInfoMessage();
    effects->startInteractivePositionSelection(
        [this] (const QPoint &p) {
            hideInfoMessage();
            if (p == QPoint(-1, -1)) {
                // error condition
                QDBusConnection::sessionBus().send(m_replyMessage.createErrorReply(QStringLiteral("org.kde.kwin.ColorPicker.Error.Cancelled"), "Color picking got cancelled"));
                m_picking = false;
            } else {
                m_scheduledPosition = p;
                effects->addRepaintFull();
            }
        }
    );
    return QColor();
}

void ColorPickerEffect::showInfoMessage()
{
    effects->showOnScreenMessage(i18n("Select a position for color picking with left click or enter.\nEscape or right click to cancel."), QStringLiteral("color-picker"));
}

void ColorPickerEffect::hideInfoMessage()
{
    effects->hideOnScreenMessage();
}

bool ColorPickerEffect::isActive() const
{
    return m_picking && ((m_scheduledPosition != QPoint(-1, -1))) && !effects->isScreenLocked();
}

} // namespace
