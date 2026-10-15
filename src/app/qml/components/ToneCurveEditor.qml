import QtQuick
import Clivcut

// Curva de tons: entrada (x) → saída (y), 0..1. Clique adiciona um ponto, arrastar move,
// duplo clique remove. Sem pontos = identidade (cantos (0,0) e (1,1)).
Item {
    id: root
    property var points: []              // [{x, y}]
    property color lineColor: Theme.text
    signal edited(var points)

    implicitHeight: width

    readonly property real pad: 8
    readonly property real plot: Math.min(width, height) - 2 * pad
    readonly property var shown: points.length > 0 ? points : [{ x: 0, y: 0 }, { x: 1, y: 1 }]
    property int dragging: -1

    function px(x) { return pad + x * plot }
    function py(y) { return pad + (1 - y) * plot }
    function vx(p) { return Math.max(0, Math.min(1, (p - pad) / plot)) }
    function vy(p) { return Math.max(0, Math.min(1, 1 - (p - pad) / plot)) }
    function copy() { return shown.map(p => ({ x: p.x, y: p.y })) }

    // Mesma interpolação monotônica do LUT (Fritsch–Carlson), para o desenho bater com o resultado.
    function curveAt(pts, x) {
        const p = pts.slice().sort((a, b) => a.x - b.x);
        if (p[0].x > 0) p.unshift({ x: 0, y: 0 });
        if (p[p.length - 1].x < 1) p.push({ x: 1, y: 1 });
        const n = p.length, d = [], m = [];
        for (let i = 0; i + 1 < n; ++i) d.push((p[i + 1].y - p[i].y) / Math.max(1e-9, p[i + 1].x - p[i].x));
        m[0] = d[0]; m[n - 1] = d[n - 2];
        for (let i = 1; i + 1 < n; ++i) m[i] = d[i - 1] * d[i] <= 0 ? 0 : (d[i - 1] + d[i]) / 2;
        for (let i = 0; i + 1 < n; ++i) {
            if (d[i] === 0) { m[i] = m[i + 1] = 0; continue; }
            const a = m[i] / d[i], b = m[i + 1] / d[i], h = a * a + b * b;
            if (h > 9) { const t = 3 / Math.sqrt(h); m[i] = t * a * d[i]; m[i + 1] = t * b * d[i]; }
        }
        let s = 0;
        while (s + 2 < n && x > p[s + 1].x) ++s;
        const hgt = Math.max(1e-9, p[s + 1].x - p[s].x), t = Math.max(0, Math.min(1, (x - p[s].x) / hgt));
        const t2 = t * t, t3 = t2 * t;
        const y = (2 * t3 - 3 * t2 + 1) * p[s].y + (t3 - 2 * t2 + t) * hgt * m[s] + (-2 * t3 + 3 * t2) * p[s + 1].y + (t3 - t2) * hgt * m[s + 1];
        return Math.max(0, Math.min(1, y));
    }

    onShownChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onLineColorChanged: canvas.requestPaint()

    Rectangle {
        x: root.pad - 1; y: root.pad - 1
        width: root.plot + 2; height: root.plot + 2
        radius: 4
        color: Theme.surface
        border.color: Theme.border
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            // Grade em quartos e a diagonal neutra.
            ctx.strokeStyle = Theme.border;
            ctx.lineWidth = 1;
            for (let i = 1; i < 4; ++i) {
                ctx.beginPath(); ctx.moveTo(root.px(i / 4), root.py(0)); ctx.lineTo(root.px(i / 4), root.py(1)); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(root.px(0), root.py(i / 4)); ctx.lineTo(root.px(1), root.py(i / 4)); ctx.stroke();
            }
            ctx.strokeStyle = Theme.textFaint;
            ctx.setLineDash([3, 3]);
            ctx.beginPath(); ctx.moveTo(root.px(0), root.py(0)); ctx.lineTo(root.px(1), root.py(1)); ctx.stroke();
            ctx.setLineDash([]);
            // Curva
            ctx.strokeStyle = root.lineColor;
            ctx.lineWidth = 2;
            ctx.beginPath();
            for (let i = 0; i <= 64; ++i) {
                const x = i / 64, y = root.curveAt(root.shown, x);
                if (i === 0) ctx.moveTo(root.px(x), root.py(y)); else ctx.lineTo(root.px(x), root.py(y));
            }
            ctx.stroke();
            // Pontos
            for (const p of root.shown) {
                ctx.fillStyle = Theme.panel;
                ctx.strokeStyle = root.lineColor;
                ctx.beginPath();
                ctx.arc(root.px(p.x), root.py(p.y), 4.5, 0, Math.PI * 2);
                ctx.fill();
                ctx.stroke();
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        function nearest(mx, my) {
            let best = -1, bestD = 12 * 12;
            for (let i = 0; i < root.shown.length; ++i) {
                const dx = root.px(root.shown[i].x) - mx, dy = root.py(root.shown[i].y) - my;
                if (dx * dx + dy * dy < bestD) { bestD = dx * dx + dy * dy; best = i; }
            }
            return best;
        }
        onPressed: (m) => {
            let i = nearest(m.x, m.y);
            if (i < 0) {
                const pts = root.copy();
                pts.push({ x: root.vx(m.x), y: root.vy(m.y) });
                pts.sort((a, b) => a.x - b.x);
                i = pts.findIndex(p => p.x === root.vx(m.x));
                root.edited(pts);
            }
            root.dragging = i;
        }
        onPositionChanged: (m) => {
            if (root.dragging < 0 || !pressed)
                return;
            const pts = root.copy();
            const i = root.dragging;
            // Um ponto não passa dos vizinhos (a curva continua uma função).
            const lo = i > 0 ? pts[i - 1].x + 0.01 : 0, hi = i + 1 < pts.length ? pts[i + 1].x - 0.01 : 1;
            pts[i] = { x: Math.max(lo, Math.min(hi, root.vx(m.x))), y: root.vy(m.y) };
            root.edited(pts);
        }
        onReleased: root.dragging = -1
        onDoubleClicked: (m) => {
            const i = nearest(m.x, m.y);
            if (i < 0 || root.shown.length <= 2)
                return;
            const pts = root.copy();
            pts.splice(i, 1);
            root.edited(pts);
        }
    }
}
