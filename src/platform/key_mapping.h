#pragma once

/**
 * Key name / key code translation helpers (pure Qt, no platform headers
 * in this header).
 */

#include <QString>

// Qt key codes (Qt::Key_F6 = 0x01000035) are NOT Windows virtual-key
// codes (VK_F6 = 0x75). GetAsyncKeyState() only understands the latter.
// Returns 0 for keys we can't map, and always 0 on non-Windows.
int qtKeyToVk(int qtKey);

// "F6", "Esc", ... (QKeySequence text) -> Windows VK code, or 0.
int vkFromKeyName(const QString& qtKeyName);

// Qt's QKeySequence text and X11 key names don't always match
// (Qt says "Esc", X11 wants "Escape"). Translates the common ones;
// unknown names are returned unchanged.
QString x11KeyName(const QString& qtKeyName);
