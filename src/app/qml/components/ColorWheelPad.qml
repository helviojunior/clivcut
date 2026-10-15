import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Clivcut

// Roda de cor (lift, gamma ou gain): arrastar o ponto empurra a cor naquela direção;
// a barra embaixo muda a luminância. Duplo clique na roda volta ao neutro.
ColumnLayout {
    id: root
    property string label
    property real wx: 0      // -1..1, direita = vermelho
    property real wy: 0      // -1..1, para cima
    property real luma: 0    // -1..1
    signal edited(real x, real y, real luma)

    spacing: 4

    Text {
        Layout.alignment: Qt.AlignHCenter
        text: root.label
        color: Theme.textDim
        font.pixelSize: Theme.fontSmall
    }

    Item {
        id: pad
        Layout.fillWidth: true
        Layout.preferredHeight: width
        readonly property real r: Math.min(width, height) / 2 - 3

        Canvas {
            id: wheel
            anchors.fill: parent
            onWidthChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                const cx = width / 2, cy = height / 2, r = pad.r;
                // Matizes no sentido anti-horário a partir do vermelho à direita (como no LUT).
                const g = ctx.createConicalGradient(cx, cy, 0);
                const stops = ["#ff4040", "#ffff40", "#40ff40", "#40ffff", "#4040ff", "#ff40ff", "#ff4040"];
                for (let i = 0; i < stops.length; ++i)
                    g.addColorStop(i / (stops.length - 1), stops[i]);
                ctx.fillStyle = g;
                ctx.beginPath();
                ctx.arc(cx, cy, r, 0, Math.PI * 2);
                ctx.fill();
                // Centro acinzentado: perto do meio, pouca cor.
                const rg = ctx.createRadialGradient(cx, cy, 0, cx, cy, r);
                rg.addColorStop(0, Theme.surfaceHi);
                rg.addColorStop(1, "transparent");
                ctx.fillStyle = rg;
                ctx.beginPath();
                ctx.arc(cx, cy, r, 0, Math.PI * 2);
                ctx.fill();
                ctx.strokeStyle = Theme.border;
                ctx.lineWidth = 1;
                ctx.stroke();
            }
        }
        Rectangle {
            width: 12; height: 12; radius: 6
            x: pad.width / 2 + root.wx * pad.r - 6
            y: pad.height / 2 - root.wy * pad.r - 6
            color: "transparent"
            border.color: "#ffffff"
            border.width: 2
            Rectangle { anchors.centerIn: parent; width: 4; height: 4; radius: 2; color: "#000000" }
        }
        MouseArea {
            anchors.fill: parent
            function apply(m) {
                let x = (m.x - pad.width / 2) / pad.r, y = -(m.y - pad.height / 2) / pad.r;
                const d = Math.hypot(x, y);
                if (d > 1) { x /= d; y /= d; }
                // Ímã no centro: fica fácil voltar ao neutro.
                if (Math.hypot(x, y) < 0.06) { x = 0; y = 0; }
                root.edited(x, y, root.luma);
            }
            onPressed: (m) => apply(m)
            onPositionChanged: (m) => { if (pressed) apply(m) }
            onDoubleClicked: root.edited(0, 0, 0)
        }
    }

    Slider {
        id: lumaSlider
        Layout.fillWidth: true
        from: -1; to: 1
        value: root.luma
        focusPolicy: Qt.NoFocus
        implicitHeight: 18
        onMoved: root.edited(root.wx, root.wy, value)
        // Trilho fino com o centro marcado (luminância neutra) e alça pequena.
        background: Item {
            x: lumaSlider.leftPadding
            y: lumaSlider.topPadding + lumaSlider.availableHeight / 2 - 2
            width: lumaSlider.availableWidth
            height: 4
            Rectangle { anchors.fill: parent; radius: 2; color: Theme.surfaceHi }
            Rectangle {
                readonly property real mid: parent.width / 2
                readonly property real pos: lumaSlider.visualPosition * parent.width
                x: Math.min(mid, pos)
                width: Math.abs(pos - mid)
                height: parent.height
                radius: 2
                color: Theme.accent
            }
            Rectangle { x: parent.width / 2 - 1; y: -2; width: 2; height: 8; color: Theme.textFaint }
        }
        handle: Rectangle {
            x: lumaSlider.leftPadding + lumaSlider.visualPosition * (lumaSlider.availableWidth - width)
            y: lumaSlider.topPadding + lumaSlider.availableHeight / 2 - height / 2
            width: 12
            height: 12
            radius: 6
            color: lumaSlider.pressed ? Theme.accent : Theme.text
        }
        ToolTip.visible: hovered || pressed
        ToolTip.text: qsTr("Luminance %1").arg(Math.round(value * 100))
    }
}
