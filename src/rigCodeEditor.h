#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "core/pack/IPack.h"
#include "TextEditor.h"

struct ImFont;

namespace rigkit {

class CodeEditorWindow;
class MEcs;

/**
 * @brief Shared ImGui code-editor pack for tool apps.
 * @details Registers a Code Editor window via IMui / MWindow. Mirrors every
 * `CCode` entity into tabs (edits write back); apps may also seed the window
 * with getText / setText. Vendors ImGuiColorTextEdit under third_party/.
 */
class rigCodeEditor : public IPack {
  public:
	/** @brief Bootstrap - register window, visibility, editor font size. */
	struct Options {
		bool registerWindow = true;
		bool windowVisible = true; ///< Hero default; hide then show when the app needs it.
		float fontSize = 14.f;
	};

	rigCodeEditor();
	explicit rigCodeEditor(Options opts);

	bool init() override;
	void setup() override;

	const Options& options() const { return m_opts; }
	ImFont* editorFont() const { return m_font; }

	/**
	 * @brief Highlighted inline TextEditor for a `CCode` buffer.
	 * @details Shared by Properties and kit panels (e.g. Snippets). Same slots  - 
	 * edits stay in sync when both are visible.
	 */
	bool drawLightEdit(uint32_t entity, std::string& text, const std::string& language,
					   float height, bool readOnly);

  private:
	struct PropLightEditor {
		TextEditor editor;
		std::string language;
		bool seeded = false;
	};

	/** @brief Mirror every `CCode` entity into the editor tabs. */
	void syncCodeBuffers(MEcs& ecs);
	void wirePropertiesHooks();

	Options m_opts;
	ImFont* m_font = nullptr;
	bool m_fontHookRegistered = false;
	CodeEditorWindow* m_window = nullptr;
	/// Cheap change detector so tabs are rebuilt only when buffers differ.
	std::string m_bufferSignature;
	std::unordered_map<uint32_t, PropLightEditor> m_propEditors;
};

} // namespace rigkit
