# clivcut architecture

## Principles

1. **A single source of truth per frame.** `clv::evaluate(project, t)` (in `src/core/Evaluate.cpp`) resolves which visual layers and audio sources are active at time `t`, with keyframes, animations and transitions already applied. Preview and export both use this function and the same `render::Compositor`.
2. **Integer time in microseconds.** This is the same unit used by imported draft JSON files. Splitting and trimming accumulate no error, and conversion to frames happens only at the edges (`Project::snapToFrame`).
3. **A simple, copyable model.** `clv::Project` is made of plain structs. Undo stores snapshots, and other threads (render, audio, export) receive an immutable `shared_ptr<const Project>`.
4. **Portable GPU.** Qt RHI runs Vulkan, OpenGL, Metal or D3D with the same GLSL shaders (compiled by `qsb`). In the preview the compositor uses Qt Quick's GPU (`QQuickRhiItem`). For export it creates its own offscreen RHI.

## Layers

```
            ┌───────────────────── src/app (Qt Quick) ─────────────────────┐
            │ Main.qml · MediaPanel · PreviewPanel · Inspector · Timeline  │
            │ ProjectController (QML singleton "Project")                  │
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

| Module | Responsibility |
|---|---|
| `core` | model, editing operations, evaluation, `.clivcut` format, draft import |
| `media` | probe, seeking decode, audio resampling, encoder |
| `render` | GPU compositor, blend modes, chroma key, transitions, text |
| `engine` | mixer, per-clip frame source, export |
| `app` | QML UI, preview, playback audio |

## Playback flow

1. `ProjectController::play()` starts `AudioOutput`. A producer thread mixes the project about 200 ms ahead into a ring buffer, which the SDL callback consumes.
2. The audio position is the **master clock**. A 15 ms timer updates `playhead`, and `PreviewItem` requests a new frame.
3. On the Qt Quick render thread, `PreviewRenderer` evaluates the project, requests frames from `DecoderFrameSource` (one `VideoDecoder` per clip, at preview resolution) and composites directly into the item's texture.

## Export flow

`Exporter` runs on its own thread with an `OffscreenRhi`. For each frame it does:
1. `evaluate`;
2. `composite`;
3. GPU readback;
4. `Encoder::writeVideo` (swscale RGBA → YUV, libavcodec);
5. mixes the frame's exact audio interval → `Encoder::writeAudio` (AAC).

## Project format (`.clivcut`)

Human-readable, versioned JSON (`format: "clivcut"`, `version: 1`). It holds the project (canvas, fps, background, magnet), the assets (absolute paths and metadata), the tracks with their clips (time in µs, transform, per-property keyframes, color, blend mode, chroma key, in transition, animations) and the markers.

## Draft JSON import

`core/DraftImport.cpp`:
- **Discovery and validation:** reads `draft_content.json` or `draft_info.json`. If the file does not start with `{`, the draft is encrypted; this is reported without any attempt to decrypt it.
- **Media:** builds a global `id → material` index, resolves the draft-folder path placeholder and converts videos, photos and audio into assets.
- **Clips:** converts segments into clips, flipping the Y axis (+y is up in the draft, down in clivcut).
- **Keyframes:** maps `common_keyframes` for position, scale, rotation, opacity and volume.
- **Secondary materials:** `audio_fades` become fades, and `transitions` become dissolves on the edge of the incoming clip.
- **Ignored, with a warning:** proprietary effects, stickers and filters.

## Tests

- `tst_core`: model, editing, keyframes, transitions, history, serialization and draft importer.
- `tst_media`: probe, seek vs. sequential decode consistency, audio, thumbnails and waveforms.
- `tst_render`: orientation, scale, opacity, multiply and text, with GPU readback (Vulkan and OpenGL).
- `tst_controller`: UI flow (import → edit → undo → save → reopen → export).
