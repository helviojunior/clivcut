import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Clivcut

Flickable {
    id: root
    contentHeight: col.implicitHeight + 24
    clip: true
    ScrollBar.vertical: ScrollBar {}

    readonly property var sel: Project.selectedClip
    // Filtro vai para o clipe selecionado (mídia ou ajuste); sem seleção, vira clipe de filtro.
    readonly property bool hasTarget: sel.id !== undefined && (sel.type === "adjust" || (sel.type === "media" && sel.mediaKind !== "audio"))
    readonly property string currentFilter: hasTarget ? sel.filter : ""
    // Recalcula o quadro de referência quando seleção/cursor mudam (com atraso para não pesar no scrub).
    property string frameRef: Project.previewFrameRef()
    Timer {
        id: refTimer
        interval: 250
        onTriggered: root.frameRef = Project.previewFrameRef()
    }
    Connections {
        target: Project
        function onSelectionChanged() { refTimer.restart() }
        function onPlayheadChanged() { if (!Project.playing) refTimer.restart() }
        function onRevisionChanged() { refTimer.restart() }
    }
    readonly property string previewBase: frameRef !== "" ? frameRef : "sample"

    function apply(filterId) {
        if (hasTarget)
            Project.applyFilter(sel.id, filterId);
        else if (filterId !== "")
            Project.addFilterClip(filterId);
    }

    ColumnLayout {
        id: col
        x: 10
        y: 10
        width: root.width - 20
        spacing: 8

        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textFaint
            font.pixelSize: Theme.fontSmall
            text: root.hasTarget ? qsTr("Click to apply to the selected clip. Use + to create a filter clip that affects everything below it on the timeline.")
                                 : qsTr("No clip selected: the filter is added as a clip on the filter track, starting at the playhead, and affects everything below.")
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                Layout.fillWidth: true
                text: qsTr("+ Adjustment layer")
                focusPolicy: Qt.NoFocus
                onClicked: Project.addFilterClip("")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Color adjustment clip (brightness, contrast, LUT…) applied to everything below")
            }
        }

        // Intensidade do filtro atual
        ColumnLayout {
            visible: root.hasTarget && root.currentFilter !== ""
            Layout.fillWidth: true
            SectionLabel {
                text: qsTr("Filter: %1").arg(root.hasTarget ? root.sel.filterName : "")
                actionText: qsTr("Remove")
                onActionClicked: Project.applyFilter(root.sel.id, "")
            }
            PropSlider {
                label: qsTr("Intensity")
                from: 0; to: 1; displayScale: 100; decimals: 0; suffix: "%"
                value: root.hasTarget ? root.sel.filterIntensity : 0.8
                onMoved: (v) => Project.setClipProperty(root.sel.id, "filterIntensity", v)
            }
        }

        Repeater {
            model: ["color", "cinema", "retro", "bw"]
            ColumnLayout {
                id: group
                required property string modelData
                Layout.fillWidth: true
                SectionLabel { text: group.modelData === "color" ? qsTr("Color") : group.modelData === "retro" ? qsTr("Retro") : group.modelData === "bw" ? qsTr("Black & white") : qsTr("Cinema") }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: Project.filters.filter(f => f.category === group.modelData)
                        Item {
                            id: tile
                            required property var modelData
                            readonly property bool active: root.currentFilter === modelData.id
                            width: Math.floor((col.width - 12) / 3)
                            height: width * 9 / 16 + 20
                            Rectangle {
                                anchors.fill: parent
                                radius: Theme.radius
                                color: Theme.surface
                                border.color: tile.active ? Theme.accent : (hover.hovered ? Theme.border : "transparent")
                                border.width: 2
                                clip: true
                                Image {
                                    id: preview
                                    x: 2
                                    y: 2
                                    width: parent.width - 4
                                    height: (parent.width - 4) * 9 / 16
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    sourceSize.width: 160
                                    source: "image://filter/" + tile.modelData.id + "/" + root.previewBase
                                }
                                Text {
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 3
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    text: tile.modelData.name
                                    color: tile.active ? Theme.accent : Theme.textDim
                                    font.pixelSize: Theme.fontSmall
                                    elide: Text.ElideRight
                                }
                                Rectangle {
                                    visible: hover.hovered
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 4
                                    width: 20
                                    height: 20
                                    radius: 10
                                    color: Theme.accent
                                    z: 2
                                    Text { anchors.centerIn: parent; text: "+"; color: "black"; font.bold: true }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: Project.addFilterClip(tile.modelData.id)
                                    }
                                    ToolTip.visible: plusHover.hovered
                                    ToolTip.text: qsTr("Add as filter clip")
                                    HoverHandler { id: plusHover }
                                }
                            }
                            HoverHandler { id: hover }
                            TapHandler { onTapped: root.apply(tile.active ? "" : tile.modelData.id) }
                        }
                    }
                }
            }
        }
    }
}
