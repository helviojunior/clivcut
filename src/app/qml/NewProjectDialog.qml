import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Clivcut

Dialog {
    id: root
    title: qsTr("New project")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 420

    background: Rectangle { color: Theme.panel; radius: 10; border.color: Theme.border }
    header: Text {
        text: root.title
        color: Theme.text
        font.pixelSize: Theme.fontLarge
        font.bold: true
        padding: 16
    }

    property int choice: 0
    readonly property var presets: [
        { name: qsTr("YouTube / Landscape"), w: 1920, h: 1080, glyph: "▭" },
        { name: qsTr("Vertical (Reels / Shorts)"), w: 1080, h: 1920, glyph: "▯" },
        { name: qsTr("Square"), w: 1080, h: 1080, glyph: "□" },
        { name: qsTr("4K Landscape"), w: 3840, h: 2160, glyph: "▭" }
    ]

    contentItem: ColumnLayout {
        spacing: 10
        Flow {
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: root.presets
                TileButton {
                    required property var modelData
                    required property int index
                    width: 92
                    glyph: modelData.glyph
                    caption: modelData.name
                    selected: root.choice === index
                    onClicked: root.choice = index
                }
            }
        }
        RowLayout {
            Text { text: qsTr("Frame rate"); color: Theme.textDim; font.pixelSize: Theme.fontNormal }
            ComboBox {
                id: fps
                model: ["24", "25", "30", "50", "60"]
                currentIndex: 2
            }
        }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textFaint
            font.pixelSize: Theme.fontSmall
            text: qsTr("Unsaved changes in the current project will be lost.")
            visible: Project.modified
        }
    }

    footer: DialogButtonBox {
        background: Rectangle { color: "transparent" }
        Button { text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        Button {
            text: qsTr("Create")
            highlighted: true
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    onAccepted: {
        const p = presets[choice];
        Project.newProject(p.w, p.h, parseFloat(fps.currentText));
    }
}
