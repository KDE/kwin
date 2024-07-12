#include "xwaylandeiscontext.h"

#include "options.h"

#include <KLocalizedString>
#include <KMessageDialog>
#include <KStandardGuiItem>

namespace KWin
{

XWaylandEisContext::XWaylandEisContext(EisBackend *backend)
    : EisContext(backend, {EIS_DEVICE_CAP_POINTER | EIS_DEVICE_CAP_POINTER_ABSOLUTE | EIS_DEVICE_CAP_KEYBOARD | EIS_DEVICE_CAP_TOUCH | EIS_DEVICE_CAP_SCROLL | EIS_DEVICE_CAP_BUTTON})
    , socketName(qgetenv("XDG_RUNTIME_DIR") + QByteArrayLiteral("/kwin-xwayland-eis-socket.") + QByteArray::number(getpid()))
{
    eis_setup_backend_socket(m_eisContext, socketName.constData());
}

void XWaylandEisContext::connectionRequested(eis_client *client)
{
    if (options->xwaylandEisNoPrompt()) {
        connectToClient(client);
        return;
    }
    auto dialog = new KMessageDialog(KMessageDialog::QuestionTwoActions, i18nc("", "%1 wants to control the pointer and keyboard", eis_client_get_name(client)));
    dialog->setCaption(QStringLiteral("Remote control requested"));
    dialog->setIcon(QIcon::fromTheme(QStringLiteral("krfb")));
    KGuiItem allow(i18nc("@action:button", "Allow"), QStringLiteral("dialog-ok"));
    dialog->setButtons(allow, KStandardGuiItem::cancel());
    QObject::connect(dialog, &QDialog::finished, [client, this](int result) {
        if (result == KMessageDialog::PrimaryAction) {
            connectToClient(client);
        } else {
            eis_client_disconnect(client);
        }
    });
    dialog->show();
}

}
