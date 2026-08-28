#include "TextEditorFindReplace.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <imgui.h>
#include <regex>

namespace rigkit {

void TextEditorFindReplace::handleShortcuts() {
	if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F)) {
		m_findVisible = true;
		m_replaceVisible = false;
	}
	if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_F)) {
		m_findVisible = true;
		m_replaceVisible = true;
	}
	if (m_findVisible && ImGui::IsKeyPressed(ImGuiKey_Escape))
		m_findVisible = false;
}

void TextEditorFindReplace::showFind(bool withReplace) {
	m_findVisible = true;
	m_replaceVisible = withReplace;
}

void TextEditorFindReplace::draw(TextEditor& editor, const char* idPrefix) {
	if (!m_findVisible)
		return;

	ImGui::Separator();

	const std::string findId = std::string("##") + idPrefix + "_find";
	const std::string replaceId = std::string("##") + idPrefix + "_replace";
	const std::string nextId = std::string("Next##") + idPrefix + "_find";
	const std::string allId = std::string("All##") + idPrefix + "_find";
	const std::string closeId = std::string("x##") + idPrefix + "_find";
	const std::string replCurId = std::string("Replace##") + idPrefix + "_find_current";
	const std::string replAllId = std::string("Replace All##") + idPrefix + "_find_all";

	ImGui::SetNextItemWidth(280.f);
	const bool enterPressed = ImGui::InputText(findId.c_str(), m_findBuf, sizeof(m_findBuf),
											   ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::SameLine();
	ImGui::Checkbox("Aa", &m_caseSensitive);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Case Sensitive");

	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Text, m_useRegex
											 ? ImGui::GetStyleColorVec4(ImGuiCol_CheckMark)
											 : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	if (ImGui::SmallButton("[.*]"))
		m_useRegex = !m_useRegex;
	ImGui::PopStyleColor();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Regular Expression");

	ImGui::SameLine();
	ImGui::BeginDisabled(m_useRegex);
	ImGui::PushStyleColor(ImGuiCol_Text, (m_wholeWord && !m_useRegex)
											 ? ImGui::GetStyleColorVec4(ImGuiCol_CheckMark)
											 : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	if (ImGui::SmallButton("[\"]"))
		m_wholeWord = !m_wholeWord;
	ImGui::PopStyleColor();
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("Whole Word");

	ImGui::SameLine();

	auto offsetToCoords = [](const std::string& text, size_t offset) -> std::pair<int, int> {
		int line = 0, col = 0;
		for (size_t i = 0; i < offset && i < text.size(); ++i) {
			if (text[i] == '\n') {
				++line;
				col = 0;
			} else {
				++col;
			}
		}
		return {line, col};
	};

	auto doFindNext = [&] {
		if (m_findBuf[0] == '\0')
			return;
		if (m_useRegex) {
			try {
				auto rxFlags = std::regex_constants::ECMAScript;
				if (!m_caseSensitive)
					rxFlags |= std::regex_constants::icase;
				std::regex pat(m_findBuf, rxFlags);
				std::string full = editor.GetText();
				int curLine = 0, curCol = 0;
				editor.GetCursorPosition(curLine, curCol);
				size_t startOff = 0;
				{
					int ln = 0, col = 0;
					for (size_t i = 0; i < full.size(); ++i) {
						if (ln == curLine && col >= curCol) {
							startOff = i;
							break;
						}
						if (full[i] == '\n') {
							++ln;
							col = 0;
						} else {
							++col;
						}
					}
				}
				std::smatch sm;
				bool found = std::regex_search(full.cbegin() + (std::ptrdiff_t)startOff,
											   full.cend(), sm, pat);
				if (!found)
					found = std::regex_search(full.cbegin(), full.cend(), sm, pat);
				if (found) {
					const size_t s =
						(size_t)(sm.prefix().first - full.cbegin()) + (size_t)sm.position();
					const size_t e = s + (size_t)sm.length();
					const auto [sl, sc] = offsetToCoords(full, s);
					const auto [el, ec] = offsetToCoords(full, e);
					editor.SelectRegion(sl, sc, el, ec);
				}
			} catch (std::regex_error&) {
			}
		} else {
			editor.SelectNextOccurrenceOf(m_findBuf, (int)strlen(m_findBuf), m_caseSensitive,
										  m_wholeWord);
		}
	};

	auto doFindAll = [&] {
		if (m_findBuf[0] == '\0')
			return;
		if (m_useRegex) {
			doFindNext();
		} else {
			editor.SelectAllOccurrencesOf(m_findBuf, (int)strlen(m_findBuf), m_caseSensitive,
										  m_wholeWord);
		}
	};

	if (ImGui::SmallButton(nextId.c_str()) || enterPressed)
		doFindNext();
	ImGui::SameLine();
	if (ImGui::SmallButton(allId.c_str()))
		doFindAll();
	ImGui::SameLine();
	if (ImGui::SmallButton(closeId.c_str()))
		m_findVisible = false;

	if (m_findBuf[0] != '\0') {
		int count = 0;
		bool invalidRegex = false;

		if (m_useRegex) {
			try {
				auto rxFlags = std::regex_constants::ECMAScript;
				if (!m_caseSensitive)
					rxFlags |= std::regex_constants::icase;
				std::regex pat(m_findBuf, rxFlags);
				std::string full = editor.GetText();
				count = (int)std::distance(std::sregex_iterator(full.begin(), full.end(), pat),
										   std::sregex_iterator{});
			} catch (std::regex_error&) {
				invalidRegex = true;
			}
		} else {
			const std::string full = editor.GetText();
			const std::string term = m_findBuf;
			std::string haystack = m_caseSensitive ? full : [&] {
				std::string lc = full;
				std::transform(lc.begin(), lc.end(), lc.begin(), ::tolower);
				return lc;
			}();
			std::string needle = m_caseSensitive ? term : [&] {
				std::string lc = term;
				std::transform(lc.begin(), lc.end(), lc.begin(), ::tolower);
				return lc;
			}();
			auto isWordChar = [](char c) { return std::isalnum((unsigned char)c) || c == '_'; };
			for (size_t pos = 0; (pos = haystack.find(needle, pos)) != std::string::npos;
				 pos += needle.size()) {
				if (m_wholeWord) {
					const bool ok = (pos == 0 || !isWordChar(haystack[pos - 1])) &&
									(pos + needle.size() >= haystack.size() ||
									 !isWordChar(haystack[pos + needle.size()]));
					if (!ok)
						continue;
				}
				++count;
			}
		}

		ImGui::SameLine();
		if (invalidRegex)
			ImGui::TextColored({1.f, 0.6f, 0.2f, 1.f}, "invalid regex");
		else if (count == 0)
			ImGui::TextColored({1.f, 0.4f, 0.4f, 1.f}, "no matches");
		else
			ImGui::TextDisabled("%d match%s", count, count == 1 ? "" : "es");
	}

	if (m_replaceVisible) {
		ImGui::SetNextItemWidth(280.f);
		ImGui::InputText(replaceId.c_str(), m_replaceBuf, sizeof(m_replaceBuf));
		ImGui::SameLine();

		const bool hasMatch = editor.AnyCursorHasSelection();
		ImGui::BeginDisabled(!hasMatch || editor.IsReadOnlyEnabled());
		if (ImGui::SmallButton(replCurId.c_str())) {
			if (m_findBuf[0] != '\0') {
				if (m_useRegex) {
					try {
						auto rxFlags = std::regex_constants::ECMAScript;
						if (!m_caseSensitive)
							rxFlags |= std::regex_constants::icase;
						std::regex pat(m_findBuf, rxFlags);
						std::string src = editor.GetText();
						std::smatch sm;
						if (std::regex_search(src.cbegin(), src.cend(), sm, pat)) {
							const size_t pos = (size_t)sm.position();
							src.replace(pos, (size_t)sm.length(), m_replaceBuf);
							editor.SetText(src);
							doFindNext();
						}
					} catch (std::regex_error&) {
					}
				} else {
					std::string src = editor.GetText();
					const std::string term = m_findBuf;
					const std::string repl = m_replaceBuf;
					std::string lower_src = src;
					std::string lower_term = term;
					if (!m_caseSensitive) {
						std::transform(lower_src.begin(), lower_src.end(), lower_src.begin(),
									   ::tolower);
						std::transform(lower_term.begin(), lower_term.end(), lower_term.begin(),
									   ::tolower);
					}
					auto isWordChar = [](char c) {
						return std::isalnum((unsigned char)c) || c == '_';
					};
					size_t pos = 0;
					while ((pos = lower_src.find(lower_term, pos)) != std::string::npos) {
						if (!m_wholeWord || ((pos == 0 || !isWordChar(lower_src[pos - 1])) &&
											 (pos + lower_term.size() >= lower_src.size() ||
											  !isWordChar(lower_src[pos + lower_term.size()])))) {
							src.replace(pos, term.size(), repl);
							editor.SetText(src);
							editor.SelectNextOccurrenceOf(m_findBuf, (int)strlen(m_findBuf),
														  m_caseSensitive, m_wholeWord);
							break;
						}
						pos += lower_term.size();
					}
				}
			}
		}
		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::BeginDisabled(editor.IsReadOnlyEnabled());
		if (ImGui::SmallButton(replAllId.c_str())) {
			if (m_findBuf[0] != '\0') {
				if (m_useRegex) {
					try {
						auto rxFlags = std::regex_constants::ECMAScript;
						if (!m_caseSensitive)
							rxFlags |= std::regex_constants::icase;
						std::regex pat(m_findBuf, rxFlags);
						std::string src = editor.GetText();
						const std::string result = std::regex_replace(src, pat, m_replaceBuf);
						if (result != src)
							editor.SetText(result);
					} catch (std::regex_error&) {
					}
				} else {
					std::string src = editor.GetText();
					const std::string term = m_findBuf;
					const std::string repl = m_replaceBuf;
					int replCount = 0;
					auto isWordChar = [](char c) {
						return std::isalnum((unsigned char)c) || c == '_';
					};
					if (m_caseSensitive) {
						for (size_t pos = 0; (pos = src.find(term, pos)) != std::string::npos;) {
							if (m_wholeWord) {
								const bool ok = (pos == 0 || !isWordChar(src[pos - 1])) &&
												(pos + term.size() >= src.size() ||
												 !isWordChar(src[pos + term.size()]));
								if (!ok) {
									pos += term.size();
									continue;
								}
							}
							src.replace(pos, term.size(), repl);
							pos += repl.size();
							++replCount;
						}
					} else {
						std::string lower = src;
						std::string lterm = term;
						std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
						std::transform(lterm.begin(), lterm.end(), lterm.begin(), ::tolower);
						for (size_t pos = 0; (pos = lower.find(lterm, pos)) != std::string::npos;) {
							if (m_wholeWord) {
								const bool ok = (pos == 0 || !isWordChar(lower[pos - 1])) &&
												(pos + lterm.size() >= lower.size() ||
												 !isWordChar(lower[pos + lterm.size()]));
								if (!ok) {
									pos += lterm.size();
									continue;
								}
							}
							src.replace(pos, term.size(), repl);
							lower.replace(pos, lterm.size(),
										  repl.size() > 0 ? std::string(repl.size(), ' ')
														  : std::string());
							pos += repl.size();
							++replCount;
						}
					}
					if (replCount > 0)
						editor.SetText(src);
				}
			}
		}
		ImGui::EndDisabled();
	}

	ImGui::Separator();
}

} // namespace rigkit
