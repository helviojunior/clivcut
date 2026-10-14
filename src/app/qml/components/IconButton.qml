import QtQuick
import QtQuick.Controls
import Clivcut

// Botão compacto: ícone SVG (ou texto), dica com atalho e estado ativo.
ToolButton {
    id: root
    property string iconName: ""
    property string glyph: ""       // compatibilidade: símbolo de texto
    property string tip: ""
    property string shortcut: ""
    property bool active: false
    property int iconSize: 17
    property color glyphColor: active ? Theme.accent : (enabled ? Theme.text : Theme.textFaint)

    implicitWidth: Math.max(32, contentItem.implicitWidth + 14)
    implicitHeight: 32
    focusPolicy: Qt.NoFocus
    hoverEnabled: true

    contentItem: Item {
        implicitWidth: root.iconName !== "" ? root.iconSize : label.implicitWidth
        implicitHeight: root.iconSize
        Icon {
            anchors.centerIn: parent
            visible: root.iconName !== ""
            name: root.iconName
            size: root.iconSize
            color: root.glyphColor
            opacity: root.enabled ? 1 : 0.45
        }
        Text {
            id: label
            anchors.centerIn: parent
            visible: root.iconName === ""
            text: root.glyph !== "" ? root.glyph : root.text
            color: root.glyphColor
            font.pixelSize: root.glyph !== "" ? 16 : Theme.fontNormal
        }
    }
    background: Rectangle {
        radius: Theme.radius
        color: root.down ? Theme.surfaceHi : (root.active ? Theme.accentSoft : (root.hovered ? Theme.surface : "transparent"))
    }

    ToolTip.visible: hovered && tip !== ""
    ToolTip.delay: 450
    ToolTip.text: shortcut !== "" ? tip + "   " + shortcut : tip
}
