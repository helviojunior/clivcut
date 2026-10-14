import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Clivcut

// Editor de atalhos: busca por ação, captura da nova tecla e troca automática em caso de conflito.
Popup {
    id: root
    modal: true
    focus: true
    width: Math.min(620, parent ? parent.width - 80 : 620)
    height: Math.min(640, parent ? parent.height - 80 : 640)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    padding: 16
    background: Rectangle { radius: 12; color: Theme.panel; border.color: Theme.border }

    property string capturing: ""   // ação aguardando a nova tecla
    property string notice: ""

    function normalize(s) { return s.toLowerCase().normalize("NFD").replace(/[̀-ͯ]/g, "") }
    readonly property var filtered: {
        const q = normalize(search.text.trim());
        return Keymap.actions.filter(a => q === "" || normalize(a.name).indexOf(q) >= 0
                                          || Keymap.keys(a.id).some(k => normalize(Project.shortcutText(k)).indexOf(q) >= 0));
    }

    onOpened: {
        capturing = "";
        notice = "";
        search.text = "";
        search.forceActiveFocus();
    }
    onClosed: capturing = ""

    contentItem: ColumnLayout {
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: qsTr("Keyboard shortcuts")
                color: Theme.text
                font.pixelSize: Theme.fontLarge
                font.bold: true
            }
            Button {
                text: qsTr("Reset all")
                flat: true
                focusPolicy: Qt.NoFocus
                enabled: Object.keys(Keymap.overrides).length > 0
                onClicked: {
                    Keymap.resetAll();
                    root.notice = "";
                }
            }
            IconButton { iconName: "x"; tip: qsTr("Close"); onClicked: root.close() }
        }
        TextField {
            id: search
            Layout.fillWidth: true
            placeholderText: qsTr("Filter by action or key…")
            color: Theme.text
            background: Rectangle { radius: Theme.radius; color: Theme.surface; border.color: search.activeFocus ? Theme.accent : Theme.border }
        }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: root.notice !== "" ? Theme.warning : Theme.textFaint
            font.pixelSize: Theme.fontSmall
            text: root.notice !== "" ? root.notice
                  : qsTr("Click + to add a shortcut and press the keys. Esc cancels. Keys already in use move to the new action.")
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.filtered
            spacing: 2
            ScrollBar.vertical: ScrollBar {}
            delegate: Column {
                id: entry
                required property var modelData
                required property int index
                // Cabeçalho do grupo quando ele muda em relação à linha anterior.
                readonly property bool firstOfGroup: index === 0 || root.filtered[index - 1].group !== modelData.group
                width: list.width - 12
                Text {
                    visible: entry.firstOfGroup
                    text: entry.modelData.group
                    color: Theme.accent
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                    topPadding: entry.index === 0 ? 0 : 10
                    bottomPadding: 4
                }
            Rectangle {
                id: row
                readonly property var modelData: entry.modelData
                readonly property bool editing: root.capturing === modelData.id
                width: entry.width
                height: 36
                radius: Theme.radius
                color: editing ? Theme.accentSoft : (rowHover.hovered ? Theme.surface : "transparent")
                HoverHandler { id: rowHover }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 6
                    spacing: 6
                    Text {
                        Layout.fillWidth: true
                        text: row.modelData.name
                        color: Theme.text
                        font.pixelSize: Theme.fontNormal
                        font.bold: Keymap.isCustom(row.modelData.id)
                        elide: Text.ElideRight
                    }
                    Repeater {
                        model: Keymap.keys(row.modelData.id)
                        Rectangle {
                            id: chip
                            required property string modelData
                            radius: 4
                            color: Theme.surfaceHi
                            implicitWidth: chipRow.implicitWidth + 10
                            implicitHeight: 24
                            Row {
                                id: chipRow
                                anchors.centerIn: parent
                                spacing: 4
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: Project.shortcutText(chip.modelData)
                                    color: Theme.text
                                    font.pixelSize: Theme.fontSmall
                                }
                                Icon {
                                    anchors.verticalCenter: parent.verticalCenter
                                    name: "x"
                                    size: 11
                                    color: removeHover.hovered ? Theme.danger : Theme.textFaint
                                    HoverHandler { id: removeHover; cursorShape: Qt.PointingHandCursor }
                                    TapHandler { onTapped: Keymap.remove(row.modelData.id, chip.modelData) }
                                }
                            }
                        }
                    }
                    Text {
                        visible: row.editing
                        text: qsTr("Press the keys…")
                        color: Theme.accent
                        font.pixelSize: Theme.fontSmall
                    }
                    IconButton {
                        iconName: "plus"
                        iconSize: 14
                        implicitWidth: 28
                        tip: qsTr("Add shortcut")
                        active: row.editing
                        onClicked: {
                            root.notice = "";
                            root.capturing = row.editing ? "" : row.modelData.id;
                            catcher.forceActiveFocus();
                        }
                    }
                    IconButton {
                        iconName: "rotate-cw"
                        iconSize: 14
                        implicitWidth: 28
                        opacity: Keymap.isCustom(row.modelData.id) ? 1 : 0
                        enabled: Keymap.isCustom(row.modelData.id)
                        tip: qsTr("Restore default")
                        onClicked: Keymap.reset(row.modelData.id)
                    }
                }
            }
            }
        }
    }

    // Recebe as teclas durante a captura (os atalhos globais ficam desligados enquanto isso).
    Item {
        id: catcher
        focus: root.capturing !== ""
        Keys.onPressed: (event) => {
            if (root.capturing === "")
                return;
            event.accepted = true;
            const k = event.key;
            if (k === Qt.Key_Shift || k === Qt.Key_Control || k === Qt.Key_Alt || k === Qt.Key_Meta
                    || k === Qt.Key_AltGr || k === Qt.Key_unknown)
                return; // espera a tecla principal
            if (k === Qt.Key_Escape && event.modifiers === Qt.NoModifier) {
                root.capturing = "";
                return;
            }
            const seq = Project.keySequence(k, event.modifiers);
            const moved = Keymap.assign(root.capturing, seq);
            root.notice = moved !== "" ? qsTr("%1 was removed from “%2”.").arg(Project.shortcutText(seq)).arg(moved) : "";
            root.capturing = "";
            search.forceActiveFocus();
        }
    }
}
