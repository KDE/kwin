import QtQml.Models 2.15
import QtQuick 2.15

ListModel
{
    id: model

    Component.onCompleted: {
        model.append(
            {
                "client" : Workspace.clients[0],
                "screen": 1
            }
        )
        model.append(
            {
                "client" : Workspace.clients[1],
                "screen": 1
            }
        )
    }
 }
