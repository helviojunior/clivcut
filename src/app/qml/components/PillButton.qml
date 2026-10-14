import QtQuick
import QtQuick.Controls
import Clivcut

// Botão de ação com ícone e texto; "primary" usa a cor de destaque.
AbstractButton {
    id: root
    property string iconName: ""
    property bool primary: false
    readonly property color ink: primary ? Theme.accentInk : Theme.text

    implicitWidth: row.implicitWidth + 28
    implicitHeight: 36
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    background: Rectangle {
        radius: 8
        color: root.primary ? (root.down ? Theme.accentDim : Theme.accent)
                            : (root.down ? Theme.surfaceHi : (root.hovered ? Theme.surface : Theme.panel))
        border.color: root.primary ? "transparent" : (root.visualFocus ? Theme.accent : Theme.border)
    }
    contentItem: Item {
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8
            Icon {
                visible: root.iconName !== ""
                anchors.verticalCenter: parent.verticalCenter
                name: root.iconName
                size: 16
                color: root.ink
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.text
                color: root.ink
                font.pixelSize: Theme.fontNormal
                font.bold: root.primary
            }
        }
    }
}
