#pragma once

// ─────────────────────────────────────────────────────────────────────
// version.h — the single source of truth for the application version.
//
// Bump BOTH constants before tagging a release. The updater module
// compares the release tag with this tag (numeric components, so any tag
// scheme works: "alpha_N", "v1.2.3", "1.2.10" ...) to decide whether an
// update exists. When switching tag conventions, keep counting up
// (e.g. alpha_13 → 14.0.0) so updates keep being detected.
// ─────────────────────────────────────────────────────────────────────

#define QMARK_VERSION_STRING "alpha_14"
#define QMARK_VERSION_SEQ    14

// Repository used by the automatic update checker (owner/repo).
#define QMARK_GITHUB_REPO    "C0m3b4ck/QMark"