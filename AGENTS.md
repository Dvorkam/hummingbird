# Agent Notes

- For include audits, use include-what-you-use (`iwyu_tool.py -p build`) and apply suggestions with `fix_includes.py`.
- Ensure `CMAKE_EXPORT_COMPILE_COMMANDS=ON` so IWYU can read `build/compile_commands.json`.
