import QtQuick
import QtQuick.Layouts
import Clivcut

RowLayout {
    id: root
    property alias text: label.text
    property alias actionText: action.text
    signal actionClicked()
    Layout.fillWidth: true
    Layout.topMargin: 10

    Text {
        id: label
        color: Theme.text
        font.pixelSize: Theme.fontNormal
        font.bold: true
        Layout.fillWidth: true
    }
    Text {
        id: action
        visible: text !== ""
        color: Theme.textDim
        font.pixelSize: Theme.fontSmall
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.actionClicked()
        }
    }
}
