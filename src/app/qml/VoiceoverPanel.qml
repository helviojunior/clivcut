import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import Clivcut

// Narração: escolhe o microfone, conta 3-2-1 e grava enquanto a timeline toca a partir do cursor.
Rectangle {
    id: root
    property bool armed: false   // painel aberto (antes ou durante a gravação)
    property int countdown: 0
    visible: armed || Project.recording
    radius: Theme.radius
    color: Qt.rgba(0.04, 0.04, 0.05, 0.9)
    border.color: Project.recording ? Theme.danger : Theme.border
    implicitWidth: 300
    implicitHeight: col.implicitHeight + 20

    function open() {
        devices.model = [qsTr("System default")].concat(Project.inputDevices());
        armed = true;
    }
    function start() {
        countdown = 3;
        countTimer.start();
    }
    function stop() {
        countTimer.stop();
        countdown = 0;
        if (Project.recording)
            Project.stopRecording();
        armed = false;
    }

    Settings {
        id: prefs
        category: "recording"
        property string device: ""
    }

    Timer {
        id: countTimer
        interval: 1000
        repeat: true
        onTriggered: {
            if (--root.countdown > 0)
                return;
            stop();
            if (!Project.startRecording(prefs.device))
                root.armed = false;
        }
    }

    ColumnLayout {
        id: col
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Rectangle {
                width: 10; height: 10; radius: 5
                color: Theme.danger
                opacity: Project.recording ? (blink.on ? 1 : 0.3) : 0.5
                Timer { id: blink; property bool on: true; running: Project.recording; interval: 500; repeat: true; onTriggered: on = !on }
            }
            Text {
                Layout.fillWidth: true
                text: Project.recording ? qsTr("Recording %1").arg(Project.formatTime(Project.recordElapsed))
                      : root.countdown > 0 ? qsTr("Starting in %1…").arg(root.countdown)
                      : qsTr("Voiceover")
                color: "#ececef"
                font.pixelSize: Theme.fontNormal
                font.bold: true
            }
        }

        // Microfone (antes de começar)
        ComboBox {
            id: devices
            visible: !Project.recording && root.countdown === 0
            Layout.fillWidth: true
            focusPolicy: Qt.NoFocus
            currentIndex: Math.max(0, model ? model.indexOf(prefs.device) : 0)
            onActivated: (i) => prefs.device = i === 0 ? "" : model[i]
        }
        Text {
            visible: !Project.recording && root.countdown === 0
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: "#9a9aa3"
            font.pixelSize: Theme.fontSmall
            text: qsTr("Records from the playhead while the timeline plays. Use headphones so the microphone does not pick up the video's sound.")
        }

        // Nível do microfone
        Rectangle {
            visible: Project.recording
            Layout.fillWidth: true
            implicitHeight: 8
            radius: 4
            color: "#2b2b30"
            Rectangle {
                width: parent.width * Math.min(1, Math.sqrt(Project.recordLevel))
                height: parent.height
                radius: 4
                color: Project.recordLevel > 0.9 ? Theme.danger : Project.recordLevel > 0.5 ? Theme.warning : Theme.success
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            PillButton {
                text: Project.recording || root.countdown > 0 ? qsTr("Stop") : qsTr("Cancel")
                iconName: Project.recording ? "square" : "x"
                onClicked: root.stop()
            }
            PillButton {
                visible: !Project.recording && root.countdown === 0
                primary: true
                iconName: "mic"
                text: qsTr("Record")
                onClicked: root.start()
            }
        }
    }
}
