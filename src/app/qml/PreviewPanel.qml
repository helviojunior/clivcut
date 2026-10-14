import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Clivcut

Rectangle {
    id: root
    color: Theme.bg

    readonly property var sel: Project.selectedClip
    readonly property bool canMove: sel.id !== undefined && sel.underPlayhead && (sel.type === "text" || sel.mediaKind !== "audio")

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Cabeçalho: proporção e qualidade
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Text {
                text: qsTr("Player")
                color: Theme.textDim
                font.pixelSize: Theme.fontNormal
                visible: root.width > 420
            }
            Item { Layout.fillWidth: true }
            ComboBox {
                id: ratio
                focusPolicy: Qt.NoFocus
                implicitWidth: 130
                model: [
                    { label: "16:9 · Paisagem", w: 1920, h: 1080 },
                    { label: "9:16 · Vertical", w: 1080, h: 1920 },
                    { label: "1:1 · Quadrado", w: 1080, h: 1080 },
                    { label: "4:3", w: 1440, h: 1080 },
                    { label: "3:4", w: 1080, h: 1440 },
                    { label: "4:5 · Feed", w: 1080, h: 1350 },
                    { label: "21:9 · Cinema", w: 2560, h: 1080 }
                ]
                textRole: "label"
                displayText: Project.canvasWidth + "×" + Project.canvasHeight
                onActivated: (i) => Project.setCanvas(model[i].w, model[i].h)
            }
            IconButton {
                text: Project.proxiesPending > 0 ? "P ⟳" + Project.proxiesPending : "Proxy"
                active: Project.useProxies
                onClicked: Project.useProxies = !Project.useProxies
                tip: Project.useProxies ? qsTr("Playing with proxies (lightweight copies of large videos). Click to use the originals.")
                                        : qsTr("Playing the original files. Click to use proxies.")
            }
            ComboBox {
                id: quality
                focusPolicy: Qt.NoFocus
                implicitContentWidthPolicy: ComboBox.WidestText
                model: [qsTr("Preview 540p"), qsTr("Preview 720p"), qsTr("Preview 1080p")]
                readonly property var sides: [960, 1280, 1920]
                currentIndex: Math.max(0, sides.indexOf(Project.previewResolution))
                onActivated: (i) => Project.previewResolution = sides[i]
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Playback resolution (does not affect export)")
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            PreviewItem {
                id: preview
                anchors.fill: parent
                maxResolution: Project.previewResolution
            }

            // Moldura do canvas + arrastar para mover o clipe selecionado.
            Rectangle {
                id: frame
                x: preview.canvasRect.x
                y: preview.canvasRect.y
                width: preview.canvasRect.width
                height: preview.canvasRect.height
                color: "transparent"
                border.color: dragMover.containsMouse && root.canMove ? Theme.accent : "transparent"
                border.width: 1

                MouseArea {
                    id: dragMover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: root.canMove ? (pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor) : Qt.ArrowCursor
                    property real startX: 0
                    property real startY: 0
                    property point origin
                    onPressed: (mouse) => {
                        if (!root.canMove) {
                            mouse.accepted = false;
                            return;
                        }
                        startX = root.sel.x;
                        startY = root.sel.y;
                        origin = Qt.point(mouse.x, mouse.y);
                    }
                    onPositionChanged: (mouse) => {
                        if (!pressed || !root.canMove)
                            return;
                        const nx = startX + (mouse.x - origin.x) / (width / 2);
                        const ny = startY + (mouse.y - origin.y) / (height / 2);
                        // Encaixa no centro quando perto.
                        Project.setClipProperties(root.sel.id, { x: Math.abs(nx) < 0.02 ? 0 : nx, y: Math.abs(ny) < 0.02 ? 0 : ny });
                    }
                    onWheel: (wheel) => {
                        if (!root.canMove || !(wheel.modifiers & Qt.ControlModifier)) {
                            wheel.accepted = false;
                            return;
                        }
                        const f = wheel.angleDelta.y > 0 ? 1.05 : 1 / 1.05;
                        Project.setClipProperty(root.sel.id, "scale", Math.max(0.05, Math.min(10, root.sel.scale * f)));
                    }
                }
                // Guias de centro durante o arraste.
                Rectangle {
                    visible: dragMover.pressed && root.canMove && Math.abs(root.sel.x) < 0.001
                    x: parent.width / 2
                    width: 1
                    height: parent.height
                    color: Theme.accent
                }
                Rectangle {
                    visible: dragMover.pressed && root.canMove && Math.abs(root.sel.y) < 0.001
                    y: parent.height / 2
                    height: 1
                    width: parent.width
                    color: Theme.accent
                }
            }

            Text {
                anchors.centerIn: parent
                visible: Project.duration <= 0
                text: qsTr("Import media and drag it to the timeline")
                color: Theme.textFaint
                font.pixelSize: Theme.fontLarge
            }
        }

        // Transporte
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Text {
                text: Project.formatTime(Project.playhead)
                color: Theme.accent
                font.pixelSize: Theme.fontNormal
                font.family: "monospace"
            }
            Text {
                text: " / " + Project.formatTime(Project.duration)
                color: Theme.textDim
                font.pixelSize: Theme.fontNormal
                font.family: "monospace"
            }
            Item { Layout.fillWidth: true }
            IconButton { iconName: "skip-back"; tip: qsTr("Go to start"); shortcut: Keymap.label("start"); onClicked: Project.jumpToEdge(false) }
            IconButton { iconName: "chevron-left"; tip: qsTr("Previous frame"); shortcut: Keymap.label("prevFrame"); onClicked: Project.stepFrames(-1) }
            // Play em destaque
            AbstractButton {
                id: playButton
                implicitWidth: 40
                implicitHeight: 40
                focusPolicy: Qt.NoFocus
                hoverEnabled: true
                onClicked: Project.togglePlay()
                background: Rectangle {
                    radius: 20
                    color: playButton.down ? Theme.accentDim : Theme.accent
                }
                contentItem: Item {
                    Icon {
                        anchors.centerIn: parent
                        anchors.horizontalCenterOffset: Project.playing ? 0 : 1
                        name: Project.playing ? "pause" : "play"
                        size: 18
                        color: Theme.accentInk
                    }
                }
                ToolTip.visible: hovered
                ToolTip.delay: 450
                ToolTip.text: qsTr("Play/pause") + "   " + Keymap.label("play")
            }
            IconButton { iconName: "chevron-right"; tip: qsTr("Next frame"); shortcut: Keymap.label("nextFrame"); onClicked: Project.stepFrames(1) }
            IconButton { iconName: "skip-forward"; tip: qsTr("Go to end"); shortcut: Keymap.label("end"); onClicked: Project.jumpToEdge(true) }
            Item { Layout.fillWidth: true }
            Text {
                text: Project.fps.toFixed(Project.fps % 1 === 0 ? 0 : 2) + " fps"
                color: Theme.textFaint
                font.pixelSize: Theme.fontSmall
            }
        }
    }
}
