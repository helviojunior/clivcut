import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Clivcut

// Paleta de comandos (Ctrl+K): busca qualquer ação pelo nome e mostra o atalho.
Popup {
    id: root
    property var commands: []   // [{ name, shortcut, icon, run: function }]
    modal: true
    focus: true
    width: Math.min(560, parent ? parent.width - 80 : 560)
    height: Math.min(440, list.contentHeight + 76)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? parent.height * 0.14 : 0
    padding: 8
    background: Rectangle {
        radius: 12
        color: Theme.panel
        border.color: Theme.border
    }

    function normalize(s) { return s.toLowerCase().normalize("NFD").replace(/[̀-ͯ]/g, "") }
    readonly property var filtered: {
        const q = normalize(field.text.trim());
        if (q === "")
            return commands;
        return commands.filter(c => q.split(/\s+/).every(w => normalize(c.name).indexOf(w) >= 0));
    }
    function runAt(i) {
        if (i < 0 || i >= filtered.length)
            return;
        const c = filtered[i];
        close();
        Qt.callLater(c.run);
    }

    onOpened: {
        field.text = "";
        list.currentIndex = 0;
        field.forceActiveFocus();
    }

    contentItem: ColumnLayout {
        spacing: 6
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 6
            spacing: 8
            Icon { name: "search"; size: 18; color: Theme.textDim }
            TextField {
                id: field
                Layout.fillWidth: true
                placeholderText: qsTr("What do you want to do? (e.g. split, export, captions)")
                color: Theme.text
                font.pixelSize: Theme.fontLarge
                background: Item {}
                Keys.onDownPressed: list.currentIndex = Math.min(list.count - 1, list.currentIndex + 1)
                Keys.onUpPressed: list.currentIndex = Math.max(0, list.currentIndex - 1)
                Keys.onReturnPressed: root.runAt(list.currentIndex)
                Keys.onEnterPressed: root.runAt(list.currentIndex)
                onTextChanged: list.currentIndex = 0
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.filtered
            highlightMoveDuration: 0
            delegate: Rectangle {
                id: row
                required property var modelData
                required property int index
                width: list.width
                height: 38
                radius: Theme.radius
                color: ListView.isCurrentItem ? Theme.accentSoft : (rowHover.hovered ? Theme.surface : "transparent")
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 10
                    Icon { name: row.modelData.icon || "chevron-right"; size: 16; color: Theme.textDim }
                    Text {
                        Layout.fillWidth: true
                        text: row.modelData.name
                        color: Theme.text
                        font.pixelSize: Theme.fontNormal
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        visible: (row.modelData.shortcut || "") !== ""
                        radius: 4
                        color: Theme.surfaceHi
                        implicitWidth: keyText.implicitWidth + 12
                        implicitHeight: 20
                        Text {
                            id: keyText
                            anchors.centerIn: parent
                            text: row.modelData.shortcut || ""
                            color: Theme.textDim
                            font.pixelSize: Theme.fontSmall
                        }
                    }
                }
                HoverHandler { id: rowHover }
                TapHandler { onTapped: root.runAt(row.index) }
            }
            Text {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("No commands found")
                color: Theme.textFaint
            }
        }
    }
}
