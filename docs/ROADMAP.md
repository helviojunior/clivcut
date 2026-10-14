# Roadmap

✅ done · 🟡 partial · ⬜ pending

## MVP (core editing flow)

| # | Feature | Status | Notes |
|---|---|---|---|
| 1 | Projects: create, open, save, recovery | ✅ | recovery backup every 30 s, recover after a crash, confirm on close with unsaved changes |
| 2 | Media import, thumbnails, waveforms, relinking | ✅ | whole-folder import pending |
| 3 | GPU player with preview quality | ✅ | fullscreen pending |
| 4 | Multi-track timeline (main, overlay, text, audio), mute/hide/lock | ✅ | |
| 5 | Split, delete left/right, trim, ripple | ✅ | |
| 6 | Main-track magnet and snapping | ✅ | |
| 7 | Multi-selection, copy/paste, attributes, groups | ✅ | rubber-band selection and group dragging |
| 8 | Undo/redo | ✅ | |
| 9 | Zoom, scroll, fit, markers | 🟡 | in/out points (I/O) missing |
| 10 | Transform, crop, flip, opacity | ✅ | on-player transform handles missing |
| 11 | Canvas aspect ratios and background color | 🟡 | blurred background missing |
| 12 | Speed, curves, reverse, freeze | 🟡 | curves done; pitch preservation missing (Rubber Band/SoundTouch); audio is muted when reversed |
| 13 | Keyframes with easing | ✅ | graph editor with custom Bézier; dragging keyframes directly on the timeline missing |
| 14 | Text and animations | ✅ | per-character/per-word animations (typewriter, karaoke) and spoken-word highlighting in automatic captions missing |
| 15 | Captions: SRT/VTT import, editing, export, automatic captions (whisper.cpp) | ✅ | caption list editor and local translation missing |
| 16 | Audio: volume, fades, detach audio, normalize, noise reduction | ✅ | AI voice isolation missing |
| 17 | Color adjustments and .cube LUTs | ✅ | exposure, brightness, contrast, saturation, highlights, shadows, temperature, tint, vignette and sharpness; 3D/1D LUTs on the GPU |
| 18 | Filters (built-in LUTs) | ✅ | 16 built-in filters, current-frame preview and adjustment layers on the timeline; curves, HSL and color wheels missing (Phase 2) |
| 19 | Transitions | ✅ | 8 types |
| 20 | In and out animations | ✅ | looping (combo) animations missing |
| 21 | Masks and chroma key | ✅ | mask keyframes, freeform path masks and on-player edit handles missing |
| 22 | Blend modes | ✅ | 10 modes |
| 23 | Export | 🟡 | hardware encoding (VA-API/NVENC), audio-only, single frame and alpha video (ProRes 4444 / WebM VP9) missing |
| 24 | Proxies and render cache | 🟡 | automatic proxies and hardware decoding done; render cache for heavy sections missing |
| 25 | Customizable shortcuts | 🟡 | fixed shortcuts |

## Phase 2

- Zero-copy hardware decoding (VA-API → dmabuf → Vulkan/GL texture) and frame prefetching on a thread.
- Compound clips, speed curves, smooth slow motion (RIFE).
- Curves, HSL, color wheels, scopes (histogram, waveform, vectorscope). ✅ Adjustment layers.
- 🟡 Local background removal (RobustVideoMatting on ONNX Runtime): matte generated and cached; applying it in the compositor and UI controls are missing. Interactive cutout (SAM 2) pending.
- Object tracking, stabilization (vid.stab). ✅ Noise reduction (afftdn) and loudness normalization (EBU R128); AI voice isolation (DeepFilterNet/Demucs) missing.
- Transcript-based editing, silence removal, local TTS (Piper).
- Stickers and shapes, curved text, per-character styles.
- Shader-based video effects library (glitch, shake, blur, VHS, glow, particles) and more transitions.
- Audio: automatic music ducking under voice, voice effects (echo, reverb, voice changer), voiceover recording on the timeline.
- Music beat detection with automatic markers.
- Project templates with media slots and ready-made text templates.
- Packaging:
  - ✅ AppImage, tested.
  - 🟡 Flatpak: manifest ready; needs validation with flatpak-builder and publishing on Flathub.
  - 🟡 Windows (MSVC/vcpkg/NSIS) and macOS (Homebrew/DMG): CMake and CI ready, not yet run (no Windows/Mac machine and no active CI).

## Phase 3

- Automatic reframing, face retouching, super-resolution (Real-ESRGAN).
- Generative AI plugins with configurable providers (local or remote), always optional.
- Screen and webcam recording via PipeWire / portal.
- Library of freely licensed music and sound effects.
- Mobile versions (Android first, then iOS):
  - Build FFmpeg, ONNX Runtime (AAR / xcframework) and whisper.cpp for arm64; replace SDL2 with `QAudioSink`.
  - Hardware decoding with MediaCodec on Android (VideoToolbox already covers iOS).
  - Dedicated QML touch UI reusing `ProjectController`: portrait layout, pinch on the timeline, long-press to drag clips.
  - File access: `content://` URIs on Android, sandbox and photo library on iOS; memory limits during export.
  - iOS: LGPL-only, dynamically linked FFmpeg build (App Store compatible), signing with an Apple Developer account.
