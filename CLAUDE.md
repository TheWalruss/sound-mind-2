# CLAUDE.md — Sound Mind Studio

Guidance for Claude Code when working on this repository. This is a ground-up rewrite of Sound Mind Studio (previous version: Python/PySide6) as a professional DAW and image/graphics studio in C++, with **responsiveness and performance as central design constraints**. See `docs/tech-stack-decisions.md` for the technology choices and rationale.

## Project Context

- **Language**: Modern C++ (C++20/23)
- **Audio/DSP**: JUCE
- **GPU compute**: DirectX 12 Compute (spectrogram/DSP transforms, image processing)
- **Primary dev platform**: Windows on Arm (Snapdragon X) via native Arm64 Visual Studio + VS Code; cross-compiled/CI-validated for x64
- **Predecessor**: previous Python codebase, kept only as a "lessons learned" reference, not as code to port directly. Previous Python codebase is in ../sound-mind/
- Real-time audio paths must remain allocation-free, lock-free where needed, and never touch GC'd/interpreted code

## Required Workflow for Every Implementation Step

Follow this sequence for **every** unit of work (feature, fix, or refactor), without skipping or reordering steps. Treat each numbered step as a checkpoint — do not silently proceed past step 2 without my explicit confirmation.

1. **Read the design docs.** Re-read the relevant design documents in `docs/` (and any linked architecture notes) before writing or changing anything. Do not rely on memory of earlier sessions.
2. **Clarify or confirm.** Identify any ambiguities, gaps, or assumptions the design docs don't resolve. Present them to me explicitly and wait for my answer before proceeding. If there are no ambiguities, state that plainly and confirm your understanding of the task in a sentence or two before continuing.
3. **Bump the version number.** Update the version per semantic versioning (`MAJOR.MINOR.PATCH`) in all locations it's declared (build files, headers/constants, packaging metadata). Use judgment on major/minor/patch based on the nature of the change, but flag it to me if it's ambiguous (e.g., could be argued as either minor or major).
4. **Write tests first.** Add or update unit/integration tests for the behavior being implemented before writing the implementation. Tests should fail meaningfully against the current code.
5. **Implement.** Write the implementation to satisfy the tests and the design doc, following modern C++ idioms (RAII, smart pointers, no raw `new`/`delete`, no allocation in real-time audio callbacks).
6. **Run the new tests.** Confirm the new/updated tests pass against the new implementation.
7. **Run the full regression suite.** Run the entire existing test suite to confirm nothing else broke. Do not proceed if there are regressions — fix or report them first.
8. **Update documentation.** Update `README.md`, `USER_GUIDE.md`, and `CHANGELOG.md` to reflect the change, in a style and level of detail consistent with the existing docs.
9. **Build the documents.** Run whatever generates the polished/distributable documentation output (doc site, PDF, or other build target) so the updated docs are verified to build cleanly, not just edited as source.
10. **Commit to GitHub.** Stage and commit with a clear, conventional commit message (what changed and why). Push if a remote is configured and I've indicated pushes are expected at this stage. Do not force-push, rewrite history, or touch branch protections.
11. **Summary report.** Give me a concise summary of what changed, why, and any trade-offs or deviations from the design doc. Suggest specific manual tests I should run myself (what to click/do, what result to expect) before considering the step fully done.

## Commit & Push Policy

- **Design/planning phase** (changes confined to `docs/`, `CLAUDE.md`, and similar planning artifacts — no source, build, or test changes): commit and push directly, without waiting for my approval. Docs can always be revisited and disagreed with after the fact, so there's no downside to pushing eagerly here.
- **Development/implementation phase** (any source, build, or test change): push only when I've indicated pushes are expected at that stage, per step 10 above. This isn't caution about the change itself — it's about doing as much QA as possible locally first, to keep GitHub's bandwidth, CPU, and storage costs down once real code is involved.

## Working Principles

- **Design docs are the source of truth.** If an implementation detail conflicts with a design doc, stop and raise it rather than quietly resolving it either way.
- **No scope creep.** Implement what the current step calls for. Note "future work" ideas in the summary report rather than building them unasked.
- **Performance is not an afterthought.** For any code touching the audio callback, GPU compute dispatch, or the rendering path, call out in the summary report what was done (or should be done) to validate performance, not just correctness.
- **Arm64 + x64 compatibility.** Flag anything that's Arm64-only, x64-only, or uses SIMD intrinsics that don't have a NEON/SSE-AVX equivalent handled.
- **Ask, don't assume, on version bump severity, breaking changes, and anything touching public API/file-format compatibility.**

## What NOT to do without explicit confirmation

- Skipping straight from implementation to commit without running tests
- Silently resolving an ambiguity in the design docs instead of asking
- Force-pushing, squashing history, or amending previous commits
- Introducing a new third-party dependency not already discussed
- Changing the tech stack decisions recorded in `docs/tech-stack-decisions.md`
