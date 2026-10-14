import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Clivcut

Rectangle {
    id: root
    color: Theme.panel

    // --- escala de tempo ---
    property real pxPerSec: 60
    // Enquanto o usuário não mexer no zoom, a timeline se ajusta ao conteúdo.
    property bool userZoomed: false
    property real lastDuration: 0
    readonly property real minPxPerSec: 2
    readonly property real maxPxPerSec: 1200
    function xOf(us) { return us / 1e6 * pxPerSec }
    function tOf(x) { return x / pxPerSec * 1e6 }
    function zoomBy(f) {
        userZoomed = true;
        const anchorT = Project.playhead;
        const anchorView = xOf(anchorT) - flick.contentX;
        pxPerSec = Math.max(minPxPerSec, Math.min(maxPxPerSec, pxPerSec * f));
        flick.contentX = Math.max(0, xOf(anchorT) - anchorView);
    }
    function autoFit() {
        if (!userZoomed && flick.width > 0)
            zoomToFit();
    }
    function zoomToFit() {
        if (flick.width <= 0)
            return;
        if (Project.duration <= 0) {
            pxPerSec = 60; // timeline vazia: escala confortável
            flick.contentX = 0;
            return;
        }
        const secs = Math.max(1, Project.duration / 1e6);
        pxPerSec = Math.max(minPxPerSec, Math.min(maxPxPerSec, (flick.width - 120) / secs));
        flick.contentX = 0;
    }
    // Arraste de vários clipes: os selecionados acompanham o clipe arrastado.
    property var dragSet: []
    property real dragSetDx: 0

    // Snapping de 8 px convertidos em µs.
    function snap(t, ignore) { return Project.snapTime(t, ignore, tOf(8)) }

    readonly property int headerW: 120
    readonly property int rulerH: 28

    // Ordem visual: faixas de vídeo/texto de cima para baixo (maior índice no topo),
    // depois a principal, depois os áudios.
    readonly property var displayTracks: {
        const t = Project.tracks;
        const visual = t.filter(x => x.kind !== "audio" && !x.isMain).sort((a, b) => b.index - a.index);
        const main = t.filter(x => x.isMain);
        const audio = t.filter(x => x.kind === "audio").sort((a, b) => a.index - b.index);
        return visual.concat(main).concat(audio);
    }
    function trackHeight(tr) {
        if (tr.isMain) return 70;
        if (tr.kind === "text" || tr.kind === "adjust") return 36;
        if (tr.kind === "audio") return 48;
        return 56;
    }
    // Índice (no modelo) da faixa sob y (coordenadas do conteúdo); -1 fora.
    function trackAtY(y) {
        let acc = 6;
        for (const tr of displayTracks) {
            const h = trackHeight(tr) + 4;
            if (y >= acc && y < acc + h)
                return tr.index;
            acc += h;
        }
        return -1;
    }
    function trackKind(index) {
        for (const tr of Project.tracks)
            if (tr.index === index) return tr.kind;
        return "";
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---------------- barra de ferramentas ----------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: Theme.panel
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 12
                spacing: 2
                IconButton { iconName: "scissors"; tip: qsTr("Dividir no cursor"); shortcut: "Ctrl+B"; onClicked: Project.splitAtPlayhead() }
                IconButton { iconName: "arrow-left-to-line"; tip: qsTr("Excluir à esquerda do cursor"); shortcut: "Q"; onClicked: Project.deleteLeft() }
                IconButton { iconName: "arrow-right-to-line"; tip: qsTr("Excluir à direita do cursor"); shortcut: "W"; onClicked: Project.deleteRight() }
                IconButton { iconName: "trash-2"; tip: qsTr("Excluir seleção"); shortcut: "Del"; enabled: Project.selection.length > 0; onClicked: Project.deleteSelection(false) }
                IconButton { iconName: "copy"; tip: qsTr("Duplicar"); shortcut: "Ctrl+D"; enabled: Project.selection.length > 0; onClicked: Project.duplicateSelection() }
                IconButton { iconName: "snowflake"; tip: qsTr("Congelar quadro"); shortcut: "Alt+F"; onClicked: Project.freezeFrame() }
                IconButton { iconName: "bookmark"; tip: qsTr("Adicionar marcador"); shortcut: "M"; onClicked: Project.addMarker() }
                Item { Layout.fillWidth: true }
                IconButton { iconName: "magnet"; tip: qsTr("Ímã da faixa principal (sem espaços)"); active: Project.magnet; onClicked: Project.magnet = !Project.magnet }
                IconButton { iconName: "crosshair"; tip: qsTr("Encaixe automático"); shortcut: "N"; active: Project.snapping; onClicked: Project.snapping = !Project.snapping }
                Rectangle { width: 1; height: 18; color: Theme.border; Layout.leftMargin: 4; Layout.rightMargin: 4 }
                IconButton { iconName: "zoom-out"; tip: qsTr("Diminuir zoom"); shortcut: "Ctrl+−"; onClicked: root.zoomBy(0.8) }
                Slider {
                    Layout.preferredWidth: 120
                    focusPolicy: Qt.NoFocus
                    from: Math.log(root.minPxPerSec)
                    to: Math.log(root.maxPxPerSec)
                    value: Math.log(root.pxPerSec)
                    onMoved: root.zoomBy(Math.exp(value) / root.pxPerSec)
                }
                IconButton { iconName: "zoom-in"; tip: qsTr("Aumentar zoom"); shortcut: "Ctrl++"; onClicked: root.zoomBy(1.25) }
                IconButton {
                    iconName: "maximize-2"
                    tip: qsTr("Ajustar à janela")
                    shortcut: "Shift+Z"
                    onClicked: {
                        root.userZoomed = false;
                        root.zoomToFit();
                    }
                }
            }
        }

        // ---------------- corpo ----------------
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Régua
            Item {
                id: ruler
                x: root.headerW
                width: parent.width - root.headerW
                height: root.rulerH
                clip: true
                Canvas {
                    id: rulerCanvas
                    anchors.fill: parent
                    property real offset: flick.contentX
                    property real scale: root.pxPerSec
                    property var markers: Project.markers
                    onOffsetChanged: requestPaint()
                    onScaleChanged: requestPaint()
                    onWidthChanged: requestPaint()
                    onMarkersChanged: requestPaint()
                    onPaint: {
                        const ctx = getContext("2d");
                        ctx.reset();
                        ctx.fillStyle = Theme.panel;
                        ctx.fillRect(0, 0, width, height);
                        const steps = [1 / Project.fps, 0.1, 0.25, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 1800];
                        let major = steps[steps.length - 1];
                        for (const s of steps)
                            if (s * scale >= 90) { major = s; break; }
                        const minor = major / 5;
                        const t0 = offset / scale;
                        const t1 = (offset + width) / scale;
                        ctx.strokeStyle = Theme.textFaint;
                        ctx.fillStyle = Theme.textDim;
                        ctx.font = "10px sans-serif";
                        ctx.lineWidth = 1;
                        for (let t = Math.floor(t0 / minor) * minor; t <= t1; t += minor) {
                            const x = Math.round(t * scale - offset) + 0.5;
                            const isMajor = Math.abs(t / major - Math.round(t / major)) < 1e-6;
                            ctx.beginPath();
                            ctx.moveTo(x, height);
                            ctx.lineTo(x, height - (isMajor ? 10 : 5));
                            ctx.stroke();
                            if (isMajor) {
                                const m = Math.floor(t / 60), s = Math.floor(t % 60);
                                const f = Math.round((t % 1) * Project.fps);
                                let label = String(m).padStart(2, "0") + ":" + String(s).padStart(2, "0");
                                if (major < 1) label += "." + String(f).padStart(2, "0");
                                ctx.fillText(label, x + 3, 11);
                            }
                        }
                        for (const mk of markers) {
                            const x = mk.t / 1e6 * scale - offset;
                            ctx.fillStyle = mk.color;
                            ctx.beginPath();
                            ctx.moveTo(x - 5, 13);
                            ctx.lineTo(x + 5, 13);
                            ctx.lineTo(x, 20);
                            ctx.closePath();
                            ctx.fill();
                        }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.IBeamCursor
                    function seekTo(x) { Project.seek(Math.max(0, root.tOf(x + flick.contentX))) }
                    onPressed: (m) => seekTo(m.x)
                    onPositionChanged: (m) => { if (pressed) seekTo(m.x) }
                }
            }

            // Cabeçalhos das faixas (rolagem vertical acompanha a timeline)
            Item {
                id: headers
                y: root.rulerH
                width: root.headerW
                height: parent.height - root.rulerH
                clip: true
                Column {
                    y: 6 - flick.contentY
                    spacing: 4
                    Repeater {
                        model: root.displayTracks
                        Rectangle {
                            required property var modelData
                            width: root.headerW - 6
                            x: 4
                            height: root.trackHeight(modelData)
                            radius: 4
                            color: Theme.surface
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.top: parent.top
                                anchors.topMargin: 5
                                text: parent.modelData.isMain ? qsTr("Principal") : parent.modelData.name
                                color: parent.modelData.isMain ? Theme.accent : Theme.textDim
                                font.pixelSize: Theme.fontSmall
                                font.bold: parent.modelData.isMain
                            }
                            Row {
                                anchors.right: parent.right
                                anchors.rightMargin: 4
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 2
                                spacing: 0
                                IconButton {
                                    implicitWidth: 24; implicitHeight: 22
                                    visible: parent.parent.modelData.kind !== "text" && parent.parent.modelData.kind !== "adjust"
                                    iconName: parent.parent.modelData.muted ? "volume-x" : "volume-2"
                                    iconSize: 14
                                    glyphColor: parent.parent.modelData.muted ? Theme.danger : Theme.textDim
                                    tip: parent.parent.modelData.muted ? qsTr("Ativar som") : qsTr("Silenciar faixa")
                                    onClicked: Project.setTrackFlag(parent.parent.modelData.index, "muted", !parent.parent.modelData.muted)
                                }
                                IconButton {
                                    implicitWidth: 24; implicitHeight: 22
                                    visible: parent.parent.modelData.kind !== "audio"
                                    iconName: parent.parent.modelData.hidden ? "eye-off" : "eye"
                                    iconSize: 14
                                    glyphColor: parent.parent.modelData.hidden ? Theme.danger : Theme.textDim
                                    tip: parent.parent.modelData.hidden ? qsTr("Mostrar faixa") : qsTr("Ocultar faixa")
                                    onClicked: Project.setTrackFlag(parent.parent.modelData.index, "hidden", !parent.parent.modelData.hidden)
                                }
                                IconButton {
                                    implicitWidth: 24; implicitHeight: 22
                                    iconName: parent.parent.modelData.locked ? "lock" : "lock-open"
                                    iconSize: 14
                                    glyphColor: parent.parent.modelData.locked ? Theme.warning : Theme.textDim
                                    tip: parent.parent.modelData.locked ? qsTr("Desbloquear") : qsTr("Bloquear faixa")
                                    onClicked: Project.setTrackFlag(parent.parent.modelData.index, "locked", !parent.parent.modelData.locked)
                                }
                            }
                        }
                    }
                }
            }

            // Área das faixas
            Flickable {
                id: flick
                x: root.headerW
                y: root.rulerH
                width: parent.width - root.headerW
                height: parent.height - root.rulerH
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: Math.max(width, root.xOf(Project.duration) + width * 0.6)
                contentHeight: Math.max(height, lanes.implicitHeight + 60)
                interactive: false // rolagem só por roda/barra: arrastar é para editar
                onWidthChanged: root.autoFit()
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOn }
                ScrollBar.vertical: ScrollBar {}

                WheelHandler {
                    acceptedModifiers: Qt.ControlModifier
                    onWheel: (e) => root.zoomBy(e.angleDelta.y > 0 ? 1.15 : 1 / 1.15)
                }
                WheelHandler {
                    acceptedModifiers: Qt.NoModifier
                    onWheel: (e) => {
                        const dx = e.angleDelta.x !== 0 ? e.angleDelta.x : 0;
                        const dy = e.angleDelta.y;
                        if (dx !== 0)
                            flick.contentX = Math.max(0, Math.min(flick.contentWidth - flick.width, flick.contentX - dx));
                        else
                            flick.contentY = Math.max(0, Math.min(flick.contentHeight - flick.height, flick.contentY - dy));
                    }
                }
                WheelHandler {
                    acceptedModifiers: Qt.ShiftModifier
                    onWheel: (e) => flick.contentX = Math.max(0, Math.min(flick.contentWidth - flick.width, flick.contentX - e.angleDelta.y))
                }

                // Vazio: clique move o cursor e limpa a seleção; arrastar faz seleção por retângulo.
                MouseArea {
                    id: emptyArea
                    width: flick.contentWidth
                    height: flick.contentHeight
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    property point origin
                    property bool marquee: false
                    onPressed: (m) => {
                        origin = Qt.point(m.x, m.y);
                        marquee = false;
                        if (m.button === Qt.RightButton) {
                            clipMenu.clipId = "";
                            clipMenu.popup();
                        }
                    }
                    onPositionChanged: (m) => {
                        if (!pressed || (m.buttons & Qt.RightButton))
                            return;
                        if (!marquee && Math.abs(m.x - origin.x) + Math.abs(m.y - origin.y) > 6)
                            marquee = true;
                        if (marquee) {
                            band.x = Math.min(origin.x, m.x);
                            band.y = Math.min(origin.y, m.y);
                            band.width = Math.abs(m.x - origin.x);
                            band.height = Math.abs(m.y - origin.y);
                        }
                    }
                    onReleased: (m) => {
                        if (m.button === Qt.RightButton)
                            return;
                        if (marquee) {
                            const tracks = [];
                            for (let y = band.y; y <= band.y + band.height; y += 8) {
                                const t = root.trackAtY(y);
                                if (t >= 0 && tracks.indexOf(t) < 0)
                                    tracks.push(t);
                            }
                            Project.selectInRange(root.tOf(band.x), root.tOf(band.x + band.width), tracks,
                                                  (m.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) !== 0);
                        } else {
                            Project.clearSelection();
                            Project.seek(root.tOf(m.x));
                        }
                        marquee = false;
                    }
                    Rectangle {
                        id: band
                        visible: emptyArea.marquee
                        color: "#3322d3c5"
                        border.color: Theme.accent
                        z: 200
                    }
                }

                // Destino do arraste de mídias da biblioteca.
                DropArea {
                    id: drop
                    width: flick.contentWidth
                    height: flick.contentHeight
                    keys: ["clivcut/asset"]
                    property real hoverX: -1
                    property int hoverTrack: -1
                    onPositionChanged: (d) => {
                        hoverX = d.x;
                        hoverTrack = root.trackAtY(d.y);
                    }
                    onExited: hoverX = -1
                    onDropped: (d) => {
                        const id = d.getDataAsString("clivcut/asset");
                        let track = root.trackAtY(d.y);
                        const t = Math.max(0, root.snap(root.tOf(d.x), []));
                        hoverX = -1;
                        Project.addAssetToTimeline(id, track, t);
                        d.accept(Qt.CopyAction);
                    }
                    Rectangle {
                        visible: drop.containsDrag && drop.hoverX >= 0
                        x: drop.hoverX
                        width: 2
                        height: lanes.implicitHeight + 12
                        color: Theme.accent
                    }
                }

                Column {
                    id: lanes
                    y: 6
                    spacing: 4
                    Repeater {
                        model: root.displayTracks
                        Rectangle {
                            id: lane
                            required property var modelData
                            width: flick.contentWidth
                            height: root.trackHeight(modelData)
                            color: modelData.isMain ? Theme.mainLane : (drop.containsDrag && drop.hoverTrack === modelData.index ? Theme.surface : "transparent")
                            radius: 4
                            opacity: modelData.locked ? 0.6 : 1

                            Text {
                                visible: lane.modelData.isMain && lane.modelData.clips.length === 0
                                anchors.verticalCenter: parent.verticalCenter
                                x: 12
                                text: qsTr("Arraste mídias para cá — faixa principal")
                                color: Theme.textFaint
                                font.pixelSize: Theme.fontNormal
                            }

                            Repeater {
                                model: lane.modelData.clips
                                TimelineClip {
                                    required property var modelData
                                    clipData: modelData
                                    track: lane.modelData
                                    timeline: root
                                    height: lane.height
                                }
                            }
                        }
                    }
                }

                // Linhas de marcadores
                Repeater {
                    model: Project.markers
                    Rectangle {
                        required property var modelData
                        x: root.xOf(modelData.t)
                        width: 1
                        height: flick.contentHeight
                        color: modelData.color
                        opacity: 0.5
                    }
                }

                // Cursor de reprodução
                Rectangle {
                    x: root.xOf(Project.playhead) - 1
                    width: 2
                    height: flick.contentHeight
                    color: Theme.playhead
                    z: 100
                }
            }

            // Cabeça do cursor sobre a régua
            Rectangle {
                x: root.headerW + root.xOf(Project.playhead) - flick.contentX - 6
                visible: x >= root.headerW - 6 && x <= parent.width
                y: root.rulerH - 12
                width: 12
                height: 12
                radius: 2
                rotation: 45
                color: Theme.playhead
            }

            // Mantém o cursor visível durante a reprodução.
            Connections {
                target: Project
                function onPlayheadChanged() {
                    const x = root.xOf(Project.playhead);
                    if (Project.playing && (x > flick.contentX + flick.width - 40 || x < flick.contentX))
                        flick.contentX = Math.max(0, x - 40);
                }
            }
        }
    }

    Component.onCompleted: autoFit()
    Connections {
        target: Project
        function onFileChanged() {
            root.userZoomed = false;
            root.autoFit();
        }
        function onProjectChanged() {
            if (Project.duration !== root.lastDuration) {
                root.lastDuration = Project.duration;
                root.autoFit();
            }
        }
    }

    // Menu de contexto dos clipes (clique direito).
    Menu {
        id: clipMenu
        property string clipId: ""
        MenuItem { text: qsTr("Copiar\tCtrl+C"); enabled: Project.selection.length > 0; onTriggered: Project.copySelection() }
        MenuItem { text: qsTr("Recortar\tCtrl+X"); enabled: Project.selection.length > 0; onTriggered: Project.cutSelection() }
        MenuItem { text: qsTr("Colar no cursor\tCtrl+V"); enabled: Project.canPaste; onTriggered: Project.paste() }
        MenuItem { text: qsTr("Duplicar\tCtrl+D"); enabled: Project.selection.length > 0; onTriggered: Project.duplicateSelection() }
        MenuSeparator {}
        MenuItem { text: qsTr("Copiar atributos\tCtrl+Alt+C"); enabled: Project.selectedClip.id !== undefined; onTriggered: Project.copyAttributes() }
        MenuItem { text: qsTr("Colar atributos\tCtrl+Alt+V"); enabled: Project.canPasteAttributes && Project.selection.length > 0; onTriggered: Project.pasteAttributesToSelection() }
        MenuSeparator {}
        MenuItem { text: qsTr("Agrupar\tCtrl+G"); enabled: Project.selection.length > 1; onTriggered: Project.groupSelection() }
        MenuItem { text: qsTr("Desagrupar\tCtrl+Shift+G"); enabled: Project.selection.length > 0; onTriggered: Project.ungroupSelection() }
        MenuSeparator {}
        MenuItem { text: qsTr("Dividir no cursor\tCtrl+B"); onTriggered: Project.splitAtPlayhead() }
        MenuItem { text: qsTr("Congelar quadro\tAlt+F"); enabled: clipMenu.clipId !== ""; onTriggered: Project.freezeFrame() }
        MenuItem { text: qsTr("Separar áudio\tCtrl+Shift+D"); enabled: clipMenu.clipId !== ""; onTriggered: Project.detachAudio() }
        MenuItem { text: qsTr("Excluir\tDel"); enabled: Project.selection.length > 0; onTriggered: Project.deleteSelection(false) }
    }
    function openClipMenu(clipId) {
        clipMenu.clipId = clipId;
        clipMenu.popup();
    }
}
