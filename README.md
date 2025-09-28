# Aalto CS-C3100 — Computer Graphics (course repository)

This repository contains the starter code, assets, and build presets for the course assignments (assignment1, assignment2, ...). The material here is intended to be built on Windows (primary grading platform) and can also be configured on macOS/Linux using the provided CMake presets and vcpkg.

## Quick Contents
- assignment1/, assignment2/ — assignment-specific sources, assets, and instructions (see each folder's `instructions.pdf`)
- shared_sources/ — common helper code used by multiple assignments
- saved_states/ — reference and example scene states
- build/ — generated build artifacts (do not include in submission archives)

## Creating a submission package (recommended)

Package only the files required to build and run the assignment on the graders' machines. Typically this is the content of the assignment subfolder, excluding the `build/` directory and the provided `reference.exe`.

A convenient way to make a ZIP from the current committed state of the assignment folder is:

```
git archive --output=STUDENTID-Assignment1-v1.zip HEAD .
```

Make sure any files your submission needs are tracked in Git (no important untracked files). Verify the ZIP on a clean machine: it should configure with CMake, build, and run.

See the `instructions.pdf` inside each assignment folder for assignment-specific requirements and grading notes.

## Build (windows — primary platform)

The project uses CMake presets and vcpkg to obtain third-party dependencies. On Windows we target Visual Studio 2022 (x64). Typical steps from the assignment directory:

```
cmake -B build --preset=configure-vs2022
cmake --build build --config Debug
```

Open `build/assignment1.sln` in Visual Studio if you prefer the IDE. The presets handle downloading/installing packages via vcpkg where needed.

## Build (macOS / Linux — best-effort)

Presets for macOS (Xcode) and generic Debug/Release builds exist. On macOS/Linux you typically need vcpkg installed and available in your environment. Example:

```
cmake -B build --preset=configure-xcode   # macOS/Xcode
cmake -B build --preset=Debug             # Linux / generic
cmake --build build
```

If you run into platform-specific issues, consult the per-assignment `instructions.pdf` or ask on the course Slack/communication channels.

## Repository layout (high level)

- assignment1/
  - src/, assets/, instructions.pdf, reference.exe, vcpkg.json, CMake presets
- assignment2/
  - src/, assets/, instructions.pdf, reference.exe, vcpkg.json, CMake presets
- shared_sources/
- saved_states/
- build/ (generated; generally not committed for submissions)

## Reference and testing

Each assignment folder contains a `reference.exe` and a few `saved_states/*.json` and `*.png` files illustrating expected results. Use these to compare your output during development.

## Notes

- Keep your submissions small: exclude `build/` and large generated files. Track any extra assets needed for grading in Git so they're included when you create the archive.
- Follow the assignment `instructions.pdf` for deadlines, grading policy, and any round-specific rules.
