#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "TextEditor.h"
#include "TextEditorFindReplace.h"
#include "imgui.h"

namespace rigkit {

/**
 * @brief Full code-editor chrome (menu, find/replace, open-file tabs).
 * @details Draw into an existing ImGui window begun with ImGuiWindowFlags_MenuBar.
 * Host buffers and disk opens appear as Sublime-style tabs above the editor.
 */
class TextEditorPanel {
  public:
	using ConfirmPath = std::function<void(const std::string& path)>;

	/// Refuse loads larger than this (TextEditor builds one glyph per character).
	static constexpr std::size_t kMaxLoadBytes = 2 * 1024 * 1024;

	void setup();

	/// Host open/save (IMui FileDialogs — extension filters like ".py", ".*").
	void setDialogCallbacks(
		std::function<void(const std::string& title, std::vector<std::string> filters,
						   ConfirmPath onConfirm)>
			openFile,
		std::function<void(const std::string& title, std::vector<std::string> filters,
						   ConfirmPath onConfirm)>
			saveFile);

	/** @brief Menu bar + tabs + editor + status — call inside an open ImGui window. */
	void drawContents();

	void setText(const std::string& text,
				 TextEditor::LanguageDefinitionId lang = TextEditor::LanguageDefinitionId::None);
	std::string getText() const;
	void setLanguage(TextEditor::LanguageDefinitionId lang);

	struct SidebarAction {
		std::string label;
		std::function<void()> onClick;
	};
	/**
	 * @brief One host-supplied source tab — a file or a live buffer.
	 * @details Set `read` to back the entry with memory instead of disk; `write`
	 * then receives every edit, making the host's buffer the source of truth.
	 * Without `write` the entry loads read-only.
	 */
	struct SidebarEntry {
		std::string id; ///< Stable key across rebuilds; falls back to `label`.
		std::string label;
		std::string path;
		bool isActive = false;
		std::vector<SidebarAction> actions;
		std::function<std::string()> read;
		std::function<void(const std::string&)> write;
		TextEditor::LanguageDefinitionId lang = TextEditor::LanguageDefinitionId::None;
		/// Bump when the backing buffer changed underneath the editor.
		uint32_t revision = 0;
	};
	void setSidebarEntries(std::vector<SidebarEntry> entries);

	/** @brief Activate the host tab whose `id` matches (no-op if missing). */
	bool selectSidebarEntryById(const std::string& id);

	/** @brief Show disk-opened buffers as closable tabs (default on). */
	void setOpenFilesSidebarEnabled(bool enabled) { m_openFilesSidebar = enabled; }
	bool isOpenFilesSidebarEnabled() const { return m_openFilesSidebar; }

	const std::string& filePath() const { return m_filePath; }

	void setHighlightLine(int line);
	void setCursorLine(int line);
	int getCursorLine() const;
	int getLineCount() const;
	int getUndoIndex() const;

	void setSyncPlaybackFromCursor(bool enabled) { m_syncPlaybackFromCursor = enabled; }
	void setOnCursorLineChanged(std::function<void(int line)> cb) {
		m_onCursorLineChanged = std::move(cb);
	}

	void setToolbarDraw(std::function<void()> cb) { m_toolbarDraw = std::move(cb); }

	/**
	 * @brief Optional File → Open → Resource menu body (drawn live each frame).
	 * @details When set, File → Open becomes a submenu: File… + Resource → ….
	 */
	void setOpenResourceMenuDraw(std::function<void()> cb) {
		m_openResourceMenuDraw = std::move(cb);
	}

	void showFind(bool withReplace = false) { m_findReplace.showFind(withReplace); }
	void handleFindReplaceShortcuts() { m_findReplace.handleShortcuts(); }

	TextEditor& editor() { return m_editor; }
	const TextEditor& editor() const { return m_editor; }

	/** @brief Monospace editor font (JetBrains Mono). */
	void setFont(ImFont* font) {
		m_font = font;
		m_editor.SetFont(font);
	}

  private:
	struct OpenBuffer {
		std::string path;
		std::string text;
		TextEditor::LanguageDefinitionId lang = TextEditor::LanguageDefinitionId::None;
	};

	void detectLanguage(const std::string& path);
	bool readFileText(const std::string& path, std::string& outText, std::string& err) const;
	void stashActiveBuffer();
	void activateSidebarEntry(int index);
	void loadSidebarEntry(int index);
	/** @brief Push editor text back into the active host buffer. */
	void flushSidebarEntry();
	void activateOpenFile(int index);
	void openPath(const std::string& path);
	void closeOpenFile(int index);
	void newDocument();
	void renameActivePath(const std::string& path);
	void writeActiveToDisk(const std::string& path);

	TextEditor m_editor;
	ImFont* m_font = nullptr;
	std::string m_filePath;
	std::string m_statusMessage;

	std::function<void(const std::string& title, std::vector<std::string> filters, ConfirmPath)>
		m_openFile;
	std::function<void(const std::string& title, std::vector<std::string> filters, ConfirmPath)>
		m_saveFile;

	TextEditorFindReplace m_findReplace;

	std::vector<SidebarEntry> m_sidebarEntries;
	int m_sidebarSelected = -1;
	uint32_t m_sidebarRevision = 0; ///< Revision of the entry currently loaded.
	int m_lastUndoIndex = 0;		///< Cheap edit detector for buffer write-back.

	bool m_openFilesSidebar = true;
	std::vector<OpenBuffer> m_openFiles;
	int m_activeOpenFile = -1;

	int m_highlightLine = -1;
	bool m_syncPlaybackFromCursor = false;
	int m_lastReportedCursorLine = -1;
	std::function<void(int)> m_onCursorLineChanged;
	std::function<void()> m_toolbarDraw;
	std::function<void()> m_openResourceMenuDraw;
};

} // namespace rigkit
