# Milestone 4 Done

- [x] **[M4 P2] T-RENDER-1: Font Setup Cache**; Goal: cache Blend2D font setup per (path,size); Scope: SDLGraphicsContext; Acceptance: draw/measure no longer reloads fonts per call; Tests: renderer tests.
- [x] **[M4 P2] T-RENDER-2: Premeasured Text Draw**; Goal: avoid re-measuring text inside draw calls; Scope: TextBox + SDLGraphicsContext; Acceptance: draw path uses precomputed metrics; Tests: renderer tests.
- [x] **[M4 P2] T-RENDER-3: Text Texture Cache (LRU)**; Goal: reuse SDL textures for repeated strings; Scope: SDLGraphicsContext; Acceptance: text-heavy pages reuse cached textures; Tests: renderer perf tests.
- [x] **[M4 P2] T-RENDER-4: Image Texture Cache (LRU)**; Goal: reuse SDL textures for repeated images; Scope: SDLGraphicsContext; Acceptance: repeated image paints reuse textures; Tests: renderer perf tests.
