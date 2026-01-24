# DocumentPipeline Split (Milestone 4)

Goal: split DocumentPipeline responsibilities while preserving behavior and test coverage.

## Substeps

- [x] Identify ownership groups and target classes (input, paint, resources, model).
- [x] Extract input focus/edit/paint into `DocumentInputController`.
- [x] Wire DocumentPipeline to delegate to new controller.
- [x] Extract DOM + CSS + render-tree logic into `DocumentModel`.
- [x] Extract stylesheet/image resource handling into `DocumentResources`.
- [x] Extract paint orchestration into `DocumentPainter`.
- [x] Wire DocumentPipeline to orchestrate the new components.
- [x] Run build + tests and remove any redundant code.
