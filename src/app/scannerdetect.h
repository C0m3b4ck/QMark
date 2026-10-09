#pragma once

// ─────────────────────────────────────────────────────────────────────
// scannerdetect.h — best-effort hardware detection of a USB/HID
// barcode scanner (a "keyboard-wedge" device that types the code into
// the focused window). Used only to choose the DEFAULT state of scanner
// mode on first run: mode is enabled when a scanner-like device is
// found and stays off otherwise. The operator can always override it.
// ─────────────────────────────────────────────────────────────────────

namespace ScannerProbe {

// Returns true when the platform reports an input device that is
// scanner-like (an extra keyboard-type HID device, or a device whose
// name contains typical scanner/vendor keywords). Pure heuristics —
// never blocks, never throws, cached after the first call.
bool hardwareScannerPresent();

} // namespace ScannerProbe