#include "TextEditorPanel.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

namespace fs = std::filesystem;

namespace rigkit {

void TextEditorPanel::setup() {
	m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::None);
	m_editor.SetPalette(TextEditor::PaletteId::Dark);
	m_editor.SetShowLineNumbersEnabled(true);
	m_editor.SetShowWhitespacesEnabled(false);
	m_editor.SetLineSpacing(1.0f);
}

void TextEditorPanel::setDialogCallbacks(
	std::function<void(const std::string& title, std::vector<std::string> filters,
					   ConfirmPath onConfirm)>
		openFile,
	std::function<void(const std::string& title, std::vector<std::string> filters,
					   ConfirmPath onConfirm)>
		saveFile) {
	m_openFile = std::move(openFile);
	m_saveFile = std::move(saveFile);
}

void TextEditorPanel::setText(const std::string& text, TextEditor::LanguageDefinitionId lang) {
	if (lang != TextEditor::LanguageDefinitionId::None)
		m_editor.SetLanguageDefinition(lang);
	m_editor.SetText(text);
	m_statusMessage.clear();
}

std::string TextEditorPanel::getText() const {
	return m_editor.GetText();
}

void TextEditorPanel::setLanguage(TextEditor::LanguageDefinitionId lang) {
	m_editor.SetLanguageDefinition(lang);
}

int TextEditorPanel::getUndoIndex() const {
	return m_editor.GetUndoIndex();
}

void TextEditorPanel::setSidebarEntries(std::vector<SidebarEntry> entries) {
	std::string activeKey;
	if (m_sidebarSelected >= 0 && m_sidebarSelected < (int)m_sidebarEntries.size()) {
		const auto& cur = m_sidebarEntries[m_sidebarSelected];
		activeKey = cur.id.empty() ? cur.label : cur.id;
	}

	m_sidebarEntries = std::move(entries);

	if (activeKey.empty()) {
		m_sidebarSelected = -1;
		return;
	}

	// Re-find by key so the host reordering or relabelling entries (a dirty
	// marker, say) does not yank the buffer out from under the caret.
	m_sidebarSelected = -1;
	for (int i = 0; i < (int)m_sidebarEntries.size(); ++i) {
		const auto& e = m_sidebarEntries[i];
		if ((e.id.empty() ? e.label : e.id) == activeKey) {
			m_sidebarSelected = i;
			break;
		}
	}
	if (m_sidebarSelected >= 0 &&
		m_sidebarEntries[m_sidebarSelected].revision != m_sidebarRevision) {
		loadSidebarEntry(m_sidebarSelected);
	}
}

bool TextEditorPanel::selectSidebarEntryById(const std::string& id) {
	if (id.empty()) {
		return false;
	}
	for (int i = 0; i < (int)m_sidebarEntries.size(); ++i) {
		const auto& e = m_sidebarEntries[i];
		if ((e.id.empty() ? e.label : e.id) == id) {
			activateSidebarEntry(i);
			return true;
		}
	}
	return false;
}

void TextEditorPanel::activateSidebarEntry(int index) {
	if (index < 0 || index >= (int)m_sidebarEntries.size())
		return;
	flushSidebarEntry();
	stashActiveBuffer();
	m_activeOpenFile = -1;
	m_sidebarSelected = index;
	loadSidebarEntry(index);
}

void TextEditorPanel::loadSidebarEntry(int index) {
	if (index < 0 || index >= (int)m_sidebarEntries.size())
		return;
	const auto& e = m_sidebarEntries[index];
	m_sidebarRevision = e.revision;

	if (e.read) {
		m_filePath = e.path;
		m_editor.SetText(e.read());
		if (e.lang != TextEditor::LanguageDefinitionId::None)
			m_editor.SetLanguageDefinition(e.lang);
		else if (!e.path.empty())
			detectLanguage(e.path);
		else
			m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::None);
		m_editor.SetReadOnlyEnabled(!e.write);
		m_statusMessage.clear();
	} else if (!e.path.empty()) {
		std::string text, err;
		if (readFileText(e.path, text, err)) {
			m_filePath = e.path;
			m_editor.SetText(text);
			if (e.lang != TextEditor::LanguageDefinitionId::None)
				m_editor.SetLanguageDefinition(e.lang);
			else
				detectLanguage(e.path);
			m_statusMessage.clear();
		} else {
			m_statusMessage = err;
		}
	}
	m_lastUndoIndex = m_editor.GetUndoIndex();
}

