
#include "tabbox/accessibility.h"
#include "tabbox/tabbox.h"
#include "main.h"
#include "abstract_client.h"

#include <qaccessible.h>

using namespace KWin;

namespace KWin {
namespace TabBox {

// When doing anything, the window needs to be activated:
//QAccessible::State state;
//state.active = true;
//QAccessibleStateChangeEvent event(this, state);
//QAccessible::updateAccessibility(&event);

// AND deactivated when hiding the tabbox

TabBoxAccessible::TabBoxAccessible(TabBox *parent) : QAccessibleObject(parent) {
    auto *appInterface = QAccessible::queryAccessibleInterface(KWin::kwinApp());
    KWinAccessibleApplication *appAccessible = static_cast<KWinAccessibleApplication*>(appInterface);
    appAccessible->addChildAccessible(this);
    qDebug() << Q_FUNC_INFO << "Created tab box accessible with object:" << parent;
}

QAccessibleInterface *TabBoxAccessible::parent() const {
    return QAccessible::queryAccessibleInterface(KWin::kwinApp());
}

QAccessibleInterface *TabBoxAccessible::focusChild() const
{
    if (!tabbox() || !tabbox()->currentClient()) {
        return nullptr;
    }
    auto *client = tabbox()->currentClient();
    auto *iface = QAccessible::queryAccessibleInterface(client);
    if (!iface) {
        auto *iface = new ClientAccessible(client, this);
        QAccessible::registerAccessibleInterface(iface);
    }
    return iface;
}

QAccessibleInterface *TabBoxAccessible::child(int index) const
{
    if (!tabbox()) {
        return nullptr;
    }

    if (index >= 0 && index < tabbox()->currentClientList().size()) {
        auto *client = tabbox()->currentClientList().at(index);
        auto *iface = QAccessible::queryAccessibleInterface(client);

        if (!iface) {
            auto *iface = new ClientAccessible(client, this);
            QAccessible::registerAccessibleInterface(iface);
        }
        return iface;
    }

    return nullptr;
}

int TabBoxAccessible::childCount() const
{
    if (tabbox()) {
//    if (tabbox()->isShown()) {
      return tabbox()->currentClientList().size();
//    }
    }
    return 0;
}

int TabBoxAccessible::indexOfChild(const QAccessibleInterface *child) const
{
    if (!tabbox()) {
        return -1;
    }
    for (int i = 0; i < tabbox()->currentClientList().size(); ++i) {
        if (tabbox()->currentClientList().at(i) == child->object()) {
            return i;
        }
    }
    return -1;
}

QString TabBoxAccessible::text(QAccessible::Text t) const
{
    if (t == QAccessible::Name) {
        return QStringLiteral("Window Switcher");
    }
    return QString();
}

QAccessible::Role TabBoxAccessible::role() const
{
    return QAccessible::Window;
}

QAccessible::State TabBoxAccessible::state() const
{
    QAccessible::State s;
    s.focusable = true;
    if (tabbox() && tabbox()->isShown()) {
        s.active = true;
        s.focused = true;
    } else {
        s.invisible = true;
    }
    return s;
}

void TabBoxAccessible::show() const
{
    QAccessible::State state;
    state.active = true;
    QAccessibleStateChangeEvent event(object(), state);
    QAccessible::updateAccessibility(&event);
}

void TabBoxAccessible::hide() const
{
    QAccessible::State state;
    state.active = false;
    QAccessibleStateChangeEvent event(object(), state);
    QAccessible::updateAccessibility(&event);
}

TabBox *TabBoxAccessible::tabbox() const
{
    return static_cast<TabBox *>(object());
}

KWinAccessibleApplication::KWinAccessibleApplication(Application *app)
{
    Q_UNUSED(app);  // FIXME remove parameter?
}

int KWinAccessibleApplication::childCount() const {
    int count = QAccessibleApplication::childCount();
    if (m_child) {
        ++count;
    }
    return count;
}

QAccessibleInterface *KWinAccessibleApplication::child(int index) const {

    qDebug() << Q_FUNC_INFO << index << "ChildCount" << QAccessibleApplication::childCount();

    if (index < QAccessibleApplication::childCount()) {
        return QAccessibleApplication::child(index);
    } else if (index == QAccessibleApplication::childCount()) {
        qDebug() << "Getting VIRTUAL CHILD";
        return m_child;
    }
//    qDebug() << "Returning nullptr as child";
    return nullptr;
}

int KWinAccessibleApplication::indexOfChild(const QAccessibleInterface *child) const {

    qDebug() << Q_FUNC_INFO;

    if (child == m_child) {
        return childCount() - 1;
    }
    return QAccessibleApplication::indexOfChild(child);
}

ClientAccessible::ClientAccessible(AbstractClient *client, const QAccessibleInterface *parentInterface)
    : QAccessibleObject(client)
{
    m_parent = const_cast<QAccessibleInterface*>(parentInterface);
}

QAccessibleInterface *ClientAccessible::parent() const
{
    return m_parent;
}

QString ClientAccessible::text(QAccessible::Text t) const
{
    auto *aClient = client();
    if (aClient) {
        if (t == QAccessible::Name) {
            return aClient->caption();
        }
    }
    return QString();
//    return "Client Accessible of KWIN";
}

QAccessible::Role ClientAccessible::role() const
{
    return QAccessible::Button;  // FIXME
}

QAccessible::State ClientAccessible::state() const
{
    QAccessible::State s; // FIXME
    s.focusable = true;
    s.focused = true;
    return s;
}

KWin::AbstractClient *ClientAccessible::client() const
{
    return static_cast<KWin::AbstractClient *>(object());
}

}
}
