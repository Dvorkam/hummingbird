# DocumentPipeline Split (Milestone 4)

Goal: split DocumentPipeline responsibilities while preserving behavior and test coverage.

## Substeps

- [x] Identify ownership groups and target classes (input, paint, resources, model).
- [x] Extract input focus/edit/paint into `DocumentInputController`.
- [x] Wire DocumentPipeline to delegate to new controller.
- [x] Run build + tests and remove any redundant code.