void TextEditorPanel::flushSidebarEntry() {
	if (m_sidebarSelected < 0 || m_sidebarSelected >= (int)m_sidebarEntries.size())
		return;
	const auto& e = m_sidebarEntries[m_sidebarSelected];
	if (e.write)
		e.write(m_editor.GetText());
}

int TextEditorPanel::getLineCount() const {
	return std::max(1, m_editor.GetLineCount());
}

int TextEditorPanel::getCursorLine() const {
	int line = 0, col = 0;
	m_editor.GetCursorPosition(line, col);
	return line;
}

void TextEditorPanel::setCursorLine(int line) {
	line = std::clamp(line, 0, getLineCount() - 1);
	m_editor.SetCursorPosition(line, 0);
}

void TextEditorPanel::setHighlightLine(int line) {
	m_highlightLine = line;
	if (line < 0) {
		m_editor.ClearSelections();
		return;
	}
	line = std::clamp(line, 0, getLineCount() - 1);
	m_editor.SelectLine(line);
	m_editor.SetViewAtLine(line, TextEditor::SetViewAtLineMode::Centered);
}

void TextEditorPanel::detectLanguage(const std::string& path) {
	std::string ext = fs::path(path).extension().string();
	if (!ext.empty() && ext.front() == '.')
		ext.erase(0, 1);
	for (char& c : ext)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

	if (ext == "glsl" || ext == "vert" || ext == "frag")
		m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Glsl);
	else if (ext == "hlsl")
		m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Hlsl);
	else if (ext == "cpp" || ext == "c" || ext == "h" || ext == "hpp")
		m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Cpp);
	else if (ext == "cs")
		m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Cs);
	else if (ext == "lua")
		m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Lua);
	else if (ext == "py")
		m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Python);
	else if (ext == "json")
		m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Json);
	else if (ext == "xml" || ext == "svg")
		m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Xml);
	else if (ext == "sql")
		m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Sql);
	else if (ext == "gcode" || ext == "nc" || ext == "cnc" || ext == "tap")
		m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Gcode);
	else if (ext == "md" || ext == "markdown")
		m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::Markdown);
	else
		m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::None);
}

bool TextEditorPanel::readFileText(const std::string& path, std::string& outText,
								   std::string& err) const {
	std::error_code ec;
	const auto size = fs::file_size(path, ec);
	if (ec) {
		err = "Could not read file size: " + path;
		return false;
	}
	if (size > kMaxLoadBytes) {
		err = "File too large for the text editor (" + std::to_string(size / (1024 * 1024)) +
			  " MB; max " + std::to_string(kMaxLoadBytes / (1024 * 1024)) +
			  " MB): " + fs::path(path).filename().string();
		return false;
	}

	std::ifstream ifs(path, std::ios::binary);
	if (!ifs) {
		err = "Could not open: " + path;
		return false;
	}

	outText.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());

	const std::size_t probe = std::min<std::size_t>(outText.size(), 8192);
	if (std::memchr(outText.data(), '\0', probe) != nullptr) {
		err = "Refusing binary file: " + fs::path(path).filename().string();
		outText.clear();
		return false;
	}
	return true;
}

void TextEditorPanel::stashActiveBuffer() {
	if (m_activeOpenFile < 0 || m_activeOpenFile >= (int)m_openFiles.size())
		return;
	auto& buf = m_openFiles[m_activeOpenFile];
	buf.text = m_editor.GetText();
	buf.path = m_filePath;
	buf.lang = m_editor.GetLanguageDefinition();
}

void TextEditorPanel::activateOpenFile(int index) {
	if (index < 0 || index >= (int)m_openFiles.size())
		return;
	flushSidebarEntry();
	m_sidebarSelected = -1;
	stashActiveBuffer();
	m_activeOpenFile = index;
	const auto& buf = m_openFiles[index];
	m_filePath = buf.path;
	m_editor.SetText(buf.text);
	m_editor.SetLanguageDefinition(buf.lang);
	m_statusMessage.clear();
}

