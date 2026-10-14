import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Clivcut

// Gráfico de keyframes de uma propriedade: valor × tempo do clipe.
// Arraste keyframes (tempo e valor), escolha a suavização de cada trecho
// e ajuste curvas Bézier pelas alças.
ColumnLayout {
    id: root
    property string clipId
    property string prop
    property real duration: 1       // µs
    property real playheadU: -1     // 0..1 ou -1
    property int revision: Project.revision

    property var kfs: []
    property var samples: []
    property int selected: -1

    function reload() {
        if (clipId === "" || prop === "") {
            kfs = [];
            samples = [];
            return;
        }
        kfs = Project.keyframes(clipId, prop);
        samples = Project.sampleKeyframes(clipId, prop, 160);
        if (selected >= kfs.length)
            selected = kfs.length - 1;
        canvas.requestPaint();
    }
    onClipIdChanged: { selected = -1; reload(); }
    onPropChanged: { selected = -1; reload(); }
    onRevisionChanged: reload()
    onPlayheadUChanged: canvas.requestPaint()

    // Faixa de valores com folga.
    readonly property real vMin: {
        let m = Infinity;
        for (const v of samples) m = Math.min(m, v);
        for (const k of kfs) m = Math.min(m, k.v);
        return m === Infinity ? 0 : m;
    }
    readonly property real vMax: {
        let m = -Infinity;
        for (const v of samples) m = Math.max(m, v);
        for (const k of kfs) m = Math.max(m, k.v);
        return m === -Infinity ? 1 : m;
    }
    readonly property real lo: vMax - vMin < 1e-6 ? vMin - 1 : vMin - (vMax - vMin) * 0.15
    readonly property real hi: vMax - vMin < 1e-6 ? vMax + 1 : vMax + (vMax - vMin) * 0.15

    spacing: 6

    Item {
        id: plot
        Layout.fillWidth: true
        implicitHeight: 180
        readonly property real pad: 10
        function xOf(t) { return pad + (t / Math.max(1, root.duration)) * (width - 2 * pad) }
        function yOf(v) { return pad + (1 - (v - root.lo) / (root.hi - root.lo)) * (height - 2 * pad) }
        function tOf(x) { return Math.max(0, Math.min(root.duration, (x - pad) / (width - 2 * pad) * root.duration)) }
        function vOf(y) { return root.lo + (1 - (y - pad) / (height - 2 * pad)) * (root.hi - root.lo) }
        onWidthChanged: canvas.requestPaint()

        Rectangle { anchors.fill: parent; radius: Theme.radius; color: Theme.surface }

        Canvas {
            id: canvas
            anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                ctx.strokeStyle = "#22ffffff";
                for (let i = 1; i < 4; ++i) {
                    const y = plot.pad + i * (height - 2 * plot.pad) / 4;
                    ctx.beginPath(); ctx.moveTo(plot.pad, y); ctx.lineTo(width - plot.pad, y); ctx.stroke();
                }
                if (root.playheadU >= 0) {
                    const x = plot.xOf(root.playheadU * root.duration);
                    ctx.strokeStyle = "#ffffff";
                    ctx.beginPath(); ctx.moveTo(x, plot.pad); ctx.lineTo(x, height - plot.pad); ctx.stroke();
                }
                if (root.samples.length < 2)
                    return;
                ctx.strokeStyle = Theme.accent;
                ctx.lineWidth = 2;
                ctx.beginPath();
                for (let i = 0; i < root.samples.length; ++i) {
                    const x = plot.xOf(root.duration * i / (root.samples.length - 1));
                    const y = plot.yOf(root.samples[i]);
                    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
                }
                ctx.stroke();
                // Alças da Bézier do trecho selecionado.
                const s = root.selected;
                if (s >= 0 && s + 1 < root.kfs.length && root.kfs[s].ease === "bezier") {
                    const a = root.kfs[s], b = root.kfs[s + 1];
                    ctx.strokeStyle = Theme.warning;
                    ctx.lineWidth = 1;
                    ctx.beginPath();
                    ctx.moveTo(plot.xOf(a.t), plot.yOf(a.v));
                    ctx.lineTo(plot.xOf(a.t + a.bx1 * (b.t - a.t)), plot.yOf(a.v + a.by1 * (b.v - a.v)));
                    ctx.moveTo(plot.xOf(b.t), plot.yOf(b.v));
                    ctx.lineTo(plot.xOf(a.t + a.bx2 * (b.t - a.t)), plot.yOf(a.v + a.by2 * (b.v - a.v)));
                    ctx.stroke();
                }
            }
        }

        // Keyframes
        Repeater {
            model: root.kfs
            Rectangle {
                id: kf
                required property var modelData
                required property int index
                property real dragT: modelData.t
                x: plot.xOf(modelData.t) - 6
                y: plot.yOf(modelData.v) - 6
                width: 12
                height: 12
                rotation: 45
                color: root.selected === index ? Theme.warning : Theme.text
                border.color: Theme.bg
                border.width: 2
                ToolTip.visible: ma.pressed || ma.containsMouse
                ToolTip.text: Project.formatTime(modelData.t) + " · " + modelData.v.toFixed(2)
                MouseArea {
                    id: ma
                    anchors.fill: parent
                    anchors.margins: -5
                    hoverEnabled: true
                    preventStealing: true
                    cursorShape: Qt.SizeAllCursor
                    onPressed: {
                        root.selected = kf.index;
                        kf.dragT = kf.modelData.t;
                        canvas.requestPaint();
                    }
                    onPositionChanged: (m) => {
                        if (!pressed)
                            return;
                        const p = mapToItem(plot, m.x, m.y);
                        const t = plot.tOf(p.x), v = plot.vOf(p.y);
                        Project.moveKeyframe(root.clipId, root.prop, kf.dragT, t, v);
                        kf.dragT = t;
                    }
                }
            }
        }

        // Alças da Bézier (trecho a partir do keyframe selecionado).
        Repeater {
            model: root.selected >= 0 && root.selected + 1 < root.kfs.length && root.kfs[root.selected].ease === "bezier" ? [1, 2] : []
            Rectangle {
                id: bh
                required property int modelData
                readonly property var a: root.kfs[root.selected]
                readonly property var b: root.kfs[root.selected + 1]
                readonly property real bx: modelData === 1 ? a.bx1 : a.bx2
                readonly property real by: modelData === 1 ? a.by1 : a.by2
                x: plot.xOf(a.t + bx * (b.t - a.t)) - 5
                y: plot.yOf(a.v + by * (b.v - a.v)) - 5
                width: 10
                height: 10
                radius: 5
                color: Theme.warning
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -5
                    preventStealing: true
                    cursorShape: Qt.SizeAllCursor
                    onPositionChanged: (m) => {
                        if (!pressed)
                            return;
                        const p = mapToItem(plot, m.x, m.y);
                        const span = bh.b.t - bh.a.t, dv = bh.b.v - bh.a.v;
                        const nx = Math.max(0, Math.min(1, (plot.tOf(p.x) - bh.a.t) / Math.max(1, span)));
                        const ny = Math.abs(dv) < 1e-9 ? (bh.modelData === 1 ? bh.a.by1 : bh.a.by2) : (plot.vOf(p.y) - bh.a.v) / dv;
                        if (bh.modelData === 1)
                            Project.setKeyframeBezier(root.clipId, root.prop, bh.a.t, nx, ny, bh.a.bx2, bh.a.by2);
                        else
                            Project.setKeyframeBezier(root.clipId, root.prop, bh.a.t, bh.a.bx1, bh.a.by1, nx, ny);
                    }
                }
            }
        }
    }

    // Suavização do trecho que começa no keyframe selecionado.
    Text {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        color: Theme.textFaint
        font.pixelSize: Theme.fontSmall
        text: root.selected < 0 ? qsTr("Click a keyframe (◆) to pick the curve to the next one. Drag to change time and value.")
              : root.selected + 1 >= root.kfs.length ? qsTr("Last keyframe: the curve applies between it and the previous one — select the previous one.")
              : qsTr("Curve from %1 to the next keyframe:").arg(Project.formatTime(root.kfs[root.selected].t))
    }
    Flow {
        visible: root.selected >= 0 && root.selected + 1 < root.kfs.length
        Layout.fillWidth: true
        spacing: 4
        Repeater {
            model: [
                { e: "linear", t: qsTr("Linear") }, { e: "easeIn", t: qsTr("In") },
                { e: "easeOut", t: qsTr("Out") }, { e: "easeInOut", t: qsTr("Smooth") },
                { e: "hold", t: qsTr("Hold") }, { e: "bezier", t: qsTr("Custom") }
            ]
            Rectangle {
                required property var modelData
                readonly property bool on: root.selected >= 0 && root.selected < root.kfs.length && root.kfs[root.selected].ease === modelData.e
                width: easeText.implicitWidth + 16
                height: 24
                radius: 12
                color: on ? Theme.surfaceHi : "transparent"
                border.color: on ? Theme.accent : Theme.border
                Text {
                    id: easeText
                    anchors.centerIn: parent
                    text: parent.modelData.t
                    color: Theme.text
                    font.pixelSize: Theme.fontSmall
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Project.setKeyframeEasing(root.clipId, root.prop, root.kfs[root.selected].t, parent.modelData.e)
                }
            }
        }
    }
    RowLayout {
        visible: root.kfs.length > 1
        Text { text: qsTr("All:"); color: Theme.textDim; font.pixelSize: Theme.fontSmall }
        Repeater {
            model: [{ e: "linear", t: qsTr("Linear") }, { e: "easeInOut", t: qsTr("Smooth") }]
            Button {
                required property var modelData
                text: modelData.t
                focusPolicy: Qt.NoFocus
                onClicked: Project.setEasingForAll(root.clipId, root.prop, modelData.e)
            }
        }
    }
}
