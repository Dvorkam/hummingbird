# Agent Notes
- Every generated code should adhere to doc/coding_constitution.md
- if you find a conflict between doc/milestones/roadmap.md and current story, point it out and sugest architectually cleanest solution.
- if during the course of programming a new task appears that doesnt really fit current scope of work add it as a future story into doc/TODOs.md
- Before every commit that changed code cmake should be used to build the project using preset in CMakeUserPresets.json
- Before every commit that changed code ctest should be run
- For include audits, use include-what-you-use (if available)(`iwyu_tool.py -p build`) and apply suggestions with `fix_includes.py`.
- Ensure `CMAKE_EXPORT_COMPILE_COMMANDS=ON` so IWYU can read `build/compile_commands.json`.
- Before commit clang-format -i -- src/**/*.cpp' 'src/**/*.h should be run to format the code
- after every self contained code change is made, make a git commit in the best practices format (feat/chore/fix/breaking/doc: msg)
 