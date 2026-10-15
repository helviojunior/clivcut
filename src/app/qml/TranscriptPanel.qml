import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import Clivcut

// Edição pela transcrição: o texto falado da timeline, palavra a palavra.
// Clicar leva o cursor à palavra; apagar palavras corta o trecho correspondente do vídeo.
FocusScope {
    id: root

    Settings {
        id: prefs
        category: "captions" // mesmo idioma e modelo das legendas automáticas
        property int language: 1
        property string model: "base"
    }

    readonly property var languages: [
        { code: "auto", name: qsTr("Detect automatically") },
        { code: "pt", name: "Português" }, { code: "en", name: "English" }, { code: "es", name: "Español" },
        { code: "fr", name: "Français" }, { code: "de", name: "Deutsch" }, { code: "it", name: "Italiano" },
        { code: "ja", name: "日本語" }, { code: "ko", name: "한국어" }, { code: "zh", name: "中文" },
        { code: "ru", name: "Русский" }, { code: "hi", name: "हिन्दी" }, { code: "ar", name: "العربية" }
    ]
    readonly property var installedModels: Project.captionModels.filter(m => m.installed)
    readonly property var words: Project.transcript

    // Seleção: índices das palavras (objeto usado como conjunto) e âncora para Shift+clique.
    property var selected: ({})
    property int anchor: -1
    readonly property int selectedCount: Object.keys(selected).length

    // Palavra sob o cursor de reprodução (busca binária; evita uma ligação por palavra).
    readonly property int currentIndex: {
        const t = Project.playhead;
        let lo = 0, hi = words.length - 1, found = -1;
        while (lo <= hi) {
            const mid = (lo + hi) >> 1;
            if (words[mid].start <= t) { found = mid; lo = mid + 1; } else hi = mid - 1;
        }
        return found >= 0 && t < words[found].end + 150000 ? found : -1;
    }

    function clearSelection() { selected = {}; anchor = -1 }
    function clicked(i, mods) {
        const s = (mods & Qt.ControlModifier) ? Object.assign({}, selected) : {};
        if ((mods & Qt.ShiftModifier) && anchor >= 0) {
            for (let k = Math.min(anchor, i); k <= Math.max(anchor, i); ++k)
                s[k] = true;
        } else if ((mods & Qt.ControlModifier) && s[i]) {
            delete s[i];
        } else {
            s[i] = true;
            anchor = i;
        }
        selected = s;
        Project.seek(words[i].start);
    }
    function removeSelected() {
        const idx = Object.keys(selected).map(Number);
        if (idx.length > 0 && Project.removeTranscriptWords(idx))
            clearSelection();
    }
    function selectFillers() {
        const s = {};
        for (const i of Project.fillerWordIndices())
            s[i] = true;
        selected = s;
    }

    Connections {
        target: Project
        function onTranscriptChanged() { if (Project.transcript.length === 0) root.clearSelection() }
    }

    Keys.onDeletePressed: removeSelected()
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Backspace) { removeSelected(); event.accepted = true }
        else if (event.key === Qt.Key_Escape) { clearSelection(); event.accepted = true }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textFaint
            font.pixelSize: Theme.fontSmall
            text: !Project.transcriptionAvailable ? qsTr("This build was compiled without whisper.cpp.")
                  : root.words.length === 0
                    ? qsTr("Transcribe the timeline to edit the video by its text: deleting words cuts that part of the video, with ripple.")
                    : qsTr("Click a word to jump to it; Shift and Ctrl select more. Delete removes the selected words from the video.")
        }

        // Transcrever (ou de novo)
        GridLayout {
            visible: root.words.length === 0 || Project.transcriptStale
            enabled: Project.transcriptionAvailable && !Project.transcribing
            columns: 2
            columnSpacing: 10
            rowSpacing: 8
            Layout.fillWidth: true
            Text { text: qsTr("Language"); color: Theme.textDim; font.pixelSize: Theme.fontSmall }
            ComboBox {
                Layout.fillWidth: true
                focusPolicy: Qt.NoFocus
                model: root.languages
                textRole: "name"
                currentIndex: prefs.language
                onActivated: (i) => prefs.language = i
            }
            Text { text: qsTr("Model"); color: Theme.textDim; font.pixelSize: Theme.fontSmall }
            ComboBox {
                id: modelBox
                Layout.fillWidth: true
                focusPolicy: Qt.NoFocus
                model: root.installedModels
                textRole: "label"
                currentIndex: Math.max(0, root.installedModels.findIndex(m => m.id === prefs.model))
                onActivated: (i) => prefs.model = root.installedModels[i].id
                displayText: root.installedModels.length === 0 ? qsTr("No model installed") : currentText
            }
        }
        Text {
            visible: root.installedModels.length === 0 && Project.transcriptionAvailable
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.warning
            font.pixelSize: Theme.fontSmall
            text: qsTr("Download a speech recognition model in the Captions panel first.")
        }
        Rectangle {
            visible: Project.transcriptStale
            Layout.fillWidth: true
            implicitHeight: staleText.implicitHeight + 16
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.warning
            Text {
                id: staleText
                anchors.fill: parent
                anchors.margins: 8
                wrapMode: Text.Wrap
                color: Theme.warning
                font.pixelSize: Theme.fontSmall
                text: qsTr("The timeline changed after the transcription. Transcribe again before cutting by text.")
            }
        }
        PillButton {
            visible: root.words.length === 0 || Project.transcriptStale
            enabled: Project.transcriptionAvailable && !Project.transcribing && root.installedModels.length > 0
            primary: true
            iconName: "file-text"
            text: root.words.length === 0 ? qsTr("Transcribe timeline") : qsTr("Transcribe again")
            onClicked: Project.generateTranscript(root.installedModels[Math.max(0, modelBox.currentIndex)].id,
                                                  root.languages[prefs.language].code)
        }
        ProgressBar {
            visible: Project.transcribing
            Layout.fillWidth: true
            value: Project.transcribeProgress
        }

        // Ações sobre a seleção
        Flow {
            visible: root.words.length > 0 && !Project.transcriptStale
            Layout.fillWidth: true
            spacing: 6
            PillButton {
                iconName: "trash-2"
                text: root.selectedCount > 0 ? qsTr("Remove %n word(s)", "", root.selectedCount) : qsTr("Remove")
                enabled: root.selectedCount > 0
                onClicked: root.removeSelected()
            }
            PillButton {
                iconName: "sparkles"
                text: qsTr("Select fillers")
                onClicked: root.selectFillers()
            }
            IconButton {
                iconName: "x"
                tip: qsTr("Close transcript")
                onClicked: Project.clearTranscript()
            }
        }

        // Texto
        Flickable {
            id: flick
            visible: root.words.length > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: flow.implicitHeight
            ScrollBar.vertical: ScrollBar {}
            Flow {
                id: flow
                width: flick.width - 10
                spacing: 3
                Repeater {
                    model: root.words
                    Rectangle {
                        id: word
                        required property var modelData
                        required property int index
                        readonly property bool isSelected: root.selected[index] === true
                        readonly property bool isCurrent: root.currentIndex === index
                        width: label.implicitWidth + 6
                        height: label.implicitHeight + 4
                        radius: 3
                        color: isSelected ? Theme.danger : (isCurrent ? Theme.accentSoft : (wordHover.hovered ? Theme.surface : "transparent"))
                        Text {
                            id: label
                            anchors.centerIn: parent
                            text: word.modelData.text
                            color: word.isSelected ? "#ffffff" : (Project.transcriptStale ? Theme.textFaint : Theme.text)
                            font.pixelSize: Theme.fontNormal
                            font.strikeout: word.isSelected
                        }
                        HoverHandler { id: wordHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            acceptedModifiers: Qt.KeyboardModifierMask
                            onTapped: (point) => {
                                root.forceActiveFocus();
                                root.clicked(word.index, point.modifiers);
                            }
                        }
                    }
                }
            }
        }
    }
}
