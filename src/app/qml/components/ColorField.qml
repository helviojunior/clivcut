import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Clivcut

// Seletor de cor: amostras rápidas + diálogo completo.
RowLayout {
    id: root
    property string label
    property color color: "white"
    property bool allowNone: false
    property bool isNone: false
    signal picked(string color)

    Layout.fillWidth: true
    spacing: 6

    Text {
        text: root.label
        color: Theme.textDim
        font.pixelSize: Theme.fontSmall
        Layout.preferredWidth: 78
    }
    Repeater {
        model: ["#ffffff", "#000000", "#ffe14d", "#ff5c6c", "#22d3c5", "#3b82f6", "#a855f7"]
        delegate: Rectangle {
            required property string modelData
            width: 18
            height: 18
            radius: 9
            color: modelData
            border.color: !root.isNone && Qt.colorEqual(root.color, modelData) ? Theme.accent : Theme.border
            border.width: 2
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.picked(parent.modelData)
            }
        }
    }
    Rectangle {
        width: 18
        height: 18
        radius: 9
        color: root.isNone ? "transparent" : root.color
        border.color: Theme.textDim
        Text {
            anchors.centerIn: parent
            text: "+"
            color: Theme.text
            font.pixelSize: 12
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: dialog.open()
        }
    }
    Text {
        visible: root.allowNone
        text: qsTr("None", "no color")
        color: root.isNone ? Theme.accent : Theme.textDim
        font.pixelSize: Theme.fontSmall
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.picked("")
        }
    }
    Item { Layout.fillWidth: true }

    ColorDialog {
        id: dialog
        selectedColor: root.color
        options: ColorDialog.ShowAlphaChannel
        onAccepted: root.picked(selectedColor.toString())
    }
}
