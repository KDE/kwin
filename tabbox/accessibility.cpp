
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

QAccessibleInterface *TabBoxAccessible::child(int index) const
{
    if (!tabbox()) {
        return nullptr;
    }

    if (index >= 0 && index < tabbox()->currentClientList().size()) {
        // FIXME: go through factory to get the cache used
        return new ClientAccessible(tabbox()->currentClientList().at(index));
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

int TabBoxAccessible::indexOfChild(const QAccessibleInterface *) const
{
    return -1;
}

QString TabBoxAccessible::text(QAccessible::Text t) const
{
    QString window;
    if (tabbox() && tabbox()->currentClient()) {
        window = tabbox()->currentClient()->caption();
    }

    if (t == QAccessible::Name) {
        return QStringLiteral("Window Switcher: ") + window;
    } else if (t == QAccessible::Description) {

        return QStringLiteral("KWIN THING description"); // FIXME
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
    QAccessibleStateChangeEvent event(object(), state());
    QAccessible::updateAccessibility(&event);
}

void TabBoxAccessible::hide() const
{
    QAccessibleStateChangeEvent event(object(), state());
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
//    qDebug() << Q_FUNC_INFO << index << "CC" << QAccessibleApplication::childCount();
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
    if (child == m_child) {
        return childCount() - 1;
    }
    return QAccessibleApplication::indexOfChild(child);
}

ClientAccessible::ClientAccessible(AbstractClient *client)
    : QAccessibleObject(client)
{
}

QAccessibleInterface *ClientAccessible::parent() const
{
    // FIXME
    return nullptr;
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
}

QAccessible::Role ClientAccessible::role() const
{
    return QAccessible::Button;  // FIXME
}

QAccessible::State ClientAccessible::state() const
{
    QAccessible::State s; // FIXME
    return s;
}

KWin::AbstractClient *ClientAccessible::client() const
{
    return static_cast<KWin::AbstractClient *>(object());
}

}
}
