#pragma once

#include "TextEditor.h"

namespace rigkit {

/**
 * @brief Find / replace UI for a TextEditor (literal, whole-word, regex).
 * @details Used by TextEditorPanel and TextEditorView.
 */
class TextEditorFindReplace {
  public:
	void handleShortcuts();
	void showFind(bool withReplace = false);
	void hideFind() { m_findVisible = false; }
	bool isVisible() const { return m_findVisible; }

	/// Draw find (and optional replace) controls. Call before editor Render().
	void draw(TextEditor& editor, const char* idPrefix = "code");

  private:
	char m_findBuf[256] = {};
	char m_replaceBuf[256] = {};
	bool m_caseSensitive = false;
	bool m_useRegex = false;
	bool m_wholeWord = false;
	bool m_findVisible = false;
	bool m_replaceVisible = false;
};

} // namespace rigkit
