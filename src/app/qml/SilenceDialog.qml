import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import Clivcut

// Remoção de silêncios: analisa a mixagem, mostra as pausas na régua e corta com ripple.
Dialog {
    id: root
    title: qsTr("Remove silences")
    modal: false // deixa ver as pausas marcadas na timeline
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

    Settings {
        id: prefs
        category: "silences"
        property alias threshold: threshold.value
        property alias minPause: minPause.value
        property alias padding: padding.value
    }

    readonly property real total: Project.silences.reduce((acc, r) => acc + r.end - r.start, 0)
    function analyze() { Project.analyzeSilences(threshold.value, minPause.value * 1000, padding.value * 1000) }

    onOpened: analyze()
    onClosed: Project.clearSilences()

    contentItem: ColumnLayout {
        spacing: 8
        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textFaint
            font.pixelSize: Theme.fontSmall
            text: (Project.inPoint >= 0 || Project.outPoint > 0)
                  ? qsTr("Only the in/out range is analyzed. Pauses are marked in red on the ruler; the margin keeps a little silence around each phrase so cuts do not sound abrupt.")
                  : qsTr("The whole timeline is analyzed. Pauses are marked in red on the ruler; the margin keeps a little silence around each phrase so cuts do not sound abrupt.")
        }
        PropSlider {
            id: threshold
            label: qsTr("Silence below")
            from: -60; to: -20; stepSize: 1; decimals: 0; suffix: " dB"
            value: -40
            onMoved: (v) => { value = v; analyzeTimer.restart() }
        }
        PropSlider {
            id: minPause
            label: qsTr("Minimum pause")
            from: 0.2; to: 3; stepSize: 0.1; decimals: 1; suffix: " s"
            value: 0.5
            onMoved: (v) => { value = v; analyzeTimer.restart() }
        }
        PropSlider {
            id: padding
            label: qsTr("Margin")
            from: 0; to: 0.5; stepSize: 0.02; decimals: 2; suffix: " s"
            value: 0.12
            onMoved: (v) => { value = v; analyzeTimer.restart() }
        }
        Timer { id: analyzeTimer; interval: 400; onTriggered: root.analyze() }
        ProgressBar {
            Layout.fillWidth: true
            visible: Project.analyzingSilences
            value: Project.silenceProgress
        }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Project.silences.length > 0 ? Theme.accent : Theme.textDim
            font.pixelSize: Theme.fontNormal
            text: Project.analyzingSilences ? qsTr("Analyzing the audio…")
                  : Project.silences.length === 0 ? qsTr("No pauses found with these settings.")
                  : qsTr("%n pause(s) to remove, %1 in total.", "", Project.silences.length).arg(Project.formatTime(root.total))
        }
    }

    footer: DialogButtonBox {
        background: Rectangle { color: "transparent" }
        Button {
            text: qsTr("Close")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: root.close()
        }
        Button {
            text: qsTr("Remove pauses")
            highlighted: true
            enabled: !Project.analyzingSilences && Project.silences.length > 0
            DialogButtonBox.buttonRole: DialogButtonBox.ApplyRole
            onClicked: {
                Project.removeSilences();
                root.close();
            }
        }
    }
}