void TextEditorPanel::openPath(const std::string& path) {
	std::string text, err;
	if (!readFileText(path, text, err)) {
		m_statusMessage = err;
		return;
	}

	flushSidebarEntry();
	m_sidebarSelected = -1;

	if (m_openFilesSidebar) {
		for (int i = 0; i < (int)m_openFiles.size(); ++i) {
			if (m_openFiles[i].path == path) {
				activateOpenFile(i);
				m_openFiles[i].text = text;
				m_editor.SetText(text);
				detectLanguage(path);
				m_openFiles[i].lang = m_editor.GetLanguageDefinition();
				return;
			}
		}
		stashActiveBuffer();
		OpenBuffer buf;
		buf.path = path;
		buf.text = text;
		m_openFiles.push_back(std::move(buf));
		m_activeOpenFile = (int)m_openFiles.size() - 1;
	}

	m_filePath = path;
	m_editor.SetText(text);
	detectLanguage(path);
	if (m_openFilesSidebar && m_activeOpenFile >= 0 && m_activeOpenFile < (int)m_openFiles.size()) {
		m_openFiles[m_activeOpenFile].lang = m_editor.GetLanguageDefinition();
	}
	m_statusMessage.clear();
}

void TextEditorPanel::closeOpenFile(int index) {
	if (index < 0 || index >= (int)m_openFiles.size())
		return;
	if (index == m_activeOpenFile)
		stashActiveBuffer();

	m_openFiles.erase(m_openFiles.begin() + index);

	if (m_openFiles.empty()) {
		m_activeOpenFile = -1;
		m_filePath.clear();
		m_editor.SetText("");
		m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::None);
		return;
	}

	if (m_activeOpenFile == index) {
		m_activeOpenFile = std::min(index, (int)m_openFiles.size() - 1);
		const auto& buf = m_openFiles[m_activeOpenFile];
		m_filePath = buf.path;
		m_editor.SetText(buf.text);
		m_editor.SetLanguageDefinition(buf.lang);
	} else if (m_activeOpenFile > index) {
		--m_activeOpenFile;
	}
}

void TextEditorPanel::newDocument() {
	flushSidebarEntry();
	m_sidebarSelected = -1;
	if (m_openFilesSidebar) {
		stashActiveBuffer();
		OpenBuffer buf;
		buf.path.clear();
		buf.text.clear();
		buf.lang = TextEditor::LanguageDefinitionId::None;
		m_openFiles.push_back(std::move(buf));
		m_activeOpenFile = (int)m_openFiles.size() - 1;
	}
	m_editor.SetText("");
	m_filePath.clear();
	m_editor.SetLanguageDefinition(TextEditor::LanguageDefinitionId::None);
	m_statusMessage.clear();
}

void TextEditorPanel::renameActivePath(const std::string& path) {
	m_filePath = path;
	detectLanguage(path);
	if (m_openFilesSidebar && m_activeOpenFile >= 0 && m_activeOpenFile < (int)m_openFiles.size()) {
		m_openFiles[m_activeOpenFile].path = path;
		m_openFiles[m_activeOpenFile].lang = m_editor.GetLanguageDefinition();
		m_openFiles[m_activeOpenFile].text = m_editor.GetText();
	}
}

void TextEditorPanel::writeActiveToDisk(const std::string& path) {
	std::ofstream ofs(path, std::ios::binary);
	if (!ofs) {
		m_statusMessage = "Could not save: " + path;
		return;
	}
	ofs << m_editor.GetText();
	renameActivePath(path);
	m_statusMessage.clear();
}

