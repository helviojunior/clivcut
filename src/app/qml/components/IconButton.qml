import QtQuick
import QtQuick.Controls
import Clivcut

// Botão compacto com glifo, dica e estado ligado/desligado.
ToolButton {
    id: root
    property string glyph: ""
    property string tip: ""
    property bool active: false
    property color glyphColor: active ? Theme.accent : (enabled ? Theme.text : Theme.textFaint)

    implicitWidth: Math.max(30, contentItem.implicitWidth + 14)
    implicitHeight: 30
    focusPolicy: Qt.NoFocus
    hoverEnabled: true

    contentItem: Text {
        text: root.glyph !== "" ? root.glyph : root.text
        color: root.glyphColor
        font.pixelSize: root.glyph !== "" ? 16 : Theme.fontNormal
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: Theme.radius
        color: root.down ? Theme.surfaceHi : (root.hovered || root.active ? Theme.surface : "transparent")
    }

    ToolTip.visible: hovered && tip !== ""
    ToolTip.delay: 500
    ToolTip.text: tip
}
