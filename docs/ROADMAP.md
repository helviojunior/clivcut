# Roadmap

✅ feito · 🟡 parcial · ⬜ pendente

## MVP (fluxo essencial de edição)

| # | Recurso | Status | Observações |
|---|---|---|---|
| 1 | Projetos: criar, abrir, salvar, recuperação | ✅ | cópia de recuperação a cada 30 s, recuperar após falha, confirmar ao fechar com alterações |
| 2 | Importar mídia, miniaturas, forma de onda, reconectar | ✅ | importar pasta inteira pendente |
| 3 | Player na GPU com qualidade de preview | ✅ | tela cheia pendente |
| 4 | Timeline multifaixa (principal, sobreposição, texto, áudio), mutar/ocultar/bloquear | ✅ | |
| 5 | Dividir, excluir à esquerda/direita, aparar, ripple | ✅ | |
| 6 | Ímã da faixa principal e snapping | ✅ | |
| 7 | Seleção múltipla, copiar/colar, atributos, grupos | ✅ | seleção por retângulo e arraste em grupo |
| 8 | Desfazer/refazer | ✅ | |
| 9 | Zoom, rolagem, ajustar, marcadores | 🟡 | faltam pontos de entrada e saída (I/O) |
| 10 | Transformação, crop, espelhar, opacidade | ✅ | falta caixa de manipulação no player |
| 11 | Proporções do canvas e cor de fundo | 🟡 | falta fundo desfocado |
| 12 | Velocidade, curvas, reverso, congelar | 🟡 | curvas prontas; falta preservar o tom (Rubber Band/SoundTouch); no reverso o áudio fica mudo |
| 13 | Keyframes com easing | ✅ | editor gráfico com Bézier personalizada; falta arrastar keyframes direto na timeline |
| 14 | Texto e animações | ✅ | faltam animações por caractere/palavra (máquina de escrever, karaokê) e destaque da palavra falada nas legendas automáticas |
| 15 | Legendas: importar SRT/VTT, editar, exportar, legendas automáticas (whisper.cpp) | ✅ | falta editor de lista de legendas e tradução local |
| 16 | Áudio: volume, fades, separar áudio, normalizar, reduzir ruído | ✅ | falta isolamento de voz por IA |
| 17 | Ajustes de cor e LUT .cube | ✅ | exposição, brilho, contraste, saturação, realces, sombras, temperatura, matiz, vinheta e nitidez; LUT 3D/1D na GPU |
| 18 | Filtros (LUTs prontos) | ✅ | 16 filtros próprios, prévia no quadro atual e camadas de ajuste na timeline; faltam curvas, HSL e rodas de cor (Fase 2) |
| 19 | Transições | ✅ | 8 tipos |
| 20 | Animações de entrada e saída | ✅ | faltam animações em loop (combo) |
| 21 | Máscaras e chroma key | ✅ | faltam keyframes de máscara, máscara de caminho livre e alça de edição no player |
| 22 | Modos de mesclagem | ✅ | 10 modos |
| 23 | Exportação | 🟡 | faltam encode por hardware (VA-API/NVENC), só áudio, quadro único e vídeo com alfa (ProRes 4444 / WebM VP9) |
| 24 | Proxies e cache de render | 🟡 | proxies automáticos e decodificação por hardware feitos; falta cache de render de trechos pesados |
| 25 | Atalhos personalizáveis | 🟡 | atalhos fixos |

## Fase 2

- Decodificação por hardware sem cópia (VA-API → dmabuf → textura Vulkan/GL) e pré-busca de quadros numa thread.
- Compound clips, curvas de velocidade, slow motion suave (RIFE).
- Curvas, HSL, rodas de cor, escopos (histograma, waveform, vectorscope). ✅ Camadas de ajuste.
- 🟡 Remoção de fundo local (RobustVideoMatting em ONNX Runtime): matte gerado e em cache; falta aplicar no compositor e controles na interface. Recorte interativo (SAM 2) pendente.
- Rastreamento de objetos, estabilização (vid.stab). ✅ Redução de ruído (afftdn) e normalização de loudness (EBU R128); falta isolamento de voz por IA (DeepFilterNet/Demucs).
- Edição pela transcrição, remoção de pausas, TTS local (Piper).
- Stickers e formas, texto curvo, estilos por caractere.
- Biblioteca de efeitos de vídeo em shader (glitch, shake, blur, VHS, brilho, partículas) e mais transições.
- Áudio: ducking automático da música sob a voz, efeitos de voz (eco, reverb, trocador de voz), gravação de narração na timeline.
- Detecção de batida na música com marcadores automáticos.
- Modelos de projeto (templates) com slots de mídia e modelos de texto prontos.
- Empacotamento:
  - ✅ AppImage, testado.
  - 🟡 Flatpak: manifesto pronto; falta validar no flatpak-builder e publicar no Flathub.
  - 🟡 Windows (MSVC/vcpkg/NSIS) e macOS (Homebrew/DMG): CMake e CI prontos, ainda não executados (sem máquina Windows/Mac nem CI ativo).

## Fase 3

- Reenquadramento automático, retoque facial, super-resolução (Real-ESRGAN).
- Plugins de IA generativa com provedores configuráveis (local ou remoto), sempre opcionais.
- Gravação de tela e webcam via PipeWire / portal.
- Acervo de músicas e efeitos sonoros com licença livre.
- Versões móveis (Android primeiro, depois iOS):
  - Compilar FFmpeg, ONNX Runtime (AAR / xcframework) e whisper.cpp para arm64; trocar SDL2 por `QAudioSink`.
  - Decodificação por hardware com MediaCodec no Android (VideoToolbox já cobre o iOS).
  - Interface de toque própria em QML reaproveitando o `ProjectController`: layout em retrato, pinça na timeline, toque longo para arrastar clipes.
  - Acesso a arquivos: URIs `content://` no Android, sandbox e galeria no iOS; limites de memória na exportação.
  - iOS: build do FFmpeg só LGPL e dinâmico (compatível com a App Store), assinatura com conta Apple Developer.
