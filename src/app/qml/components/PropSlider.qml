import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Clivcut

// Linha de propriedade: rótulo, slider, valor editável e botão de keyframe (◆).
RowLayout {
    id: root
    property string label
    property real value: 0
    property real from: 0
    property real to: 1
    property real stepSize: 0.01
    property int decimals: 2
    property real displayScale: 1      // ex.: 100 para mostrar porcentagem
    property string suffix: ""
    property bool keyframable: false
    property bool hasKeyframes: false
    property bool keyframeHere: false
    signal moved(real value)
    signal keyframeToggled()

    Layout.fillWidth: true
    spacing: 8

    Text {
        text: root.label
        color: Theme.textDim
        font.pixelSize: Theme.fontSmall
        Layout.preferredWidth: 78
        elide: Text.ElideRight
    }
    Slider {
        id: slider
        Layout.fillWidth: true
        from: root.from
        to: root.to
        stepSize: root.stepSize
        value: root.value
        focusPolicy: Qt.NoFocus
        onMoved: root.moved(value)

        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth
            height: 3
            radius: 2
            color: Theme.surfaceHi
            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                radius: 2
                color: Theme.accent
            }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 12
            height: 12
            radius: 6
            color: slider.pressed ? Theme.accent : Theme.text
        }
    }
    TextField {
        id: field
        Layout.preferredWidth: 58
        horizontalAlignment: Text.AlignRight
        color: Theme.text
        font.pixelSize: Theme.fontSmall
        selectByMouse: true
        text: activeFocus ? text : (root.value * root.displayScale).toFixed(root.decimals) + root.suffix
        background: Rectangle {
            radius: 4
            color: Theme.surface
            border.color: field.activeFocus ? Theme.accent : "transparent"
        }
        onEditingFinished: {
            const v = parseFloat(text.replace(",", ".").replace(root.suffix, ""));
            if (!isNaN(v))
                root.moved(Math.max(root.from, Math.min(root.to, v / root.displayScale)));
            focus = false;
        }
    }
    Text {
        visible: root.keyframable
        text: root.keyframeHere ? "◆" : "◇"
        color: root.keyframeHere ? Theme.accent : (root.hasKeyframes ? Theme.text : Theme.textFaint)
        font.pixelSize: 15
        Layout.preferredWidth: 16
        MouseArea {
            anchors.fill: parent
            anchors.margins: -4
            cursorShape: Qt.PointingHandCursor
            onClicked: root.keyframeToggled()
        }
        ToolTip.visible: kfHover.hovered
        ToolTip.text: root.keyframeHere ? qsTr("Remover keyframe") : qsTr("Adicionar keyframe no cursor")
        HoverHandler { id: kfHover }
    }
}
