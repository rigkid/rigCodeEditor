#pragma once

#include "TextEditor.h"
#include "TextEditorFindReplace.h"
#include "imgui.h"

namespace rigkit {

/**
 * @brief Hosted TextEditor with find/replace chrome (no full menu bar).
 * @details Use for docked/custom panels; the full window is TextEditorPanel.
 */
class TextEditorView {
  public:
	void handleShortcuts() { m_findReplace.handleShortcuts(); }
	void showFind(bool withReplace = false) { m_findReplace.showFind(withReplace); }
	void drawFindReplace(TextEditor& editor, const char* idPrefix = "code") {
		m_findReplace.draw(editor, idPrefix);
	}

	TextEditorFindReplace& findReplace() { return m_findReplace; }
	const TextEditorFindReplace& findReplace() const { return m_findReplace; }

	/// Draw editor region; returns true if text changed this frame.
	static bool drawEditor(TextEditor& editor, float heightPx, bool readOnly = false);

  private:
	TextEditorFindReplace m_findReplace;
};

} // namespace rigkit
