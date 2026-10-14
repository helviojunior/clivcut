import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtCore
import Clivcut

Flickable {
    id: root
    contentHeight: col.implicitHeight + 24
    clip: true
    ScrollBar.vertical: ScrollBar {}

    Settings {
        id: prefs
        category: "captions"
        property alias language: langBox.currentIndex
        property string model: "base"
        property alias replace: replaceBox.checked
    }

    readonly property var languages: [
        { code: "auto", name: qsTr("Detectar automaticamente") },
        { code: "pt", name: "Português" }, { code: "en", name: "English" }, { code: "es", name: "Español" },
        { code: "fr", name: "Français" }, { code: "de", name: "Deutsch" }, { code: "it", name: "Italiano" },
        { code: "ja", name: "日本語" }, { code: "ko", name: "한국어" }, { code: "zh", name: "中文" },
        { code: "ru", name: "Русский" }, { code: "hi", name: "हिन्दी" }, { code: "ar", name: "العربية" }
    ]
    readonly property var selectedModel: Project.captionModels.find(m => m.id === prefs.model) || Project.captionModels[0]

    ColumnLayout {
        id: col
        x: 12
        y: 10
        width: root.width - 24
        spacing: 8

        SectionLabel { text: qsTr("Legendas automáticas") }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textFaint
            font.pixelSize: Theme.fontSmall
            text: Project.transcriptionAvailable
                  ? qsTr("Reconhecimento de fala 100% local (whisper.cpp): o áudio não sai do seu computador.")
                  : qsTr("Este build foi compilado sem whisper.cpp.")
        }

        GridLayout {
            columns: 2
            columnSpacing: 10
            rowSpacing: 8
            Layout.fillWidth: true
            enabled: Project.transcriptionAvailable && !Project.transcribing

            Text { text: qsTr("Idioma"); color: Theme.textDim; font.pixelSize: Theme.fontSmall }
            ComboBox {
                id: langBox
                Layout.fillWidth: true
                focusPolicy: Qt.NoFocus
                model: root.languages
                textRole: "name"
                currentIndex: 1
            }
            Text { text: qsTr("Áudio"); color: Theme.textDim; font.pixelSize: Theme.fontSmall }
            ComboBox {
                id: sourceBox
                Layout.fillWidth: true
                focusPolicy: Qt.NoFocus
                model: [{ index: -1, name: qsTr("Toda a timeline") }].concat(Project.audioTracks)
                textRole: "name"
            }
        }

        // Modelos
        Text {
            text: qsTr("Modelo")
            color: Theme.textDim
            font.pixelSize: Theme.fontSmall
            Layout.topMargin: 4
        }
        Repeater {
            model: Project.captionModels
            Rectangle {
                id: modelRow
                required property var modelData
                readonly property bool chosen: prefs.model === modelData.id
                readonly property bool downloading: Project.downloadingModel === modelData.id
                Layout.fillWidth: true
                Layout.maximumWidth: col.width
                implicitHeight: 44
                radius: Theme.radius
                color: chosen ? Theme.surfaceHi : Theme.surface
                border.color: chosen ? Theme.accent : "transparent"
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    enabled: !Project.transcribing
                    onClicked: prefs.model = modelRow.modelData.id
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 8
                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Text {
                            Layout.fillWidth: true
                            text: modelRow.modelData.label + "  ·  " + modelRow.modelData.sizeMb + " MB"
                            color: Theme.text
                            font.pixelSize: Theme.fontNormal
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelRow.modelData.description
                            color: Theme.textFaint
                            font.pixelSize: Theme.fontSmall
                            elide: Text.ElideRight
                        }
                    }
                    Text {
                        visible: modelRow.modelData.installed
                        text: "✓"
                        color: Theme.accent
                        font.pixelSize: 16
                    }
                    Button {
                        visible: !modelRow.modelData.installed && !modelRow.downloading
                        enabled: Project.downloadingModel === ""
                        text: qsTr("Baixar")
                        focusPolicy: Qt.NoFocus
                        onClicked: {
                            prefs.model = modelRow.modelData.id;
                            Project.downloadModel(modelRow.modelData.id);
                        }
                    }
                    RowLayout {
                        visible: modelRow.downloading
                        ProgressBar {
                            Layout.preferredWidth: 70
                            value: Project.modelDownloadProgress
                        }
                        IconButton {
                            iconName: "x"
                            tip: qsTr("Cancelar download")
                            onClicked: Project.cancelModelDownload()
                        }
                    }
                }
            }
        }

        CheckBox {
            id: replaceBox
            text: qsTr("Substituir legendas existentes")
            checked: true
            focusPolicy: Qt.NoFocus
        }

        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            visible: !Project.transcribing
            enabled: Project.transcriptionAvailable && root.selectedModel !== undefined && root.selectedModel.installed && Project.duration > 0
            highlighted: true
            focusPolicy: Qt.NoFocus
            text: root.selectedModel !== undefined && !root.selectedModel.installed ? qsTr("Baixe o modelo para continuar")
                                                                                   : qsTr("Gerar legendas")
            onClicked: Project.generateCaptions(prefs.model, root.languages[langBox.currentIndex].code,
                                                sourceBox.model[sourceBox.currentIndex].index, replaceBox.checked)
        }
        RowLayout {
            visible: Project.transcribing
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                Text {
                    text: Project.transcribeProgress < 0.15 ? qsTr("Extraindo áudio…")
                                                             : qsTr("Reconhecendo fala… %1%").arg(Math.round(Project.transcribeProgress * 100))
                    color: Theme.text
                    font.pixelSize: Theme.fontSmall
                }
                ProgressBar {
                    Layout.fillWidth: true
                    value: Project.transcribeProgress
                    indeterminate: Project.transcribeProgress > 0.15 && Project.transcribeProgress < 0.16
                }
            }
            Button {
                text: qsTr("Cancelar")
                focusPolicy: Qt.NoFocus
                onClicked: Project.cancelTranscription()
            }
        }

        SectionLabel { text: qsTr("Arquivos de legenda") }
        RowLayout {
            Layout.fillWidth: true
            Button {
                Layout.fillWidth: true
                text: qsTr("Importar SRT/VTT")
                focusPolicy: Qt.NoFocus
                onClicked: importDialog.open()
            }
            Button {
                Layout.fillWidth: true
                text: qsTr("Exportar SRT")
                focusPolicy: Qt.NoFocus
                onClicked: exportDialog.open()
            }
        }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textFaint
            font.pixelSize: Theme.fontSmall
            text: qsTr("As legendas viram clipes de texto na faixa \"Legendas\": edite o texto e o tempo direto na timeline. Para mudar o visual de todas, selecione uma, ajuste o estilo e use \"Aplicar a todas as legendas\".")
        }
    }

    FileDialog {
        id: importDialog
        title: qsTr("Importar legendas")
        nameFilters: [qsTr("Legendas (*.srt *.vtt)")]
        onAccepted: Project.importSubtitles(selectedFile)
    }
    FileDialog {
        id: exportDialog
        title: qsTr("Exportar legendas")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "srt"
        nameFilters: [qsTr("SubRip (*.srt)")]
        onAccepted: Project.exportSubtitles(selectedFile)
    }
}
