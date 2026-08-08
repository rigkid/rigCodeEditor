#pragma once

#include <string>
#include "IWindow.h"
#include "TextEditorPanel.h"

namespace rigkit {

/**
 * @brief Dockable Code Editor — TextEditorPanel chrome + JetBrains Mono.
 * @details Begun with ImGuiWindowFlags_MenuBar; panel owns File/Edit/View,
 * find/replace, open-file tabs, and IMui dialog callbacks.
 */
class CodeEditorWindow : public IWindow {
  public:
	explicit CodeEditorWindow(const std::string& title = "Code Editor");

	void render() override;
	void renderContents() override;

	TextEditorPanel& panel() { return m_panel; }
	const TextEditorPanel& panel() const { return m_panel; }

	void setText(const std::string& text,
				 TextEditor::LanguageDefinitionId lang = TextEditor::LanguageDefinitionId::None) {
		m_panel.setText(text, lang);
	}
	std::string getText() const { return m_panel.getText(); }
	void setLanguage(TextEditor::LanguageDefinitionId lang) { m_panel.setLanguage(lang); }
	TextEditor& editor() { return m_panel.editor(); }
	const TextEditor& editor() const { return m_panel.editor(); }

	/** @brief Apply monospace font once the atlas hook has loaded it. */
	void setEditorFont(ImFont* font) { m_panel.setFont(font); }

  private:
	TextEditorPanel m_panel;
};

} // namespace rigkit
