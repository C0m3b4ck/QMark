#pragma once

// ─────────────────────────────────────────────────────────────────────
// version.h — the single source of truth for the application version.
//
// Bump BOTH constants before tagging a release. The updater module
// compares the numeric sequence (QMARK_VERSION_SEQ) with the "alpha_N"
// tag of the newest GitHub release to decide whether an update exists.
// ─────────────────────────────────────────────────────────────────────

#define QMARK_VERSION_STRING "alpha_13"
#define QMARK_VERSION_SEQ    13

// Repository used by the automatic update checker (owner/repo).
#define QMARK_GITHUB_REPO    "C0m3b4ck/QMark"