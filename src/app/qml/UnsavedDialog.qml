import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Clivcut

// "Salvar alterações?" antes de fechar, abrir ou criar outro projeto.
Dialog {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 420
    title: qsTr("Save changes?")

    signal saveRequested()
    signal discardRequested()

    background: Rectangle { color: Theme.panel; radius: 10; border.color: Theme.border }
    header: Text {
        text: root.title
        color: Theme.text
        font.pixelSize: Theme.fontLarge
        font.bold: true
        padding: 16
    }
    contentItem: Text {
        wrapMode: Text.Wrap
        color: Theme.textDim
        font.pixelSize: Theme.fontNormal
        text: qsTr("The project “%1” has unsaved changes.").arg(Project.projectName)
    }
    footer: DialogButtonBox {
        background: Rectangle { color: "transparent" }
        Button {
            text: qsTr("Discard")
            DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole
            onClicked: {
                root.close();
                root.discardRequested();
            }
        }
        Button { text: qsTr("Cancel"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        Button {
            text: qsTr("Save")
            highlighted: true
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            onClicked: {
                root.close();
                root.saveRequested();
            }
        }
    }
}
