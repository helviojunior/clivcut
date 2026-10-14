import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtCore
import Clivcut

ApplicationWindow {
    id: window
    width: 1480
    height: 920
    minimumWidth: 860
    minimumHeight: 600
    visible: true
    color: Theme.bg
    title: (Project.modified ? "• " : "") + Project.projectName + " — clivcut"

    palette.window: Theme.panel
    palette.windowText: Theme.text
    palette.base: Theme.surface
    palette.text: Theme.text
    palette.button: Theme.surface
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: "#000000"
    palette.toolTipBase: Theme.surfaceHi
    palette.toolTipText: Theme.text
    palette.placeholderText: Theme.textFaint
    palette.mid: Theme.border
    palette.light: Theme.surfaceHi
    palette.dark: Theme.bg

    // Atalhos não disparam enquanto se digita num campo de texto.
    readonly property bool typing: activeFocusItem !== null && activeFocusItem.hasOwnProperty("cursorPosition")

    Settings {
        id: settings
        category: "window"
        property alias x: window.x
        property alias y: window.y
        property alias width: window.width
        property alias height: window.height
        property string lastFolder: StandardPaths.writableLocation(StandardPaths.MoviesLocation)
    }

    // Ações que trocam/fecham o projeto passam por aqui: pergunta antes de perder alterações.
    property string pendingAction: ""
    property bool closeConfirmed: false
    function guarded(action) {
        if (!Project.modified) {
            runAction(action);
            return;
        }
        pendingAction = action;
        unsavedDialog.open();
    }
    function runAction(action) {
        pendingAction = "";
        if (action === "new") newDialog.open();
        else if (action === "open") openDialog.open();
        else if (action === "draft") draftDialog.open();
        else if (action === "quit") {
            closeConfirmed = true;
            window.close();
        }
    }
    onClosing: (close) => {
        if (!closeConfirmed && Project.modified) {
            close.accepted = false;
            guarded("quit");
        }
    }
    Component.onCompleted: Qt.callLater(() => {
        recoveryDialog.refresh();
        if (recoveryDialog.items.length > 0)
            recoveryDialog.open();
    })

    header: TopBar {
        onNewRequested: window.guarded("new")
        onOpenRequested: window.guarded("open")
        onSaveRequested: window.save()
        onSaveAsRequested: saveDialog.open()
        onImportDraftRequested: window.guarded("draft")
        onExportRequested: exportDialog.open()
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Vertical
        handle: Rectangle {
            implicitHeight: 4
            color: SplitHandle.pressed ? Theme.accent : Theme.bg
        }

        SplitView {
            SplitView.fillHeight: true
            SplitView.minimumHeight: 240
            orientation: Qt.Horizontal
            handle: Rectangle {
                implicitWidth: 4
                color: SplitHandle.pressed ? Theme.accent : Theme.bg
            }

            MediaPanel {
                SplitView.preferredWidth: Math.max(220, window.width * 0.22)
                SplitView.minimumWidth: 200
                onImportRequested: mediaDialog.open()
                onImportDraftRequested: window.guarded("draft")
                onRelinkRequested: (assetId) => {
                    relinkDialog.assetId = assetId;
                    relinkDialog.open();
                }
            }
            PreviewPanel {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 260
            }
            Inspector {
                SplitView.preferredWidth: Math.max(250, window.width * 0.23)
                SplitView.minimumWidth: 230
            }
        }

        Timeline {
            id: timeline
            SplitView.preferredHeight: Math.max(220, window.height * 0.36)
            SplitView.minimumHeight: 160
        }
    }

    // --- arrastar arquivos do gerenciador para a janela ---
    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: (drop) => {
            if (!drop.hasUrls)
                return;
            const files = [];
            for (const u of drop.urls) {
                const s = u.toString();
                if (s.endsWith(".clivcut") || s.indexOf("draft_content.json") >= 0 || s.indexOf("draft_info.json") >= 0)
                    Project.openProject(u);
                else
                    files.push(u);
            }
            if (files.length > 0)
                Project.importMedia(files);
            drop.accept();
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: 8
            visible: parent.containsDrag
            color: "#2022d3c5"
            border.color: Theme.accent
            border.width: 2
            radius: 12
            Text {
                anchors.centerIn: parent
                text: qsTr("Solte para importar")
                color: Theme.text
                font.pixelSize: 22
            }
        }
    }

    // --- atalhos ---
    Shortcut { sequences: ["Space"]; enabled: !window.typing; onActivated: Project.togglePlay() }
    Shortcut { sequences: ["Ctrl+B"]; enabled: !window.typing; onActivated: Project.splitAtPlayhead() }
    Shortcut { sequences: ["Q"]; enabled: !window.typing; onActivated: Project.deleteLeft() }
    Shortcut { sequences: ["W"]; enabled: !window.typing; onActivated: Project.deleteRight() }
    Shortcut { sequences: [StandardKey.Delete, "Backspace"]; enabled: !window.typing; onActivated: Project.deleteSelection(false) }
    Shortcut { sequences: ["Shift+Delete"]; enabled: !window.typing; onActivated: Project.deleteSelection(true) }
    Shortcut { sequences: [StandardKey.Undo]; enabled: !window.typing; onActivated: Project.undo() }
    Shortcut { sequences: [StandardKey.Redo, "Ctrl+Shift+Z"]; enabled: !window.typing; onActivated: Project.redo() }
    Shortcut { sequences: ["Ctrl+D"]; enabled: !window.typing; onActivated: Project.duplicateSelection() }
    Shortcut { sequences: [StandardKey.Copy]; enabled: !window.typing; onActivated: Project.copySelection() }
    Shortcut { sequences: [StandardKey.Cut]; enabled: !window.typing; onActivated: Project.cutSelection() }
    Shortcut { sequences: [StandardKey.Paste]; enabled: !window.typing; onActivated: Project.paste() }
    Shortcut { sequences: ["Ctrl+Alt+C"]; enabled: !window.typing; onActivated: Project.copyAttributes() }
    Shortcut { sequences: ["Ctrl+Alt+V"]; enabled: !window.typing; onActivated: Project.pasteAttributesToSelection() }
    Shortcut { sequences: ["Ctrl+G"]; enabled: !window.typing; onActivated: Project.groupSelection() }
    Shortcut { sequences: ["Ctrl+Shift+D"]; enabled: !window.typing; onActivated: Project.detachAudio() }
    Shortcut { sequences: ["Ctrl+Shift+G"]; enabled: !window.typing; onActivated: Project.ungroupSelection() }
    Shortcut { sequences: [StandardKey.SelectAll]; enabled: !window.typing; onActivated: Project.selectAll() }
    Shortcut { sequences: ["Left"]; enabled: !window.typing; onActivated: Project.stepFrames(-1) }
    Shortcut { sequences: ["Right"]; enabled: !window.typing; onActivated: Project.stepFrames(1) }
    Shortcut { sequences: ["Shift+Left"]; enabled: !window.typing; onActivated: Project.stepFrames(-Math.round(Project.fps)) }
    Shortcut { sequences: ["Shift+Right"]; enabled: !window.typing; onActivated: Project.stepFrames(Math.round(Project.fps)) }
    Shortcut { sequences: ["Home"]; enabled: !window.typing; onActivated: Project.jumpToEdge(false) }
    Shortcut { sequences: ["End"]; enabled: !window.typing; onActivated: Project.jumpToEdge(true) }
    Shortcut { sequences: ["M"]; enabled: !window.typing; onActivated: Project.addMarker() }
    Shortcut { sequences: ["Alt+F"]; enabled: !window.typing; onActivated: Project.freezeFrame() }
    Shortcut { sequences: ["Ctrl+T"]; enabled: !window.typing; onActivated: Project.addText("", "") }
    Shortcut { sequences: ["N"]; enabled: !window.typing; onActivated: Project.snapping = !Project.snapping }
    Shortcut { sequences: ["Ctrl+=", "Ctrl++"]; enabled: !window.typing; onActivated: timeline.zoomBy(1.25) }
    Shortcut { sequences: ["Ctrl+-"]; enabled: !window.typing; onActivated: timeline.zoomBy(0.8) }
    Shortcut { sequences: ["Shift+Z"]; enabled: !window.typing; onActivated: { timeline.userZoomed = false; timeline.zoomToFit() } }
    Shortcut { sequences: ["Escape"]; enabled: !window.typing; onActivated: Project.clearSelection() }
    Shortcut { sequences: [StandardKey.Save]; onActivated: window.save() }
    Shortcut { sequences: [StandardKey.SaveAs]; onActivated: saveDialog.open() }
    Shortcut { sequences: [StandardKey.Open]; onActivated: window.guarded("open") }
    Shortcut { sequences: [StandardKey.New]; onActivated: window.guarded("new") }
    Shortcut { sequences: ["Ctrl+I"]; onActivated: mediaDialog.open() }
    Shortcut { sequences: ["Ctrl+E"]; onActivated: exportDialog.open() }

    function save() {
        if (Project.filePath === "")
            saveDialog.open();
        else
            Project.saveProject();
    }

    // --- diálogos ---
    FileDialog {
        id: mediaDialog
        title: qsTr("Importar mídia")
        fileMode: FileDialog.OpenFiles
        currentFolder: settings.lastFolder
        nameFilters: [qsTr("Mídia (*.mp4 *.mov *.mkv *.webm *.avi *.m4v *.mts *.mp3 *.wav *.m4a *.aac *.flac *.ogg *.opus *.png *.jpg *.jpeg *.webp *.gif *.bmp *.tif *.tiff *.heic)"), qsTr("Todos os arquivos (*)")]
        onAccepted: {
            settings.lastFolder = currentFolder;
            Project.importMedia(selectedFiles);
        }
    }
    FileDialog {
        id: openDialog
        title: qsTr("Abrir projeto")
        nameFilters: [qsTr("Projetos clivcut (*.clivcut)"), qsTr("Draft JSON (draft_content.json draft_info.json)")]
        onAccepted: Project.openProject(selectedFile)
    }
    FileDialog {
        id: saveDialog
        title: qsTr("Salvar projeto")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "clivcut"
        nameFilters: [qsTr("Projetos clivcut (*.clivcut)")]
        onAccepted: {
            if (Project.saveProjectAs(selectedFile) && window.pendingAction !== "")
                window.runAction(window.pendingAction);
        }
        onRejected: window.pendingAction = ""
    }
    UnsavedDialog {
        id: unsavedDialog
        onSaveRequested: {
            if (Project.filePath === "")
                saveDialog.open(); // continua a ação depois de salvar
            else if (Project.saveProject())
                window.runAction(window.pendingAction);
        }
        onDiscardRequested: {
            Project.discardChanges();
            window.runAction(window.pendingAction);
        }
        onRejected: window.pendingAction = ""
    }
    RecoveryDialog { id: recoveryDialog }
    FolderDialog {
        id: draftDialog
        title: qsTr("Pasta do draft")
        onAccepted: Project.importDraft(selectedFolder)
    }
    FileDialog {
        id: relinkDialog
        property string assetId
        title: qsTr("Reconectar mídia")
        onAccepted: Project.relinkAsset(assetId, selectedFile)
    }
    ExportDialog { id: exportDialog }
    NewProjectDialog { id: newDialog }

    // --- mensagens ---
    Connections {
        target: Project
        function onErrorOccurred(message) { toast.show(message, true) }
        function onStatusMessageChanged() { toast.show(Project.statusMessage, false) }
        function onExportFinished(ok, message) {
            toast.show(ok ? qsTr("Vídeo exportado: %1").arg(message) : message, !ok);
        }
    }

    Rectangle {
        id: toast
        property bool error: false
        function show(msg, isError) {
            label.text = msg;
            error = isError;
            opacity = 1;
            hideTimer.restart();
        }
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        width: Math.min(label.implicitWidth + 32, window.width - 80)
        height: label.implicitHeight + 18
        radius: 8
        color: error ? "#5a1f27" : Theme.surfaceHi
        border.color: error ? Theme.danger : Theme.border
        opacity: 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 180 } }
        Text {
            id: label
            anchors.centerIn: parent
            width: Math.min(implicitWidth, window.width - 112)
            color: Theme.text
            font.pixelSize: Theme.fontNormal
            wrapMode: Text.Wrap
        }
        Timer {
            id: hideTimer
            interval: 4000
            onTriggered: toast.opacity = 0
        }
    }
}
