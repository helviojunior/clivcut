import QtQuick
import QtQuick.Controls
import Clivcut

// Bloco clicável usado nas grades de presets (texto, transições, animações).
AbstractButton {
    id: root
    property string glyph: ""
    property string caption: ""
    property bool selected: false
    property color tint: Theme.surfaceHi

    implicitWidth: 92
    implicitHeight: 78
    hoverEnabled: true
    focusPolicy: Qt.NoFocus

    background: Rectangle {
        radius: Theme.radius
        color: root.hovered ? Theme.surfaceHi : Theme.surface
        border.color: root.selected ? Theme.accent : (root.hovered ? Theme.border : "transparent")
        border.width: 2
    }
    contentItem: Column {
        spacing: 6
        topPadding: 8
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: root.width - 20
            height: 36
            radius: 4
            color: root.tint
            Text {
                anchors.centerIn: parent
                text: root.glyph
                color: Theme.text
                font.pixelSize: 18
            }
        }
        Text {
            width: root.width - 8
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
            text: root.caption
            color: root.selected ? Theme.accent : Theme.textDim
            font.pixelSize: Theme.fontSmall
            elide: Text.ElideRight
        }
    }
}
