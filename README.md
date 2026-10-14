# clivcut

Editor de vídeo **open-source e multiplataforma** (Linux primeiro), com o fluxo de edição: faixa principal magnética, cortes rápidos, texto, transições, animações, keyframes e exportação em um clique.

Escrito em **C++20 + Qt 6 (Qt Quick / RHI) + FFmpeg**. A composição de vídeo roda na GPU (Vulkan, OpenGL, Metal ou Direct3D, via Qt RHI). Preview e exportação usam **o mesmo compositor**, então o arquivo exportado é idêntico ao que aparece no player.


## Recursos

- **Mídia:** importa vídeo, áudio e imagem (tudo que o FFmpeg lê), por arrastar e soltar ou pelo diálogo. Tem miniaturas, forma de onda, reconexão de mídia ausente e respeita a rotação de vídeos de celular.
- **Desempenho:**
  - **Decodificação por hardware:** VA-API no Linux, D3D11VA no Windows e VideoToolbox no macOS, com redução de escala na GPU.
  - **Proxies automáticos** para vídeos acima de 1080p, gerados em segundo plano. O botão "Proxy" do player alterna entre proxy e original, e a exportação sempre usa o original.
- **Timeline:**
  - Faixa principal magnética, faixas de sobreposição, texto e áudio, com mutar, ocultar e bloquear.
  - Dividir (Ctrl+B), excluir à esquerda e à direita (Q/W), aparar, mover entre faixas e excluir com ripple.
  - Snapping, zoom, marcadores, duplicar e congelar quadro.
  - **Copiar, recortar e colar** clipes, também entre projetos via área de transferência do sistema, levando as mídias junto.
  - **Copiar e colar atributos**, **agrupar** clipes, **seleção por retângulo** e mover vários clipes juntos.
  - Menu de clique direito nos clipes.
- **Transformação:** posição, escala, rotação, opacidade, espelhar e crop. Mover arrastando no player; Ctrl+roda redimensiona.
- **Keyframes:**
  - Em posição, escala, rotação, opacidade, volume, brilho, contraste e saturação, com o botão ◆ ao lado de cada propriedade.
  - **Editor gráfico** na aba Keyframes: arrastar tempo e valor e escolher a curva de cada trecho (linear, entrada, saída, suave, manter ou **Bézier personalizada** com alças).
- **Texto:** fonte, tamanho, cor, contorno, sombra, caixa de fundo e alinhamento. Presets de título, legenda e destaque.
- **Transições:** dissolver, escurecer, cortinas, deslizar, círculo e zoom.
- **Animações** de entrada e saída: esmaecer, zoom e deslizar.
- **Cor:**
  - 16 filtros próprios (Vívido, Teal & Orange, Vintage, Noir…), com prévia no quadro atual e intensidade.
  - LUTs `.cube` com intensidade.
  - Exposição, brilho, contraste, saturação, realces, sombras, temperatura, matiz, vinheta e nitidez.
  - **Camadas de ajuste/filtro** na timeline, que afetam tudo abaixo delas.
- **Composição:**
  - 10 modos de mesclagem e chroma key.
  - **Máscaras** linear, espelho, círculo e retângulo arredondado, com posição, tamanho, rotação, suavização e inversão. Também valem em camadas de ajuste, para corrigir a cor só numa região.
- **Velocidade:**
  - Constante de 0,1× a 10× e reverso.
  - **Curvas de velocidade** (Montagem, Herói, Bala, Salto…), com editor gráfico de pontos. As curvas dos drafts também são importadas.
- **Áudio:** volume (até 200%), fade in e fade out, mixagem de várias faixas, **separar o áudio do vídeo** (Ctrl+Shift+D) e silenciar clipe.
- **Canvas:** 16:9, 9:16, 1:1, 4:3, 3:4, 4:5 e 21:9, com cor de fundo.
- **Exportação:**
  - Codecs H.264, HEVC, AV1 e ProRes 422 HQ, conforme o FFmpeg instalado.
  - Resolução de 480p a 4K, de 24 a 60 fps, com áudio AAC.
