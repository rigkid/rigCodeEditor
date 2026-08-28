#include "CodeEditorFonts.h"

#include "imgui.h"
#include "JetBrainsMono_Regular_ttf.h"

#include <cstdio>
#include <spdlog/spdlog.h>

namespace rigkit {

ImFont* loadCodeEditorFont(ImFontAtlas& atlas, float pixelSize) {
	float sz = pixelSize;
	if (sz < 8.f) {
		sz = 8.f;
	}
	if (sz > 48.f) {
		sz = 48.f;
	}

	ImFontConfig cfg;
	cfg.FontDataOwnedByAtlas = false;
	cfg.OversampleH = 2;
	cfg.OversampleV = 1;
	cfg.PixelSnapH = true;
	std::snprintf(cfg.Name, sizeof(cfg.Name), "JetBrains Mono");

	ImFont* font =
		atlas.AddFontFromMemoryTTF(const_cast<unsigned char*>(JetBrainsMono_Regular_ttf_data),
								   static_cast<int>(JetBrainsMono_Regular_ttf_size), sz, &cfg);
	if (font) {
		spdlog::info("[rigCodeEditor] Loaded JetBrains Mono at {:.0f}px", sz);
	} else {
		spdlog::warn("[rigCodeEditor] Failed to load JetBrains Mono");
	}
	return font;
}

} // namespace rigkit
