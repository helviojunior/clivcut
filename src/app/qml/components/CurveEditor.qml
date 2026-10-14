import QtQuick
import QtQuick.Controls
import Clivcut

// Editor de curva de velocidade: eixo X = posição no clipe (0..1),
// eixo Y = velocidade em escala logarítmica (0,1× embaixo, 10× em cima).
Item {
    id: root
    property var points: []          // [{u, v}]
    property real playheadU: -1      // posição do cursor no clipe (ou -1)
    signal edited(var points)

    implicitHeight: 170

    readonly property real padL: 34
    readonly property real padR: 8
    readonly property real padT: 8
    readonly property real padB: 8
    readonly property real plotW: width - padL - padR
    readonly property real plotH: height - padT - padB

    function xOf(u) { return padL + u * plotW }
    function yOf(v) { return padT + (1 - (Math.log(v) / Math.LN10 + 1) / 2) * plotH }
    function uOf(x) { return Math.max(0, Math.min(1, (x - padL) / plotW)) }
    function vOf(y) {
        const t = 1 - (y - padT) / plotH; // 0..1
        return Math.max(0.1, Math.min(10, Math.pow(10, t * 2 - 1)));
    }
    function copyPoints() { return root.points.map(p => ({ u: p.u, v: p.v })) }

    onPointsChanged: canvas.requestPaint()
    onPlayheadUChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius
        color: Theme.surface
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.font = "9px sans-serif";
            // Grade: 0,1× · 0,5× · 1× · 2× · 10×
            for (const v of [0.1, 0.5, 1, 2, 10]) {
                const y = root.yOf(v);
                ctx.strokeStyle = v === 1 ? "#55ffffff" : "#22ffffff";
                ctx.beginPath();
                ctx.moveTo(root.padL, y);
                ctx.lineTo(root.width - root.padR, y);
                ctx.stroke();
                ctx.fillStyle = Theme.textFaint;
                ctx.fillText(v + "×", 4, y + 3);
            }
            if (root.playheadU >= 0) {
                ctx.strokeStyle = "#ffffff";
                ctx.beginPath();
                ctx.moveTo(root.xOf(root.playheadU), root.padT);
                ctx.lineTo(root.xOf(root.playheadU), root.height - root.padB);
                ctx.stroke();
            }
            if (root.points.length < 2)
                return;
            // Curva (linear por partes em velocidade, desenhada em escala log).
            ctx.strokeStyle = Theme.accent;
            ctx.lineWidth = 2;
            ctx.beginPath();
            for (let i = 0; i < root.points.length - 1; ++i) {
                const a = root.points[i], b = root.points[i + 1];
                for (let s = 0; s <= 16; ++s) {
                    const t = s / 16;
                    const u = a.u + (b.u - a.u) * t, v = a.v + (b.v - a.v) * t;
                    if (i === 0 && s === 0)
                        ctx.moveTo(root.xOf(u), root.yOf(v));
                    else
                        ctx.lineTo(root.xOf(u), root.yOf(v));
                }
            }
            ctx.stroke();
        }
    }

    // Duplo clique no vazio: novo ponto.
    MouseArea {
        anchors.fill: parent
        onDoubleClicked: (m) => {
            const pts = root.copyPoints();
            pts.push({ u: root.uOf(m.x), v: root.vOf(m.y) });
            pts.sort((a, b) => a.u - b.u);
            root.edited(pts);
        }
    }

    Repeater {
        model: root.points
        Rectangle {
            id: handle
            required property var modelData
            required property int index
            readonly property bool isEnd: index === 0 || index === root.points.length - 1
            x: root.xOf(modelData.u) - width / 2
            y: root.yOf(modelData.v) - height / 2
            width: 12
            height: 12
            radius: 6
            color: drag.pressed ? Theme.accent : Theme.text
            border.color: Theme.bg
            border.width: 2

            ToolTip.visible: drag.pressed || drag.containsMouse
            ToolTip.text: modelData.v.toFixed(2) + "×"

            MouseArea {
                id: drag
                anchors.fill: parent
                anchors.margins: -5
                hoverEnabled: true
                cursorShape: Qt.SizeAllCursor
                preventStealing: true
                onPositionChanged: (m) => {
                    if (!pressed)
                        return;
                    const p = mapToItem(root, m.x, m.y);
                    const pts = root.copyPoints();
                    const i = handle.index;
                    // As pontas ficam em u=0 e u=1; os demais entre os vizinhos.
                    if (!handle.isEnd) {
                        const lo = pts[i - 1].u + 0.01, hi = pts[i + 1].u - 0.01;
                        pts[i].u = Math.max(lo, Math.min(hi, root.uOf(p.x)));
                    }
                    pts[i].v = root.vOf(p.y);
                    root.edited(pts);
                }
                onDoubleClicked: {
                    if (handle.isEnd || root.points.length <= 2)
                        return;
                    const pts = root.copyPoints();
                    pts.splice(handle.index, 1);
                    root.edited(pts);
                }
            }
        }
    }
}
