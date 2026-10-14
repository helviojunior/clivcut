import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Clivcut

// Oferece recuperar projetos de sessões que não fecharam normalmente.
Dialog {
    id: root
    title: qsTr("Recuperar projetos")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 480
    closePolicy: Popup.CloseOnEscape

    property var items: []
    function refresh() { items = Project.recoverableProjects() }

    background: Rectangle { color: Theme.panel; radius: 10; border.color: Theme.border }
    header: Text {
        text: root.title
        color: Theme.text
        font.pixelSize: Theme.fontLarge
        font.bold: true
        padding: 16
    }

    contentItem: ColumnLayout {
        spacing: 10
        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textDim
            font.pixelSize: Theme.fontNormal
            text: qsTr("O clivcut não foi fechado normalmente da última vez. Estas alterações não salvas foram guardadas:")
        }
        Repeater {
            model: root.items
            Rectangle {
                id: row
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: 56
                radius: Theme.radius
                color: Theme.surface
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            Layout.fillWidth: true
                            text: row.modelData.name
                            color: Theme.text
                            font.pixelSize: Theme.fontNormal
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("%1 · %2%3").arg(row.modelData.savedAt).arg(Project.formatTime(row.modelData.duration))
                                  .arg(row.modelData.originalPath !== "" ? " · " + row.modelData.originalPath : qsTr(" · nunca salvo"))
                            color: Theme.textFaint
                            font.pixelSize: Theme.fontSmall
                            elide: Text.ElideMiddle
                        }
                    }
                    Button {
                        text: qsTr("Recuperar")
                        highlighted: true
                        focusPolicy: Qt.NoFocus
                        onClicked: {
                            Project.recoverProject(row.modelData.file);
                            root.close();
                        }
                    }
                    Button {
                        text: qsTr("Descartar")
                        focusPolicy: Qt.NoFocus
                        onClicked: {
                            Project.discardRecovery(row.modelData.file);
                            root.refresh();
                            if (root.items.length === 0)
                                root.close();
                        }
                    }
                }
            }
        }
    }

    footer: DialogButtonBox {
        background: Rectangle { color: "transparent" }
        Button { text: qsTr("Agora não"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
    }
}
