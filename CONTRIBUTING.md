# Contributing

## Commit messages

clivcut follows [Conventional Commits](https://www.conventionalcommits.org/). Messages are written in English.

```
<type>(<scope>): <summary>

<body: what changed and why, wrapped at 72 columns>
```

- **Summary:** imperative mood, lowercase, no trailing period, at most 72 characters ("add", not "added" or "adds").
- **Types:** `feat`, `fix`, `perf`, `refactor`, `test`, `docs`, `build`, `ci`, `chore`.
- **Scopes** (optional): a module (`core`, `media`, `render`, `engine`, `app`, `cli`) or an area (`audio`, `timeline`, `captions`, `export`, `packaging`, `roadmap`).
- **Body** (optional): explain the reason for the change and anything non-obvious. A short bullet list is fine for larger changes.
- **Breaking changes:** add `!` after the type/scope and a `BREAKING CHANGE:` footer, for example when the `.clivcut` format changes incompatibly.

Examples:

```
feat(audio): add loudness normalization and noise reduction
fix(media): pass FFmpeg numeric options as integers
docs(roadmap): add mobile versions (Android and iOS)
build(packaging): bundle ONNX Runtime in the AppImage
```

Keep each commit focused on one logical change, and make sure it builds and `ctest` passes.
