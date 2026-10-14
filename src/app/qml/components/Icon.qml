import QtQuick
import Clivcut

// Ícone SVG da interface (icons/ui), colorido com qualquer cor do tema.
Item {
    id: root
    property string name
    property color color: Theme.text
    property int size: 18
    implicitWidth: size
    implicitHeight: size

    Image {
        anchors.fill: parent
        sourceSize: Qt.size(root.size * 2, root.size * 2) // nítido em telas HiDPI
        smooth: true
        source: root.name !== "" ? "image://icon/" + root.name + "/" + Theme.hex(root.color) : ""
    }
}
