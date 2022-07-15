import QtQml.Models 2.15
import QtQuick 2.15

 ListModel
 {
    id: model

    property list<QtObject> desktops: [
        QtObject {
            property int desktop: 1
            property string name: "Desktop"
        }
    ]

    Component.onCompleted: {
        model.append({"desktop": desktops[0]})
    }

 }