- **Legendas automáticas 100% locais** com whisper.cpp, sem nuvem:
  - Mais de 90 idiomas, com modelos baixados sob demanda (Tiny a Large v3 Turbo).
  - Cada legenda vira um clipe de texto editável numa faixa "Legendas".
  - "Aplicar a todas" copia o estilo de uma legenda para a faixa inteira.
  - Importação de SRT/VTT e exportação de SRT.
- **Importação de drafts** (`draft_content.json` / `draft_info.json`, sem criptografia):
  - Traz mídias, cortes, velocidade, volume, transformações e keyframes de posição, escala, rotação, opacidade e volume.
  - Traz também textos, fades e transições.
- **Undo/redo** ilimitado, com edições contínuas agrupadas.
- **Recuperação:**
  - Cópia automática a cada 30 s com alterações pendentes.
  - Oferece recuperar o trabalho depois de um fechamento inesperado.
  - Pergunta antes de descartar alterações.
- **`clivcut-render`:** renderização, conversão e transcrição (`--transcribe`) pela linha de comando, sem interface.

Veja o [roadmap](docs/ROADMAP.md) para o que vem a seguir: máscaras, LUTs, proxies, curvas de velocidade e remoção de fundo por IA local.

## Compilar

Dependências:
- CMake ≥ 3.24 e um compilador C++20 (GCC 12+ ou Clang 15+);
- Qt ≥ 6.7: Core, Gui (com headers privados para o RHI), Quick, QuickControls2, ShaderTools, Concurrent e Test;
- FFmpeg ≥ 6 (libavformat, libavcodec, libswscale, libswresample, libavfilter);
- SDL2;
- whisper.cpp: opcional. É baixado e compilado automaticamente pelo CMake (`-DCLIVCUT_WITH_WHISPER=OFF` desliga; `-DCLIVCUT_WHISPER_VULKAN=ON` acelera na GPU).

```sh
# Debian/Ubuntu (24.04+)
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-base-private-dev qt6-declarative-dev \
    qt6-shadertools-dev libavformat-dev libavcodec-dev libswscale-dev libswresample-dev \
    libavfilter-dev libsdl2-dev ffmpeg
# Fedora
sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel qt6-qtbase-private-devel \
    qt6-qtdeclarative-devel qt6-qtshadertools-devel ffmpeg-devel SDL2-devel
# Gentoo
emerge dev-qt/qtbase dev-qt/qtdeclarative dev-qt/qtshadertools media-video/ffmpeg media-libs/libsdl2

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build          # testes (núcleo, mídia, GPU, controlador)
```

## Pacotes

**AppImage** (um arquivo, roda na maioria das distros):

```sh
packaging/appimage/build-appimage.sh      # gera dist/clivcut-<versão>-x86_64.AppImage
chmod +x dist/clivcut-*.AppImage && dist/clivcut-*.AppImage
```

**Flatpak** (manifesto em `packaging/flatpak/`; compila FFmpeg com x264, VA-API e whisper.cpp no sandbox):

```sh
flatpak install flathub org.kde.Sdk//6.9 org.kde.Platform//6.9
flatpak-builder --user --install --force-clean build-flatpak packaging/flatpak/io.github.clivcut.yml
flatpak run io.github.clivcut
```

`cmake --install build` instala os binários, o `.desktop`, o ícone e o metainfo AppStream.

