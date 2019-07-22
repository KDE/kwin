/*
 * Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickView>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/pointergestures.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/seat.h>

using namespace KWayland::Client;

class PinchGesture : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(qreal scale READ scale NOTIFY scaleChanged)
    Q_PROPERTY(qreal progressScale READ progressScale NOTIFY progressScaleChanged)

public:
    explicit PinchGesture(QQuickItem *parent = nullptr);
    ~PinchGesture() override;

    qreal scale() const {
        return m_scale;
    }

    qreal progressScale() const {
        return m_progressScale;
    }

protected:
    void componentComplete() override;

Q_SIGNALS:
    void scaleChanged();
    void progressScaleChanged();

private:
    void initWayland();
    void setupGesture();

    Pointer *m_pointer = nullptr;
    PointerGestures *m_pointerGestures = nullptr;
    PointerPinchGesture *m_gesture = nullptr;

    qreal m_scale = 1.0;
    qreal m_progressScale = 1.0;
};

PinchGesture::PinchGesture(QQuickItem* parent)
    : QQuickItem(parent)
{
}

PinchGesture::~PinchGesture() = default;

void PinchGesture::componentComplete()
{
    QQuickItem::componentComplete();
    initWayland();
}

void PinchGesture::initWayland()
{
    auto c = ConnectionThread::fromApplication(this);
    Registry *r = new Registry(c);
    r->create(c);

    connect(r, &Registry::interfacesAnnounced, this,
        [this, r] {
            const auto gi = r->interface(Registry::Interface::PointerGesturesUnstableV1);
            if (gi.name == 0) {
                return;
            }
            m_pointerGestures = r->createPointerGestures(gi.name, gi.version, this);

            // now create seat
            const auto si = r->interface(Registry::Interface::Seat);
            if (si.name == 0) {
                return;
            }
            auto seat = r->createSeat(si.name, si.version, this);
            connect(seat, &Seat::hasKeyboardChanged, this,
                [this, seat] (bool hasPointer) {
                    if (hasPointer) {
                        m_pointer = seat->createPointer(this);
                        setupGesture();
                    } else {
                        delete m_pointer;
                        delete m_gesture;
                        m_pointer = nullptr;
                        m_gesture = nullptr;
                    }
                }
            );
        }
    );

    r->setup();
    c->roundtrip();
}

void PinchGesture::setupGesture()
{
    if (m_gesture || !m_pointerGestures || !m_pointer) {
        return;
    }
    m_gesture = m_pointerGestures->createPinchGesture(m_pointer, this);
    connect(m_gesture, &PointerPinchGesture::updated, this,
        [this] (const QSizeF &delta, qreal scale) {
            Q_UNUSED(delta)
            m_progressScale = scale;
            emit progressScaleChanged();
        }
    );
    connect(m_gesture, &PointerPinchGesture::ended, this,
        [this] {
            m_scale = m_scale * m_progressScale;
            m_progressScale = 1.0;
            emit scaleChanged();
            emit progressScaleChanged();
        }
    );
    connect(m_gesture, &PointerPinchGesture::cancelled, this,
        [this] {
            m_progressScale = 1.0;
            emit progressScaleChanged();
        }
    );
}


int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("wayland"));
    QGuiApplication app(argc, argv);

    qmlRegisterType<PinchGesture>("org.kde.kwin.tests", 1, 0, "PinchGesture");

    QQuickView view;
    view.setSource(QUrl::fromLocalFile(QStringLiteral(DIR) +QStringLiteral("/pointergesturestest.qml")));

    view.show();

    return app.exec();
}

#include "pointergesturestest.moc"
