# Arquitetura do clivcut

## Princípios

1. **Uma única fonte de verdade por quadro.** `clv::evaluate(project, t)` (em `src/core/Evaluate.cpp`) resolve quais camadas visuais e fontes de áudio estão ativas no instante `t`, já com keyframes, animações e transições aplicados. O preview e o export usam essa mesma função e o mesmo `render::Compositor`.
2. **Tempo inteiro em microssegundos.** É a mesma unidade usada pelos drafts JSON importados. Dividir e aparar não acumulam erro, e a conversão para quadros é feita só nas bordas (`Project::snapToFrame`).
3. **Modelo simples e copiável.** `clv::Project` é feito de structs simples. O undo guarda snapshots, e outras threads (render, áudio, export) recebem um `shared_ptr<const Project>` imutável.
4. **GPU portátil.** Qt RHI roda Vulkan, OpenGL, Metal ou D3D com os mesmos shaders GLSL (compilados pelo `qsb`). No preview o compositor usa a GPU do Qt Quick (`QQuickRhiItem`). No export ele cria um RHI offscreen próprio.

## Camadas

```
            ┌───────────────────── src/app (Qt Quick) ─────────────────────┐
            │ Main.qml · MediaPanel · PreviewPanel · Inspector · Timeline  │
            │ ProjectController (singleton QML "Project")                  │
            │ PreviewItem (QQuickRhiItem) · AudioOutput (SDL2) · providers │
            └──────────────┬───────────────────────────┬───────────────────┘
                           │                           │
              src/engine: DecoderFrameSource, AudioMixer, Exporter
                           │                           │
      src/render: Compositor (RHI) + TextRaster    src/media: FFmpeg
                           │                           │
                           └──────── src/core ─────────┘
           Model · Edit · Keyframes · History · Evaluate · Serialize · DraftImport
```

| Módulo | Responsabilidade |
|---|---|
| `core` | modelo, operações de edição, avaliação, formato `.clivcut`, importação de drafts |
| `media` | probe, decodificação com seek, reamostragem de áudio, encoder |
| `render` | compositor GPU, modos de mesclagem, chroma, transições, texto |
| `engine` | mixer, fonte de quadros por clipe, export |
| `app` | interface QML, preview, áudio de reprodução |

## Fluxo de reprodução

1. `ProjectController::play()` liga o `AudioOutput`. Uma thread produtora mixa o projeto cerca de 200 ms à frente num anel, e o callback do SDL consome.
2. A posição do áudio é o **relógio mestre**. Um timer de 15 ms atualiza `playhead`, e o `PreviewItem` pede um novo quadro.
3. Na thread de render do Qt Quick, `PreviewRenderer` avalia o projeto, pede os quadros ao `DecoderFrameSource` (um `VideoDecoder` por clipe, na resolução do preview) e compõe direto na textura do item.

## Fluxo de export

O `Exporter` roda numa thread própria com um `OffscreenRhi`. Para cada quadro ele faz:
1. `evaluate`;
2. `composite`;
3. readback da GPU;
4. `Encoder::writeVideo` (swscale RGBA → YUV, libavcodec);
5. a mixagem do intervalo de áudio exato do quadro → `Encoder::writeAudio` (AAC).

## Formato de projeto (`.clivcut`)

É JSON legível e versionado (`format: "clivcut"`, `version: 1`). Contém o projeto (canvas, fps, fundo, ímã), os assets (caminhos absolutos e metadados), as faixas com os clipes (tempo em µs, transformação, keyframes por propriedade, cor, mesclagem, chroma, transição de entrada, animações) e os marcadores.

## Importação de drafts JSON

`core/DraftImport.cpp`:
- **Localização e validação:** lê `draft_content.json` ou `draft_info.json`. Se o arquivo não começa com `{`, o draft é criptografado, e isso é reportado sem tentar decifrar.
- **Mídias:** monta um índice global `id → material`, resolve o placeholder de caminho da pasta do draft e converte vídeos, fotos e áudios em assets.
- **Clipes:** converte segmentos em clipes, invertendo o eixo Y (+y para cima no draft, para baixo no clivcut).
- **Keyframes:** mapeia os `common_keyframes` de posição, escala, rotação, opacidade e volume.
- **Materiais secundários:** os `audio_fades` viram fades, e as `transitions` viram dissolver, na borda do clipe que entra.
- **Ignorado, com aviso:** efeitos, stickers e filtros proprietários.

## Testes

- `tst_core`: modelo, edição, keyframes, transições, histórico, serialização e importador de drafts.
- `tst_media`: probe, consistência entre seek e decodificação sequencial, áudio, miniaturas e forma de onda.
- `tst_render`: orientação, escala, opacidade, multiplicar e texto, com readback da GPU (Vulkan e OpenGL).
- `tst_controller`: fluxo da interface (importar → editar → desfazer → salvar → reabrir → exportar).