**Windows** (MSVC 2022 + [vcpkg](https://vcpkg.io)):

```powershell
vcpkg install --triplet x64-windows "ffmpeg[core,avcodec,avformat,avfilter,swscale,swresample,gpl,x264,dav1d,opus]" sdl2
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64 `
      -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build build
cd build && cpack -G NSIS      # instalador clivcut-<versão>-win64.exe (Qt e DLLs incluídos)
```

**macOS** (Homebrew):

```sh
brew install ninja pkg-config ffmpeg sdl2 qt
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
cd build && cpack -G DragNDrop # clivcut-<versão>-Darwin.dmg
```

No Windows a GPU usa Direct3D 11 e no macOS, Metal, com os mesmos shaders. O CI (`.github/workflows/build.yml`) gera AppImage, instalador do Windows e DMG do macOS a cada push.

## Usar

```sh
build/src/app/clivcut                         # interface gráfica
build/src/app/clivcut video1.mp4 musica.mp3   # abre já importando mídias
build/src/app/clivcut ~/caminho/do/draft      # abre um draft da solução

# Linha de comando
build/src/cli/clivcut-render projeto.clivcut saida.mp4 --codec hevc --height 2160
build/src/cli/clivcut-render ~/pasta_do_draft saida.mp4         # renderiza um draft JSON
build/src/cli/clivcut-render --convert ~/pasta_do_draft projeto.clivcut
build/src/cli/clivcut-render --probe video.mp4
build/src/cli/clivcut-render --transcribe --model small --language pt video.mp4 legendas.srt
build/src/cli/clivcut-render --bench video_4k.mp4            # mede a decodificação (--no-hw compara com software)
```

Os modelos de transcrição ficam em `~/.local/share/clivcut/models`. Para mudar o local, use `CLIVCUT_MODELS_DIR`. Baixe pela aba **Legendas** do app ou manualmente:
`curl -L -o ~/.local/share/clivcut/models/ggml-small.bin https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin`

Variáveis úteis:
- `CLIVCUT_RHI=vulkan|opengl` escolhe o backend do export.
- `QSG_RHI_BACKEND=vulkan|opengl` escolhe o backend da interface.
- `CLIVCUT_HWDEC=0` desliga a decodificação por hardware.
- `CLIVCUT_CACHE_DIR` muda a pasta de cache (proxies), que por padrão é `~/.cache/clivcut`.
- `CLIVCUT_FFMPEG_LOG=1` mostra todos os logs do FFmpeg.

### Atalhos

| Ação | Tecla |
|---|---|
| Reproduzir/pausar | Espaço |
| Dividir | Ctrl+B |
| Excluir à esquerda/direita do cursor | Q / W |
| Excluir (com ripple) | Del (Shift+Del) |
| Desfazer/refazer | Ctrl+Z / Ctrl+Shift+Z |
| Duplicar | Ctrl+D |
| Copiar / recortar / colar | Ctrl+C / Ctrl+X / Ctrl+V |
| Copiar / colar atributos | Ctrl+Alt+C / Ctrl+Alt+V |
| Agrupar / desagrupar | Ctrl+G / Ctrl+Shift+G |
| Selecionar tudo | Ctrl+A |
| Seleção por retângulo | arrastar no vazio da timeline (Ctrl/Shift soma) |
| Quadro anterior/próximo (1 s com Shift) | ← / → |
| Início/fim | Home / End |
| Marcador | M |
| Congelar quadro | Alt+F |
| Novo texto | Ctrl+T |
| Snapping liga/desliga | N |
| Zoom da timeline / ajustar | Ctrl+= / Ctrl+− / Shift+Z |
| Importar / Exportar / Salvar | Ctrl+I / Ctrl+E / Ctrl+S |

## Arquitetura

Visão geral em [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

```
src/core    modelo, edição, keyframes, histórico, avaliação por quadro, importador de drafts JSON (só QtCore)
src/media   FFmpeg: probe, decodificação com seek preciso, áudio 48 kHz, encoder
src/render  compositor Qt RHI + shaders GLSL, rasterização de texto
src/engine  mixer de áudio, fonte de quadros, exportador
src/app     interface Qt Quick (QML), preview na GPU, saída de áudio SDL2
src/cli     clivcut-render
tests       QtTest
```

## Licença

GPL-3.0-or-later. Veja [LICENSE](LICENSE).
