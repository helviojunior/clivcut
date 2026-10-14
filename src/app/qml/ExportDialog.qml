import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtCore
import Clivcut

Dialog {
    id: root
    title: qsTr("Export video")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 460
    closePolicy: Project.exporting ? Popup.NoAutoClose : Popup.CloseOnEscape

    background: Rectangle { color: Theme.panel; radius: 10; border.color: Theme.border }
    header: Text {
        text: root.title
        color: Theme.text
        font.pixelSize: Theme.fontLarge
        font.bold: true
        padding: 16
    }

    Settings {
        id: prefs
        category: "export"
        property alias resolution: resBox.currentIndex
        property alias fpsIndex: fpsBox.currentIndex
        property alias qualityIndex: qualityBox.currentIndex
        property string folder: StandardPaths.writableLocation(StandardPaths.MoviesLocation)
    }

    readonly property var codecLabels: ({ h264: "H.264 (MP4)", hevc: "HEVC / H.265 (MP4)", av1: "AV1 (MP4)", prores: "ProRes 422 HQ (MOV)" })

    function suggestedName() {
        const base = Project.projectName.replace(/[\\/:*?"<>|]/g, "_");
        const ext = codecBox.currentValue === "prores" ? ".mov" : ".mp4";
        return prefs.folder + "/" + base + ext;
    }

    onOpened: pathField.text = suggestedName().replace(/^file:\/\//, "")

    contentItem: ColumnLayout {
        spacing: 12
        enabled: !Project.exporting

        GridLayout {
            columns: 2
            columnSpacing: 12
            rowSpacing: 10
            Layout.fillWidth: true

            Text { text: qsTr("File"); color: Theme.textDim; font.pixelSize: Theme.fontNormal }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: pathField
                    Layout.fillWidth: true
                    color: Theme.text
                    selectByMouse: true
                }
                Button {
                    text: "…"
                    focusPolicy: Qt.NoFocus
                    onClicked: fileDialog.open()
                }
            }

            Text { text: qsTr("Resolution"); color: Theme.textDim; font.pixelSize: Theme.fontNormal }
            ComboBox {
                id: resBox
                Layout.fillWidth: true
                model: ["480p", "720p", "1080p", "2K (1440p)", "4K (2160p)"]
                currentIndex: 2
                readonly property var values: [480, 720, 1080, 1440, 2160]
            }

            Text { text: qsTr("Frame rate"); color: Theme.textDim; font.pixelSize: Theme.fontNormal }
            ComboBox {
                id: fpsBox
                Layout.fillWidth: true
                model: [qsTr("From project (%1)").arg(Project.fps), "24", "25", "30", "50", "60"]
                readonly property var values: [0, 24, 25, 30, 50, 60]
            }

            Text { text: qsTr("Codec"); color: Theme.textDim; font.pixelSize: Theme.fontNormal }
            ComboBox {
                id: codecBox
                Layout.fillWidth: true
                model: Project.videoCodecs.map(c => ({ value: c, text: root.codecLabels[c] || c }))
                textRole: "text"
                valueRole: "value"
                onActivated: pathField.text = pathField.text.replace(/\.(mp4|mov)$/i, currentValue === "prores" ? ".mov" : ".mp4")
            }

            Text { text: qsTr("Quality"); color: Theme.textDim; font.pixelSize: Theme.fontNormal }
            ComboBox {
                id: qualityBox
                Layout.fillWidth: true
                model: [qsTr("High"), qsTr("Recommended"), qsTr("Smaller file")]
                currentIndex: 1
                readonly property var crf: [16, 20, 26]
            }
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textFaint
            font.pixelSize: Theme.fontSmall
            text: qsTr("Duration: %1 · Canvas %2×%3").arg(Project.formatTime(Project.duration)).arg(Project.canvasWidth).arg(Project.canvasHeight)
        }

        ProgressBar {
            Layout.fillWidth: true
            visible: Project.exporting
            value: Project.exportProgress
        }
    }

    footer: DialogButtonBox {
        background: Rectangle { color: "transparent" }
        Button {
            text: Project.exporting ? qsTr("Cancel export") : qsTr("Close")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: Project.exporting ? Project.cancelExport() : root.close()
        }
        Button {
            text: Project.exporting ? qsTr("%1%").arg(Math.round(Project.exportProgress * 100)) : qsTr("Export")
            enabled: !Project.exporting && Project.duration > 0 && pathField.text !== ""
            highlighted: true
            DialogButtonBox.buttonRole: DialogButtonBox.ApplyRole
            onClicked: {
                const path = pathField.text;
                prefs.folder = "file://" + path.substring(0, path.lastIndexOf("/"));
                Project.exportVideo("file://" + path, resBox.values[resBox.currentIndex], fpsBox.values[fpsBox.currentIndex],
                                    codecBox.currentValue, qualityBox.crf[qualityBox.currentIndex]);
            }
        }
    }

    Connections {
        target: Project
        function onExportFinished(ok, message) {
            if (ok)
                root.close();
        }
    }

    FileDialog {
        id: fileDialog
        fileMode: FileDialog.SaveFile
        currentFolder: prefs.folder
        nameFilters: [qsTr("Video (*.mp4 *.mov *.mkv)")]
        onAccepted: pathField.text = selectedFile.toString().replace(/^file:\/\//, "")
    }
}