void TextEditorPanel::drawContents() {
	// Returns extension filters for IMui FileDialogs.
	auto getDialogFilters = [this]() -> std::vector<std::string> {
		auto lang = m_editor.GetLanguageDefinition();
		if (lang == TextEditor::LanguageDefinitionId::Gcode)
			return {".gcode", ".nc", ".cnc", ".tap", ".*"};
		if (lang == TextEditor::LanguageDefinitionId::Glsl)
			return {".glsl", ".vert", ".frag", ".hlsl", ".*"};
		if (lang == TextEditor::LanguageDefinitionId::Hlsl)
			return {".hlsl", ".vert", ".frag", ".glsl", ".*"};
		if (lang == TextEditor::LanguageDefinitionId::Cpp ||
			lang == TextEditor::LanguageDefinitionId::C)
			return {".cpp", ".c", ".h", ".hpp", ".*"};
		if (lang == TextEditor::LanguageDefinitionId::Cs)
			return {".cs", ".*"};
		if (lang == TextEditor::LanguageDefinitionId::Python)
			return {".py", ".*"};
		if (lang == TextEditor::LanguageDefinitionId::Lua)
			return {".lua", ".*"};
		if (lang == TextEditor::LanguageDefinitionId::Json)
			return {".json", ".*"};
		if (lang == TextEditor::LanguageDefinitionId::Xml)
			return {".xml", ".svg", ".*"};
		if (lang == TextEditor::LanguageDefinitionId::Sql)
			return {".sql", ".*"};
		if (lang == TextEditor::LanguageDefinitionId::Markdown)
			return {".md", ".markdown", ".*"};
		return {".cpp", ".h", ".hpp", ".c", ".py", ".glsl", ".gcode", ".json", ".txt", ".*"};
	};

	auto doOpen = [&] {
		if (!m_openFile)
			return;
		m_openFile("Open File",
				   {".cpp", ".h", ".hpp", ".c", ".cs", ".py", ".lua", ".glsl", ".vert", ".frag",
					".gcode", ".nc", ".json", ".xml", ".svg", ".txt", ".md", ".*"},
				   [this](const std::string& path) { openPath(path); });
	};

	auto doSave = [&] {
		if (!m_saveFile) {
			if (!m_filePath.empty())
				writeActiveToDisk(m_filePath);
			return;
		}
		if (m_filePath.empty()) {
			m_saveFile("Save As", getDialogFilters(),
					   [this](const std::string& path) { writeActiveToDisk(path); });
		} else {
			writeActiveToDisk(m_filePath);
		}
	};

	auto doSaveAs = [&] {
		if (!m_saveFile)
			return;
		m_saveFile("Save As", getDialogFilters(),
				   [this](const std::string& path) { writeActiveToDisk(path); });
	};

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
		if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N))
			newDocument();
		if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O))
			doOpen();
		if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) {
			if (ImGui::GetIO().KeyShift)
				doSaveAs();
			else
				doSave();
		}
		if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_W) && m_openFilesSidebar &&
			m_activeOpenFile >= 0) {
			closeOpenFile(m_activeOpenFile);
		}
		handleFindReplaceShortcuts();
	}

	if (ImGui::BeginMenuBar()) {

		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New", "Ctrl+N"))
				newDocument();
			ImGui::Separator();
			if (m_openResourceMenuDraw) {
				if (ImGui::BeginMenu("Open")) {
					if (ImGui::MenuItem("File...", "Ctrl+O"))
						doOpen();
					if (ImGui::BeginMenu("Resource")) {
						m_openResourceMenuDraw();
						ImGui::EndMenu();
					}
					ImGui::EndMenu();
				}
			} else if (ImGui::MenuItem("Open...", "Ctrl+O")) {
				doOpen();
			}
			ImGui::Separator();
			bool noPath = m_filePath.empty();
			ImGui::BeginDisabled(noPath || m_editor.IsReadOnlyEnabled());
			if (ImGui::MenuItem("Save", "Ctrl+S"))
				doSave();
			ImGui::EndDisabled();
			if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
				doSaveAs();
			if (m_openFilesSidebar && m_activeOpenFile >= 0) {
				ImGui::Separator();
				if (ImGui::MenuItem("Close", "Ctrl+W"))
					closeOpenFile(m_activeOpenFile);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit")) {
			bool ro = m_editor.IsReadOnlyEnabled();
			ImGui::BeginDisabled(!m_editor.CanUndo() || ro);
			if (ImGui::MenuItem("Undo", "Ctrl+Z"))
				m_editor.Undo();
			ImGui::EndDisabled();
			ImGui::BeginDisabled(!m_editor.CanRedo() || ro);
			if (ImGui::MenuItem("Redo", "Ctrl+Y"))
				m_editor.Redo();
			ImGui::EndDisabled();
			ImGui::Separator();
			if (ImGui::MenuItem("Find", "Ctrl+F"))
				showFind(false);
			if (ImGui::MenuItem("Find & Replace", "Ctrl+Shift+F"))
				showFind(true);
			ImGui::Separator();
			if (ImGui::MenuItem("Read Only", nullptr, ro))
				m_editor.SetReadOnlyEnabled(!ro);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View")) {
			if (ImGui::BeginMenu("Language")) {
				const struct {
					const char* label;
					TextEditor::LanguageDefinitionId id;
				} kLangs[] = {
					{"None", TextEditor::LanguageDefinitionId::None},
					{"C++", TextEditor::LanguageDefinitionId::Cpp},
					{"C", TextEditor::LanguageDefinitionId::C},
					{"C#", TextEditor::LanguageDefinitionId::Cs},
					{"Python", TextEditor::LanguageDefinitionId::Python},
					{"Lua", TextEditor::LanguageDefinitionId::Lua},
					{"JSON", TextEditor::LanguageDefinitionId::Json},
					{"XML", TextEditor::LanguageDefinitionId::Xml},
					{"SQL", TextEditor::LanguageDefinitionId::Sql},
					{"AngelScript", TextEditor::LanguageDefinitionId::AngelScript},
					{"GLSL", TextEditor::LanguageDefinitionId::Glsl},
					{"HLSL", TextEditor::LanguageDefinitionId::Hlsl},
					{"G-code", TextEditor::LanguageDefinitionId::Gcode},
					{"Markdown", TextEditor::LanguageDefinitionId::Markdown},
				};
				auto curLang = m_editor.GetLanguageDefinition();
				for (auto& l : kLangs)
					if (ImGui::MenuItem(l.label, nullptr, curLang == l.id))
						m_editor.SetLanguageDefinition(l.id);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Color Theme")) {
				const struct {
					const char* label;
					TextEditor::PaletteId id;
				} kPals[] = {
					{"Dark", TextEditor::PaletteId::Dark},
					{"Light", TextEditor::PaletteId::Light},
					{"Mariana", TextEditor::PaletteId::Mariana},
					{"Retro Blue", TextEditor::PaletteId::RetroBlue},
				};
				auto curPal = m_editor.GetPalette();
				for (auto& p : kPals)
					if (ImGui::MenuItem(p.label, nullptr, curPal == p.id))
						m_editor.SetPalette(p.id);
				ImGui::EndMenu();
			}
			ImGui::Separator();
			bool lineNums = m_editor.IsShowLineNumbersEnabled();
			if (ImGui::MenuItem("Line Numbers", nullptr, lineNums))
				m_editor.SetShowLineNumbersEnabled(!lineNums);
			bool autoInd = m_editor.IsAutoIndentEnabled();
			if (ImGui::MenuItem("Auto Indent", nullptr, autoInd))
				m_editor.SetAutoIndentEnabled(!autoInd);
			bool showWS = m_editor.IsShowWhitespacesEnabled();
			if (ImGui::MenuItem("Show Whitespace", nullptr, showWS))
				m_editor.SetShowWhitespacesEnabled(!showWS);
			bool shortTabs = m_editor.IsShortTabsEnabled();
			if (ImGui::MenuItem("Short Tabs", nullptr, shortTabs))
				m_editor.SetShortTabsEnabled(!shortTabs);
			ImGui::Separator();
			{
				int tabSz = m_editor.GetTabSize();
				ImGui::SetNextItemWidth(60.f);
				if (ImGui::DragInt("Tab Size", &tabSz, 1.f, 1, 8))
					m_editor.SetTabSize(tabSz);
			}
			{
				float ls = m_editor.GetLineSpacing();
				ImGui::SetNextItemWidth(60.f);
				if (ImGui::DragFloat("Line Spacing", &ls, 0.05f, 1.0f, 2.0f, "%.2f"))
					m_editor.SetLineSpacing(ls);
			}
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	if (m_toolbarDraw) {
		m_toolbarDraw();
		ImGui::Separator();
	}

	m_findReplace.draw(m_editor);

	const float statusH = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;

	const bool hasHostEntries = !m_sidebarEntries.empty();
	const bool hasOpenFiles = m_openFilesSidebar && !m_openFiles.empty();

	if (hasHostEntries || hasOpenFiles) {
		const ImGuiTabBarFlags tabFlags =
			ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_Reorderable;
		if (ImGui::BeginTabBar("##code_tabs", tabFlags)) {
			for (int i = 0; i < (int)m_sidebarEntries.size(); ++i) {
				auto& e = m_sidebarEntries[i];
				std::string display = e.label;
				bool dirty = false;
				if (display.size() >= 2 && display.compare(display.size() - 2, 2, " *") == 0) {
					dirty = true;
					display.resize(display.size() - 2);
				}
				const std::string stable =
					e.id.empty() ? display : e.id;
				const std::string tabLabel = display + "###src" + stable;

				ImGuiTabItemFlags itemFlags = 0;
				if (e.isActive || m_sidebarSelected == i)
					itemFlags |= ImGuiTabItemFlags_SetSelected;
				if (dirty)
					itemFlags |= ImGuiTabItemFlags_UnsavedDocument;

				ImGui::PushID(i);
				if (ImGui::BeginTabItem(tabLabel.c_str(), nullptr, itemFlags)) {
					if (m_sidebarSelected != i)
						activateSidebarEntry(i);
					ImGui::EndTabItem();
				}
				if (!e.path.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
					ImGui::SetTooltip("%s", e.path.c_str());
				if (ImGui::BeginPopupContextItem("##sidebar_ctx")) {
					for (const auto& action : e.actions) {
						if (action.label.empty() || !action.onClick)
							continue;
						if (ImGui::MenuItem(action.label.c_str()))
							action.onClick();
					}
					ImGui::EndPopup();
				}
				ImGui::PopID();
			}

			int closeIdx = -1;
			if (hasOpenFiles) {
				for (int i = 0; i < (int)m_openFiles.size(); ++i) {
					const auto& buf = m_openFiles[i];
					const std::string display =
						buf.path.empty() ? "untitled" : fs::path(buf.path).filename().string();
					const std::string tabLabel = display + "###open" + std::to_string(i);

					ImGuiTabItemFlags itemFlags = 0;
					if (m_activeOpenFile == i)
						itemFlags |= ImGuiTabItemFlags_SetSelected;

					bool open = true;
					ImGui::PushID(1000 + i);
					if (ImGui::BeginTabItem(tabLabel.c_str(), &open, itemFlags)) {
						if (m_activeOpenFile != i)
							activateOpenFile(i);
						ImGui::EndTabItem();
					}
					if (!open)
						closeIdx = i;
					if (!buf.path.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
						ImGui::SetTooltip("%s", buf.path.c_str());
					if (ImGui::BeginPopupContextItem("##openfile_ctx")) {
						if (ImGui::MenuItem("Close"))
							closeIdx = i;
						ImGui::EndPopup();
					}
					ImGui::PopID();
				}
			}

			ImGui::EndTabBar();
			if (closeIdx >= 0)
				closeOpenFile(closeIdx);
		}
	}

	ImVec2 editorSize = ImGui::GetContentRegionAvail();
	editorSize.y -= statusH;

	m_editor.Render("##code", ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows), editorSize);

	// The undo index moves on every edit, undo and redo — cheaper than diffing
	// the buffer, and enough to keep the host's copy authoritative.
	if (m_sidebarSelected >= 0) {
		const int undoIndex = m_editor.GetUndoIndex();
		if (undoIndex != m_lastUndoIndex) {
			m_lastUndoIndex = undoIndex;
			flushSidebarEntry();
		}
	}

	if (m_syncPlaybackFromCursor && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
		const int curLine = getCursorLine();
		if (curLine != m_lastReportedCursorLine) {
			m_lastReportedCursorLine = curLine;
			if (m_onCursorLineChanged)
				m_onCursorLineChanged(curLine);
		}
	}

	ImGui::Separator();
	int curLine = 0, curCol = 0;
	m_editor.GetCursorPosition(curLine, curCol);
	if (!m_statusMessage.empty()) {
		ImGui::TextColored(ImVec4(1.f, 0.45f, 0.4f, 1.f), "%s", m_statusMessage.c_str());
	} else {
		ImGui::TextDisabled("Ln %d, Col %d   |   %d lines   |   %s%s", curLine + 1, curCol + 1,
							m_editor.GetLineCount(), m_editor.GetLanguageDefinitionName(),
							m_editor.IsReadOnlyEnabled() ? "   [Read Only]" : "");
	}
}

} // namespace rigkit
