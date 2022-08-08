/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "colorpicker.h"
#include <KLocalizedString>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <kwinglutils.h>
#include <kwinglutils_funcs.h>

Q_DECLARE_METATYPE(QColor)

QDBusArgument &operator<<(QDBusArgument &argument, const QColor &color)
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
    effects->paintScreen(mask, region, data);

    const QRect geo = effects->renderTargetRect();
    if (m_scheduledPosition != QPoint(-1, -1) && geo.contains(m_scheduledPosition)) {
        uint8_t data[4];
        constexpr GLsizei PIXEL_SIZE = 1;
        const QPoint screenPosition(m_scheduledPosition.x() - geo.x(), m_scheduledPosition.y() - geo.y());
        const QPoint texturePosition(screenPosition.x() * effects->renderTargetScale(), (geo.height() - screenPosition.y() - PIXEL_SIZE) * effects->renderTargetScale());

        glReadnPixels(texturePosition.x(), texturePosition.y(), PIXEL_SIZE, PIXEL_SIZE, GL_RGBA, GL_UNSIGNED_BYTE, 4, data);
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
        [this](const QPoint &p) {
            hideInfoMessage();
            if (p == QPoint(-1, -1)) {
                // error condition
                QDBusConnection::sessionBus().send(m_replyMessage.createErrorReply(QStringLiteral("org.kde.kwin.ColorPicker.Error.Cancelled"), "Color picking got cancelled"));
                m_picking = false;
            } else {
                m_scheduledPosition = p;
                effects->addRepaintFull();
            }
        });
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
