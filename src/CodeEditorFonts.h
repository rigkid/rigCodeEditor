#pragma once

struct ImFont;
struct ImFontAtlas;

namespace rigkit {

/**
 * @brief Load JetBrains Mono Regular into the atlas.
 * @return Font pointer, or nullptr on failure.
 */
ImFont* loadCodeEditorFont(ImFontAtlas& atlas, float pixelSize = 14.0f);

} // namespace rigkit
