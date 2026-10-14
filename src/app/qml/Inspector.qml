import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Clivcut

Rectangle {
    id: root
    color: Theme.panel

    readonly property var sel: Project.selectedClip
    readonly property bool hasSel: sel.id !== undefined
    readonly property bool isText: hasSel && sel.type === "text"
    readonly property bool isMedia: hasSel && sel.type === "media"
    readonly property bool isAdjust: hasSel && sel.type === "adjust"
    readonly property bool hasColor: isAdjust || (isMedia && sel.mediaKind !== "audio")
    readonly property bool isAudioOnly: isMedia && (sel.mediaKind === "audio" || sel.audioOnly)
    readonly property bool hasVisual: hasSel && !isAudioOnly && !isAdjust

    function setProp(key, value) { Project.setClipProperty(sel.id, key, value) }

    // Abas disponíveis conforme o tipo de clipe.
    readonly property var pages: !hasSel ? ["project"]
        : isText ? ["text", "transform", "keyframes", "animation"]
        : isAdjust ? ["color", "mask", "keyframes"]
        : isAudioOnly ? ["audio", "speed", "keyframes"]
        : sel.mediaKind === "image" ? ["transform", "color", "mask", "keyframes", "advanced"]
        : ["transform", "color", "mask", "keyframes", "audio", "speed", "advanced"]
    readonly property var pageNames: ({
        project: qsTr("Projeto"), text: qsTr("Texto"), transform: qsTr("Vídeo"), color: isAdjust ? qsTr("Ajuste/Filtro") : qsTr("Ajustes"),
        audio: qsTr("Áudio"), speed: qsTr("Velocidade"), advanced: qsTr("Avançado"), animation: qsTr("Animação"),
        mask: qsTr("Máscara"), keyframes: qsTr("Keyframes")
    })
    property string page: pages[0]
    onPagesChanged: {
        if (Project.screenshotPage !== "" && pages.indexOf(Project.screenshotPage) >= 0)
            page = Project.screenshotPage;
        else if (pages.indexOf(page) < 0)
            page = pages[0];
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Abas
        Flow {
            Layout.fillWidth: true
            Layout.margins: 8
            spacing: 4
            Repeater {
                model: root.pages
                Rectangle {
                    required property string modelData
                    width: tabText.implicitWidth + 18
                    height: 26
                    radius: 13
                    color: root.page === modelData ? Theme.surfaceHi : "transparent"
                    Text {
                        id: tabText
                        anchors.centerIn: parent
                        text: root.pageNames[parent.modelData]
                        color: root.page === parent.modelData ? Theme.text : Theme.textDim
                        font.pixelSize: Theme.fontNormal
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.page = parent.modelData
                    }
                }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: content.implicitHeight + 24
            clip: true
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: content
                x: 12
                y: 8
                width: parent.width - 24
                spacing: 8

                // ---------------- Projeto ----------------
                ColumnLayout {
                    visible: root.page === "project"
                    Layout.fillWidth: true
                    SectionLabel { text: qsTr("Projeto") }
                    GridLayout {
                        columns: 2
                        columnSpacing: 10
                        Text { text: qsTr("Resolução"); color: Theme.textDim; font.pixelSize: Theme.fontSmall }
                        Text { text: Project.canvasWidth + " × " + Project.canvasHeight; color: Theme.text; font.pixelSize: Theme.fontNormal }
                        Text { text: qsTr("Quadros/s"); color: Theme.textDim; font.pixelSize: Theme.fontSmall }
                        Text { text: Project.fps; color: Theme.text; font.pixelSize: Theme.fontNormal }
                        Text { text: qsTr("Duração"); color: Theme.textDim; font.pixelSize: Theme.fontSmall }
                        Text { text: Project.formatTime(Project.duration); color: Theme.text; font.pixelSize: Theme.fontNormal }
                        Text { text: qsTr("Mídias"); color: Theme.textDim; font.pixelSize: Theme.fontSmall }
                        Text { text: Project.assets.length; color: Theme.text; font.pixelSize: Theme.fontNormal }
                    }
                    SectionLabel { text: qsTr("Fundo do canvas") }
                    ColorField {
                        label: qsTr("Cor")
                        color: Project.background
                        onPicked: (c) => Project.setBackground(c === "" ? "#000000" : c)
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.topMargin: 16
                        wrapMode: Text.Wrap
                        color: Theme.textFaint
                        font.pixelSize: Theme.fontSmall
                        text: qsTr("Selecione um clipe na timeline para editar posição, cor, áudio, velocidade e animações.\n\nAtalhos: Espaço reproduz · Ctrl+B divide · Q/W exclui à esquerda/direita · Del exclui · Ctrl+D duplica · M marcador · ←/→ quadro a quadro.")
                    }
                }

                // ---------------- Texto ----------------
                ColumnLayout {
                    visible: root.page === "text" && root.isText
                    Layout.fillWidth: true
                    SectionLabel { text: qsTr("Conteúdo") }
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 80
                        TextArea {
                            id: textEdit
                            text: root.isText ? root.sel.text : ""
                            color: Theme.text
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: textEdit.activeFocus ? Theme.accent : "transparent" }
                            onTextChanged: if (activeFocus && root.isText && text !== root.sel.text) root.setProp("text", text)
                        }
                    }
                    SectionLabel { text: qsTr("Estilo") }
                    RowLayout {
                        Layout.fillWidth: true
                        ComboBox {
                            id: fontBox
                            Layout.fillWidth: true
                            focusPolicy: Qt.NoFocus
                            model: Qt.fontFamilies()
                            currentIndex: root.isText ? Math.max(0, model.indexOf(root.sel.fontFamily)) : 0
                            onActivated: (i) => root.setProp("fontFamily", model[i])
                        }
                        IconButton { iconName: "bold"; tip: qsTr("Negrito"); active: root.isText && root.sel.bold; onClicked: root.setProp("bold", !root.sel.bold) }
                        IconButton { iconName: "italic"; tip: qsTr("Itálico"); active: root.isText && root.sel.italic; onClicked: root.setProp("italic", !root.sel.italic) }
                    }
                    RowLayout {
                        Text { text: qsTr("Alinhamento"); color: Theme.textDim; font.pixelSize: Theme.fontSmall; Layout.preferredWidth: 78 }
                        IconButton { iconName: "text-align-start"; tip: qsTr("Esquerda"); active: root.isText && root.sel.align === 0; onClicked: root.setProp("align", 0) }
                        IconButton { iconName: "text-align-center"; tip: qsTr("Centro"); active: root.isText && root.sel.align === 1; onClicked: root.setProp("align", 1) }
                        IconButton { iconName: "text-align-end"; tip: qsTr("Direita"); active: root.isText && root.sel.align === 2; onClicked: root.setProp("align", 2) }
                    }
                    PropSlider {
                        label: qsTr("Tamanho")
                        from: 10; to: 400; stepSize: 1; decimals: 0
                        value: root.isText ? root.sel.fontSize : 72
                        onMoved: (v) => root.setProp("fontSize", v)
                    }
                    ColorField { label: qsTr("Cor"); color: root.isText ? root.sel.color : "white"; onPicked: (c) => root.setProp("color", c === "" ? "#ffffff" : c) }
                    ColorField { label: qsTr("Contorno"); color: root.isText ? root.sel.strokeColor : "black"; onPicked: (c) => root.setProp("strokeColor", c === "" ? "#000000" : c) }
                    PropSlider {
                        label: qsTr("Espessura")
                        from: 0; to: 20; stepSize: 0.5; decimals: 1
                        value: root.isText ? root.sel.strokeWidth : 0
                        onMoved: (v) => root.setProp("strokeWidth", v)
                    }
                    ColorField {
                        label: qsTr("Fundo")
                        allowNone: true
                        isNone: !root.isText || root.sel.background === ""
                        color: root.isText && root.sel.background !== "" ? root.sel.background : "black"
                        onPicked: (c) => root.setProp("background", c)
                    }
                    CheckBox {
                        text: qsTr("Sombra")
                        checked: root.isText && root.sel.shadow
                        focusPolicy: Qt.NoFocus
                        onToggled: root.setProp("shadow", checked)
                    }
                    Button {
                        visible: root.isText && root.sel.inCaptionTrack
                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        text: qsTr("Aplicar a todas as legendas")
                        focusPolicy: Qt.NoFocus
                        onClicked: Project.applyStyleToTrack(root.sel.id)
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Copia estilo, posição e animações deste texto para toda a faixa")
                    }
                }

                // ---------------- Transformação ----------------
                ColumnLayout {
                    visible: root.page === "transform" && root.hasVisual
                    Layout.fillWidth: true
                    SectionLabel {
                        text: qsTr("Transformação")
                        actionText: qsTr("Redefinir")
                        onActionClicked: Project.resetClipProperties(root.sel.id, "transform")
                    }
                    Repeater {
                        model: [
                            { key: "scale", label: qsTr("Escala"), from: 0.05, to: 5, scale: 100, suffix: "%", dec: 0 },
                            { key: "x", label: qsTr("Posição X"), from: -2, to: 2, scale: 100, suffix: "", dec: 0 },
                            { key: "y", label: qsTr("Posição Y"), from: -2, to: 2, scale: 100, suffix: "", dec: 0 },
                            { key: "rotation", label: qsTr("Rotação"), from: -360, to: 360, scale: 1, suffix: "°", dec: 0 },
                            { key: "opacity", label: qsTr("Opacidade"), from: 0, to: 1, scale: 100, suffix: "%", dec: 0 }
                        ]
                        PropSlider {
                            required property var modelData
                            label: modelData.label
                            from: modelData.from
                            to: modelData.to
                            stepSize: (modelData.to - modelData.from) / 1000
                            displayScale: modelData.scale
                            decimals: modelData.dec
                            suffix: modelData.suffix
                            value: root.hasSel ? root.sel[modelData.key] : 0
                            keyframable: true
                            hasKeyframes: root.hasSel && root.sel["kf_" + modelData.key]
                            keyframeHere: root.hasSel && root.sel["kfAt_" + modelData.key]
                            onMoved: (v) => root.setProp(modelData.key, v)
                            onKeyframeToggled: Project.toggleKeyframe(root.sel.id, modelData.key)
                        }
                    }
                    RowLayout {
                        visible: root.isMedia
                        Text { text: qsTr("Espelhar"); color: Theme.textDim; font.pixelSize: Theme.fontSmall; Layout.preferredWidth: 78 }
                        IconButton { iconName: "flip-horizontal-2"; tip: qsTr("Horizontal"); active: root.hasSel && root.sel.flipH; onClicked: root.setProp("flipH", !root.sel.flipH) }
                        IconButton { iconName: "flip-vertical-2"; tip: qsTr("Vertical"); active: root.hasSel && root.sel.flipV; onClicked: root.setProp("flipV", !root.sel.flipV) }
                    }
                    SectionLabel { visible: root.isMedia; text: qsTr("Cortar (crop)") }
                    Repeater {
                        model: root.isMedia ? [
                            { key: "cropLeft", label: qsTr("Esquerda") }, { key: "cropRight", label: qsTr("Direita") },
                            { key: "cropTop", label: qsTr("Topo") }, { key: "cropBottom", label: qsTr("Base") }
                        ] : []
                        PropSlider {
                            required property var modelData
                            label: modelData.label
                            from: 0; to: 0.9; stepSize: 0.005
                            displayScale: 100; decimals: 0; suffix: "%"
                            value: root.isMedia ? root.sel[modelData.key] : 0
                            onMoved: (v) => root.setProp(modelData.key, v)
                        }
                    }
                }

                // ---------------- Ajustes de cor ----------------
                ColumnLayout {
                    visible: root.page === "color" && root.hasColor
                    Layout.fillWidth: true

                    PropSlider {
                        visible: root.isAdjust
                        label: qsTr("Intensidade")
                        from: 0; to: 1; displayScale: 100; decimals: 0; suffix: "%"
                        value: root.isAdjust ? root.sel.opacity : 1
                        keyframable: true
                        hasKeyframes: root.isAdjust && root.sel.kf_opacity
                        keyframeHere: root.isAdjust && root.sel.kfAt_opacity
                        onMoved: (v) => root.setProp("opacity", v)
                        onKeyframeToggled: Project.toggleKeyframe(root.sel.id, "opacity")
                    }

                    // Filtro
                    SectionLabel {
                        text: root.hasColor && root.sel.filter !== "" ? qsTr("Filtro: %1").arg(root.sel.filterName) : qsTr("Filtro")
                        actionText: root.hasColor && root.sel.filter !== "" ? qsTr("Remover") : ""
                        onActionClicked: Project.applyFilter(root.sel.id, "")
                    }
                    Text {
                        visible: root.hasColor && root.sel.filter === ""
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: qsTr("Escolha um filtro na aba Filtros do painel esquerdo.")
                        color: Theme.textFaint
                        font.pixelSize: Theme.fontSmall
                    }
                    PropSlider {
                        visible: root.hasColor && root.sel.filter !== ""
                        label: qsTr("Intensidade")
                        from: 0; to: 1; displayScale: 100; decimals: 0; suffix: "%"
                        value: root.hasColor ? root.sel.filterIntensity : 0.8
                        onMoved: (v) => root.setProp("filterIntensity", v)
                    }

                    SectionLabel {
                        text: qsTr("Ajustes básicos")
                        actionText: qsTr("Redefinir")
                        onActionClicked: Project.resetClipProperties(root.sel.id, "color")
                    }
                    Repeater {
                        model: [
                            { key: "exposure", label: qsTr("Exposição"), kf: false },
                            { key: "brightness", label: qsTr("Brilho"), kf: true },
                            { key: "contrast", label: qsTr("Contraste"), kf: true },
                            { key: "saturation", label: qsTr("Saturação"), kf: true },
                            { key: "highlights", label: qsTr("Realces"), kf: false },
                            { key: "shadows", label: qsTr("Sombras"), kf: false },
                            { key: "temperature", label: qsTr("Temperatura"), kf: false },
                            { key: "tint", label: qsTr("Matiz"), kf: false }
                        ]
                        PropSlider {
                            required property var modelData
                            label: modelData.label
                            from: -1; to: 1; stepSize: 0.01
                            displayScale: 100; decimals: 0
                            value: root.hasColor ? root.sel[modelData.key] : 0
                            keyframable: modelData.kf
                            hasKeyframes: root.hasColor && !!root.sel["kf_" + modelData.key]
                            keyframeHere: root.hasColor && !!root.sel["kfAt_" + modelData.key]
                            onMoved: (v) => root.setProp(modelData.key, v)
                            onKeyframeToggled: Project.toggleKeyframe(root.sel.id, modelData.key)
                        }
                    }
                    SectionLabel { text: qsTr("Efeitos de imagem") }
                    PropSlider {
                        label: qsTr("Vinheta")
                        from: 0; to: 1; displayScale: 100; decimals: 0
                        value: root.hasColor ? root.sel.vignette : 0
                        onMoved: (v) => root.setProp("vignette", v)
                    }
                    PropSlider {
                        visible: root.isMedia
                        label: qsTr("Nitidez")
                        from: 0; to: 1; displayScale: 100; decimals: 0
                        value: root.hasColor ? root.sel.sharpen : 0
                        onMoved: (v) => root.setProp("sharpen", v)
                    }

                    // LUT do usuário
                    SectionLabel {
                        text: qsTr("LUT (.cube)")
                        actionText: root.hasColor && root.sel.lutPath !== "" ? qsTr("Remover") : ""
                        onActionClicked: root.setProp("lutPath", "")
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: root.hasColor && root.sel.lutPath !== "" ? root.sel.lutName : qsTr("Nenhum LUT carregado")
                            color: root.hasColor && root.sel.lutPath !== "" ? Theme.text : Theme.textFaint
                            font.pixelSize: Theme.fontSmall
                            elide: Text.ElideMiddle
                        }
                        Button {
                            text: qsTr("Carregar…")
                            focusPolicy: Qt.NoFocus
                            onClicked: lutDialog.open()
                        }
                    }
                    PropSlider {
                        visible: root.hasColor && root.sel.lutPath !== ""
                        label: qsTr("Intensidade")
                        from: 0; to: 1; displayScale: 100; decimals: 0; suffix: "%"
                        value: root.hasColor ? root.sel.lutIntensity : 1
                        onMoved: (v) => root.setProp("lutIntensity", v)
                    }
                }

                // ---------------- Máscara ----------------
                ColumnLayout {
                    id: maskPage
                    visible: root.page === "mask" && root.hasColor
                    Layout.fillWidth: true
                    readonly property string shape: root.hasColor ? root.sel.maskShape : "none"
                    SectionLabel {
                        text: qsTr("Máscara")
                        actionText: maskPage.shape !== "none" ? qsTr("Redefinir") : ""
                        onActionClicked: Project.resetClipProperties(root.sel.id, "mask")
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: [
                                { shape: "none", glyph: "∅", name: qsTr("Nenhuma") },
                                { shape: "linear", glyph: "⊟", name: qsTr("Linear") },
                                { shape: "mirror", glyph: "☰", name: qsTr("Espelho") },
                                { shape: "circle", glyph: "◯", name: qsTr("Círculo") },
                                { shape: "rectangle", glyph: "▢", name: qsTr("Retângulo") }
                            ]
                            TileButton {
                                required property var modelData
                                width: 58
                                implicitHeight: 64
                                glyph: modelData.glyph
                                caption: modelData.name
                                selected: maskPage.shape === modelData.shape
                                onClicked: root.setProp("maskShape", modelData.shape)
                            }
                        }
                    }
                    Repeater {
                        model: [
                            { key: "maskX", label: qsTr("Posição X"), from: -0.5, to: 1.5, shapes: ["linear", "mirror", "circle", "rectangle"] },
                            { key: "maskY", label: qsTr("Posição Y"), from: -0.5, to: 1.5, shapes: ["linear", "mirror", "circle", "rectangle"] },
                            { key: "maskWidth", label: qsTr("Largura"), from: 0.01, to: 2, shapes: ["circle", "rectangle"] },
                            { key: "maskHeight", label: qsTr("Altura"), from: 0.01, to: 2, shapes: ["mirror", "circle", "rectangle"] },
                            { key: "maskRotation", label: qsTr("Rotação"), from: -180, to: 180, shapes: ["linear", "mirror", "circle", "rectangle"] },
                            { key: "maskFeather", label: qsTr("Suavização"), from: 0, to: 1, shapes: ["linear", "mirror", "circle", "rectangle"] },
                            { key: "maskRoundness", label: qsTr("Arredondar"), from: 0, to: 1, shapes: ["rectangle"] }
                        ]
                        PropSlider {
                            required property var modelData
                            visible: modelData.shapes.indexOf(maskPage.shape) >= 0
                            label: modelData.label
                            from: modelData.from
                            to: modelData.to
                            stepSize: (modelData.to - modelData.from) / 400
                            displayScale: modelData.key === "maskRotation" ? 1 : 100
                            decimals: 0
                            suffix: modelData.key === "maskRotation" ? "°" : (modelData.key === "maskX" || modelData.key === "maskY" ? "" : "%")
                            value: root.hasColor ? root.sel[modelData.key] : 0
                            onMoved: (v) => root.setProp(modelData.key, v)
                        }
                    }
                    CheckBox {
                        visible: maskPage.shape !== "none"
                        text: qsTr("Inverter")
                        checked: root.hasColor && root.sel.maskInvert
                        focusPolicy: Qt.NoFocus
                        onToggled: root.setProp("maskInvert", checked)
                    }
                    Text {
                        visible: maskPage.shape === "none"
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: Theme.textFaint
                        font.pixelSize: Theme.fontSmall
                        text: root.isAdjust ? qsTr("Com máscara, o ajuste/filtro vale só dentro da forma (ex.: escurecer o céu).")
                                            : qsTr("Mostre só parte do clipe: útil para telas divididas, molduras e efeitos de recorte.")
                    }
                }

                // ---------------- Keyframes ----------------
                ColumnLayout {
                    id: kfPage
                    visible: root.page === "keyframes" && root.hasSel
                    Layout.fillWidth: true
                    readonly property var labels: ({
                        x: qsTr("Posição X"), y: qsTr("Posição Y"), scale: qsTr("Escala"), rotation: qsTr("Rotação"),
                        opacity: root.isAdjust ? qsTr("Intensidade") : qsTr("Opacidade"), volume: qsTr("Volume"),
                        brightness: qsTr("Brilho"), contrast: qsTr("Contraste"), saturation: qsTr("Saturação")
                    })
                    readonly property var animated: root.hasSel
                        ? ["x", "y", "scale", "rotation", "opacity", "volume", "brightness", "contrast", "saturation"].filter(p => root.sel["kf_" + p])
                        : []
                    property string current: ""
                    readonly property string prop: animated.indexOf(current) >= 0 ? current : (animated.length > 0 ? animated[0] : "")

                    SectionLabel { text: qsTr("Propriedades animadas") }
                    Text {
                        visible: kfPage.animated.length === 0
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: Theme.textFaint
                        font.pixelSize: Theme.fontSmall
                        text: qsTr("Nenhuma propriedade animada. Use o ◇ ao lado de Escala, Posição, Opacidade, Volume ou Cor (nas outras abas) para criar keyframes no cursor.")
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 4
                        Repeater {
                            model: kfPage.animated
                            Rectangle {
                                required property string modelData
                                width: propText.implicitWidth + 16
                                height: 24
                                radius: 12
                                color: kfPage.prop === modelData ? Theme.surfaceHi : "transparent"
                                border.color: kfPage.prop === modelData ? Theme.accent : Theme.border
                                Text {
                                    id: propText
                                    anchors.centerIn: parent
                                    text: kfPage.labels[parent.modelData]
                                    color: Theme.text
                                    font.pixelSize: Theme.fontSmall
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: kfPage.current = parent.modelData
                                }
                            }
                        }
                    }
                    KeyframeGraph {
                        visible: kfPage.prop !== ""
                        Layout.fillWidth: true
                        clipId: root.hasSel ? root.sel.id : ""
                        prop: kfPage.prop
                        duration: root.hasSel ? root.sel.duration : 1
                        playheadU: root.hasSel && root.sel.underPlayhead ? root.sel.playheadU : -1
                    }
                }

                // ---------------- Áudio ----------------
                ColumnLayout {
                    visible: root.page === "audio" && root.isMedia
                    Layout.fillWidth: true
                    SectionLabel {
                        text: qsTr("Áudio")
                        actionText: qsTr("Redefinir")
                        onActionClicked: Project.resetClipProperties(root.sel.id, "audio")
                    }
                    Text {
                        visible: root.isMedia && !root.sel.hasAudio
                        text: qsTr("Este clipe não tem áudio.")
                        color: Theme.textFaint
                        font.pixelSize: Theme.fontSmall
                    }
                    RowLayout {
                        visible: root.isMedia && root.sel.hasAudio
                        CheckBox {
                            text: qsTr("Silenciar clipe")
                            checked: root.isMedia && root.sel.audioMuted
                            focusPolicy: Qt.NoFocus
                            onToggled: root.setProp("audioMuted", checked)
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            visible: root.isMedia && root.sel.mediaKind === "video" && !root.sel.audioOnly && !root.sel.audioMuted
                            text: qsTr("Separar áudio")
                            focusPolicy: Qt.NoFocus
                            onClicked: Project.detachAudio()
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Cria um clipe de áudio independente e silencia o vídeo")
                        }
                    }
                    PropSlider {
                        enabled: root.isMedia && root.sel.hasAudio
                        label: qsTr("Volume")
                        from: 0; to: 2; stepSize: 0.01
                        displayScale: 100; decimals: 0; suffix: "%"
                        value: root.isMedia ? root.sel.volume : 1
                        keyframable: true
                        hasKeyframes: root.isMedia && root.sel.kf_volume
                        keyframeHere: root.isMedia && root.sel.kfAt_volume
                        onMoved: (v) => root.setProp("volume", v)
                        onKeyframeToggled: Project.toggleKeyframe(root.sel.id, "volume")
                    }
                    PropSlider {
                        enabled: root.isMedia && root.sel.hasAudio
                        label: qsTr("Ganho")
                        from: -24; to: 24; stepSize: 0.5; decimals: 1; suffix: " dB"
                        value: root.isMedia ? root.sel.gainDb : 0
                        onMoved: (v) => root.setProp("gainDb", v)
                    }
                    RowLayout {
                        visible: root.isMedia && root.sel.hasAudio
                        Layout.fillWidth: true
                        ComboBox {
                            id: loudTarget
                            Layout.fillWidth: true
                            focusPolicy: Qt.NoFocus
                            model: [qsTr("Redes sociais (-14 LUFS)"), qsTr("Podcast (-16 LUFS)"), qsTr("TV/broadcast (-23 LUFS)")]
                            readonly property var values: [-14, -16, -23]
                        }
                        Button {
                            text: Project.normalizing ? qsTr("Medindo…") : qsTr("Normalizar")
                            enabled: !Project.normalizing
                            focusPolicy: Qt.NoFocus
                            onClicked: Project.normalizeLoudness(loudTarget.values[loudTarget.currentIndex])
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Mede a loudness (EBU R128) dos clipes selecionados e ajusta o ganho para o alvo")
                        }
                    }
                    SectionLabel {
                        visible: root.isMedia && root.sel.hasAudio
                        text: qsTr("Redução de ruído")
                    }
                    PropSlider {
                        visible: root.isMedia && root.sel.hasAudio
                        label: qsTr("Intensidade")
                        from: 0; to: 1; stepSize: 0.1; displayScale: 100; decimals: 0; suffix: "%"
                        value: root.isMedia ? root.sel.denoise : 0
                        onMoved: (v) => root.setProp("denoise", v)
                    }
                    Text {
                        visible: root.isMedia && root.sel.hasAudio
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: root.isMedia && root.sel.denoiseState === "ready" ? Theme.accent : Theme.textFaint
                        font.pixelSize: Theme.fontSmall
                        text: !root.isMedia ? ""
                              : root.sel.denoiseState === "processing" ? qsTr("Processando… %1%").arg(Math.round(root.sel.denoiseProgress * 100))
                              : root.sel.denoiseState === "queued" ? qsTr("Na fila…")
                              : root.sel.denoiseState === "ready" ? qsTr("✓ Áudio limpo em uso (processado localmente)")
                              : qsTr("Remove chiado, zumbido e ruído de fundo constante. O processamento roda em segundo plano.")
                    }
                    PropSlider {
                        enabled: root.isMedia && root.sel.hasAudio
                        label: qsTr("Fade in")
                        from: 0; to: 10; stepSize: 0.1; decimals: 1; suffix: "s"
                        value: root.isMedia ? root.sel.fadeIn / 1e6 : 0
                        onMoved: (v) => root.setProp("fadeIn", v * 1e6)
                    }
                    PropSlider {
                        enabled: root.isMedia && root.sel.hasAudio
                        label: qsTr("Fade out")
                        from: 0; to: 10; stepSize: 0.1; decimals: 1; suffix: "s"
                        value: root.isMedia ? root.sel.fadeOut / 1e6 : 0
                        onMoved: (v) => root.setProp("fadeOut", v * 1e6)
                    }
                }

                // ---------------- Velocidade ----------------
                ColumnLayout {
                    visible: root.page === "speed" && root.isMedia
                    Layout.fillWidth: true
                    SectionLabel { text: qsTr("Velocidade") }
                    Text {
                        visible: root.isMedia && root.sel.speed <= 0
                        text: qsTr("Quadro congelado")
                        color: Theme.textDim
                        font.pixelSize: Theme.fontSmall
                    }
                    // Normal | Curva
                    Row {
                        id: speedMode
                        visible: root.isMedia && root.sel.speed > 0 && root.sel.mediaKind !== "image"
                        spacing: 4
                        property bool curve: root.isMedia && root.sel.speedCurve !== undefined && root.sel.speedCurve.length >= 2
                        property bool showCurve: curve
                        onCurveChanged: showCurve = curve
                        Repeater {
                            model: [{ c: false, t: qsTr("Normal") }, { c: true, t: qsTr("Curva") }]
                            Rectangle {
                                required property var modelData
                                width: modeText.implicitWidth + 22
                                height: 26
                                radius: 13
                                color: speedMode.showCurve === modelData.c ? Theme.surfaceHi : "transparent"
                                border.color: speedMode.showCurve === modelData.c ? Theme.accent : Theme.border
                                Text {
                                    id: modeText
                                    anchors.centerIn: parent
                                    text: parent.modelData.t
                                    color: Theme.text
                                    font.pixelSize: Theme.fontSmall
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        speedMode.showCurve = parent.modelData.c;
                                        if (!parent.modelData.c && speedMode.curve)
                                            Project.setSpeed(root.sel.id, root.sel.speed); // volta a constante
                                    }
                                }
                            }
                        }
                    }
                    // --- Normal ---
                    PropSlider {
                        visible: speedMode.visible && !speedMode.showCurve
                        label: qsTr("Velocidade")
                        from: 0.1; to: 10; stepSize: 0.05; decimals: 2; suffix: "x"
                        value: root.isMedia ? root.sel.speed : 1
                        onMoved: (v) => Project.setSpeed(root.sel.id, v)
                    }
                    Flow {
                        visible: speedMode.visible && !speedMode.showCurve
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: [0.25, 0.5, 1, 1.5, 2, 4]
                            Button {
                                required property real modelData
                                text: modelData + "x"
                                focusPolicy: Qt.NoFocus
                                highlighted: root.isMedia && Math.abs(root.sel.speed - modelData) < 0.001
                                onClicked: Project.setSpeed(root.sel.id, modelData)
                            }
                        }
                    }
                    // --- Curva ---
                    Flow {
                        visible: speedMode.visible && speedMode.showCurve
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: Project.speedPresets
                            TileButton {
                                required property var modelData
                                width: 72
                                implicitHeight: 58
                                glyph: "〰"
                                caption: modelData.name
                                onClicked: Project.applySpeedPreset(root.sel.id, modelData.id)
                            }
                        }
                    }
                    CurveEditor {
                        visible: speedMode.visible && speedMode.showCurve
                        Layout.fillWidth: true
                        points: speedMode.curve ? root.sel.speedCurve : [{ u: 0, v: 1 }, { u: 1, v: 1 }]
                        playheadU: root.isMedia && root.sel.underPlayhead ? root.sel.playheadU : -1
                        onEdited: (pts) => Project.setSpeedCurve(root.sel.id, pts)
                    }
                    Text {
                        visible: speedMode.visible && speedMode.showCurve
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: Theme.textFaint
                        font.pixelSize: Theme.fontSmall
                        text: qsTr("Arraste os pontos; duplo clique adiciona ou remove. Duração: %1 (média %2×)")
                              .arg(root.isMedia ? Project.formatTime(root.sel.duration) : "")
                              .arg(root.isMedia ? root.sel.speed.toFixed(2) : "1")
                    }
                    CheckBox {
                        visible: root.isMedia && root.sel.mediaKind === "video" && root.sel.speed > 0
                        text: qsTr("Reproduzir ao contrário")
                        checked: root.isMedia && root.sel.reverse
                        focusPolicy: Qt.NoFocus
                        onToggled: root.setProp("reverse", checked)
                    }
                    Button {
                        visible: root.isMedia && root.sel.mediaKind === "video"
                        text: qsTr("❄ Congelar quadro no cursor (Alt+F)")
                        focusPolicy: Qt.NoFocus
                        onClicked: Project.freezeFrame()
                    }
                }

                // ---------------- Avançado ----------------
                ColumnLayout {
                    visible: root.page === "advanced" && root.isMedia
                    Layout.fillWidth: true
                    SectionLabel { text: qsTr("Modo de mesclagem") }
                    ComboBox {
                        Layout.fillWidth: true
                        focusPolicy: Qt.NoFocus
                        readonly property var modes: ["normal", "multiply", "screen", "overlay", "darken", "lighten", "colorDodge", "colorBurn", "softLight", "difference"]
                        model: [qsTr("Normal"), qsTr("Multiplicar"), qsTr("Tela"), qsTr("Sobrepor"), qsTr("Escurecer"), qsTr("Clarear"), qsTr("Subexposição de cor"), qsTr("Superexposição de cor"), qsTr("Luz suave"), qsTr("Diferença")]
                        currentIndex: root.isMedia ? Math.max(0, modes.indexOf(root.sel.blend)) : 0
                        onActivated: (i) => root.setProp("blend", modes[i])
                    }
                    SectionLabel { text: qsTr("Chroma key (fundo verde)") }
                    CheckBox {
                        text: qsTr("Remover cor")
                        checked: root.isMedia && root.sel.chromaEnabled
                        focusPolicy: Qt.NoFocus
                        onToggled: root.setProp("chromaEnabled", checked)
                    }
                    ColorField {
                        visible: root.isMedia && root.sel.chromaEnabled
                        label: qsTr("Cor-chave")
                        color: root.isMedia ? root.sel.chromaColor : "#00ff00"
                        onPicked: (c) => root.setProp("chromaColor", c === "" ? "#00ff00" : c)
                    }
                    PropSlider {
                        visible: root.isMedia && root.sel.chromaEnabled
                        label: qsTr("Intensidade")
                        from: 0; to: 1; displayScale: 100; decimals: 0
                        value: root.isMedia ? root.sel.chromaSimilarity : 0.3
                        onMoved: (v) => root.setProp("chromaSimilarity", v)
                    }
                    PropSlider {
                        visible: root.isMedia && root.sel.chromaEnabled
                        label: qsTr("Suavidade")
                        from: 0; to: 1; displayScale: 100; decimals: 0
                        value: root.isMedia ? root.sel.chromaSmoothness : 0.1
                        onMoved: (v) => root.setProp("chromaSmoothness", v)
                    }

                    SectionLabel {
                        visible: root.isMedia && root.sel.mediaKind === "video"
                        text: qsTr("Remover fundo (IA)")
                    }
                    CheckBox {
                        visible: root.isMedia && root.sel.mediaKind === "video"
                        text: qsTr("Recortar pessoas automaticamente")
                        checked: root.isMedia && root.sel.removeBackground
                        enabled: root.isMedia && root.sel.matteState !== "unavailable"
                        focusPolicy: Qt.NoFocus
                        onToggled: root.setProp("removeBackground", checked)
                    }
                    ProgressBar {
                        visible: root.isMedia && (root.sel.matteState === "processing"
                                                  || (root.sel.matteState === "needsModel" && Project.downloadingModel === "rvm"))
                        Layout.fillWidth: true
                        value: root.sel.matteState === "processing" ? root.sel.matteProgress : Project.modelDownloadProgress
                    }
                    PillButton {
                        visible: root.isMedia && root.sel.matteState === "needsModel" && Project.downloadingModel !== "rvm"
                        enabled: Project.downloadingModel === ""
                        text: qsTr("Baixar modelo (15 MB)")
                        iconName: "download"
                        onClicked: Project.downloadModel("rvm")
                    }
                    Text {
                        visible: root.isMedia && root.sel.mediaKind === "video"
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: root.isMedia && root.sel.matteState === "ready" ? Theme.accent : Theme.textFaint
                        font.pixelSize: Theme.fontSmall
                        text: !root.isMedia ? ""
                              : root.sel.matteState === "unavailable" ? qsTr("Indisponível nesta versão (compilada sem ONNX Runtime).")
                              : root.sel.matteState === "needsModel" ? (Project.downloadingModel === "rvm" ? qsTr("Baixando o modelo de recorte…")
                                                                         : qsTr("Requer o modelo de recorte de pessoas (RobustVideoMatting, GPL-3.0), baixado uma única vez."))
                              : root.sel.matteState === "processing" ? qsTr("Analisando o vídeo… %1%").arg(Math.round(root.sel.matteProgress * 100))
                              : root.sel.matteState === "queued" ? qsTr("Na fila…")
                              : root.sel.matteState === "ready" ? qsTr("✓ Fundo removido (processado localmente)")
                              : qsTr("Separa pessoas do fundo sem tela verde. Tudo roda no seu computador, em segundo plano.")
                    }
                }

                // ---------------- Animação (texto) ----------------
                ColumnLayout {
                    visible: root.page === "animation"
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: Theme.textDim
                        font.pixelSize: Theme.fontSmall
                        text: qsTr("Escolha animações de entrada e saída na aba \"Animações\" do painel à esquerda.")
                    }
                }
            }
        }
    }

    FileDialog {
        id: lutDialog
        title: qsTr("Carregar LUT")
        nameFilters: [qsTr("LUT 3D (*.cube)")]
        onAccepted: Project.loadLut(root.sel.id, selectedFile)
    }
}
