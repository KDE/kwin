/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "colorpicker.h"
#include "libkwineffects/glutils.h"
#include "libkwineffects/glutils_funcs.h"
#include "libkwineffects/rendertarget.h"
#include "libkwineffects/renderviewport.h"
#include <KLocalizedString>
#include <QDBusConnection>
#include <QDBusMetaType>

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

void ColorPickerEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const QRegion &region, EffectScreen *screen)
{
    effects->paintScreen(renderTarget, viewport, mask, region, screen);

    const QRectF geo = viewport.renderRect();
    if (m_scheduledPosition != QPoint(-1, -1) && exclusiveContains(geo, m_scheduledPosition)) {
        uint8_t data[4];
        constexpr GLsizei PIXEL_SIZE = 1;
        const QPoint texturePosition = viewport.mapToRenderTarget(m_scheduledPosition).toPoint();

        glReadnPixels(texturePosition.x(), renderTarget.size().height() - texturePosition.y() - PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, GL_RGBA, GL_UNSIGNED_BYTE, 4, data);
        QVector3D sRGB = renderTarget.colorDescription().mapTo(QVector3D(data[0] / 255.0, data[1] / 255.0, data[2] / 255.0), ColorDescription::sRGB);
        QDBusConnection::sessionBus().send(m_replyMessage.createReply(QColor(255 * sRGB.x(), 255 * sRGB.y(), 255 * sRGB.z())));
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
        [this](const QPointF &p) {
            hideInfoMessage();
            if (p == QPointF(-1, -1)) {
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

#include "moc_colorpicker.cpp"
