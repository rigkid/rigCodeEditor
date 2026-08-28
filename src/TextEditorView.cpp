#include "TextEditorView.h"

#include <algorithm>
#include "imgui_internal.h" // GetTopMostPopupModal

namespace rigkit {

bool TextEditorView::drawEditor(TextEditor& editor, float heightPx, bool readOnly) {
	editor.SetReadOnlyEnabled(readOnly);
	const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
						 ImGui::GetTopMostPopupModal() == nullptr;
	const bool changed = editor.Render("##code_editor", focused, ImVec2(-1.f, heightPx), true);
	const int lines = std::max(1, editor.GetLineCount());
	ImGui::TextDisabled("%d lines", lines);
	return changed;
}

} // namespace rigkit
