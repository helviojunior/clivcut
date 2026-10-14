import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Clivcut

ToolBar {
    id: root
    signal newRequested()
    signal openRequested()
    signal saveRequested()
    signal saveAsRequested()
    signal importDraftRequested()
    signal exportRequested()

    height: 46
    background: Rectangle {
        color: Theme.panel
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.border
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 4

        Image {
            source: "qrc:/qt/qml/Clivcut/icons/clivcut.svg"
            sourceSize: Qt.size(24, 24)
        }
        Text {
            text: "clivcut"
            color: Theme.text
            font.pixelSize: Theme.fontLarge
            font.bold: true
            Layout.rightMargin: 12
        }

        IconButton {
            text: qsTr("Arquivo")
            onClicked: fileMenu.popup()
            Menu {
                id: fileMenu
                MenuItem { text: qsTr("Novo projeto…"); onTriggered: root.newRequested() }
                MenuItem { text: qsTr("Abrir…"); onTriggered: root.openRequested() }
                MenuItem { text: qsTr("Salvar"); onTriggered: root.saveRequested() }
                MenuItem { text: qsTr("Salvar como…"); onTriggered: root.saveAsRequested() }
                MenuSeparator {}
                MenuItem { text: qsTr("Importar draft JSON…"); onTriggered: root.importDraftRequested() }
                MenuSeparator {}
                MenuItem { text: qsTr("Exportar vídeo…"); onTriggered: root.exportRequested() }
            }
        }
        IconButton {
            glyph: "↶"
            tip: Project.canUndo ? qsTr("Desfazer: %1 (Ctrl+Z)").arg(Project.undoText) : qsTr("Desfazer")
            enabled: Project.canUndo
            onClicked: Project.undo()
        }
        IconButton {
            glyph: "↷"
            tip: Project.canRedo ? qsTr("Refazer: %1 (Ctrl+Shift+Z)").arg(Project.redoText) : qsTr("Refazer")
            enabled: Project.canRedo
            onClicked: Project.redo()
        }

        Item { Layout.fillWidth: true }

        TextField {
            id: nameField
            Layout.preferredWidth: 260
            horizontalAlignment: Text.AlignHCenter
            text: Project.projectName
            color: Theme.text
            font.pixelSize: Theme.fontNormal
            selectByMouse: true
            background: Rectangle {
                radius: Theme.radius
                color: nameField.activeFocus ? Theme.surface : "transparent"
                border.color: nameField.activeFocus ? Theme.accent : "transparent"
            }
            onEditingFinished: {
                Project.projectName = text;
                focus = false;
            }
        }
        Text {
            text: Project.modified ? qsTr("não salvo") : (Project.filePath !== "" ? qsTr("salvo") : "")
            color: Theme.textFaint
            font.pixelSize: Theme.fontSmall
        }

        Item { Layout.fillWidth: true }

        Button {
            id: exportButton
            text: Project.exporting ? qsTr("Exportando %1%").arg(Math.round(Project.exportProgress * 100)) : qsTr("⇪  Exportar")
            focusPolicy: Qt.NoFocus
            onClicked: root.exportRequested()
            contentItem: Text {
                text: exportButton.text
                color: "#06201e"
                font.pixelSize: Theme.fontNormal
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                implicitWidth: 120
                implicitHeight: 32
                radius: Theme.radius
                color: exportButton.down ? Theme.accentDim : Theme.accent
            }
        }
    }
}
