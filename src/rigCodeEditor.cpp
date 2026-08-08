#include "rigCodeEditor.h"

#include <algorithm>
#include <cstdint>
#include <imgui.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>
#include "CodeEditorFonts.h"
#include "CodeEditorWindow.h"
#include "TextEditorView.h"
#include "MWindow.h"
#include "PropertiesWindow.h"
#include "core/IMui.h"
#include "core/RigKitEngine.h"
#include "core/pack/PackRegistry.h"
#include "CAssetRef.h"
#include "CCode.h"
#include "ecs/MEcs.h"

namespace rigkit {
namespace {

/** @brief `CCode::language` id → highlighter. Unknown ids stay unhighlighted. */
TextEditor::LanguageDefinitionId languageFromId(const std::string& id) {
	using Lang = TextEditor::LanguageDefinitionId;
	if (id == "gcode" || id == "nc")
		return Lang::Gcode;
	if (id == "svg" || id == "xml")
		return Lang::Xml;
	if (id == "python" || id == "py")
		return Lang::Python;
	if (id == "json")
		return Lang::Json;
	if (id == "cpp" || id == "c" || id == "h")
		return Lang::Cpp;
	if (id == "cs")
		return Lang::Cs;
	if (id == "lua")
		return Lang::Lua;
	if (id == "sql")
		return Lang::Sql;
	if (id == "glsl")
		return Lang::Glsl;
	if (id == "hlsl")
		return Lang::Hlsl;
	if (id == "markdown" || id == "md")
		return Lang::Markdown;
	return Lang::None;
}

} // namespace

rigCodeEditor::rigCodeEditor() : rigCodeEditor(Options{}) {}

rigCodeEditor::rigCodeEditor(Options opts) : IPack("rigCodeEditor"), m_opts(opts) {
	setDescription("ImGui code editor (TextEditorPanel + JetBrains Mono)");
	addDependency("rigImGui");
	addDependency("rigComponent"); // CCode buffers listed as editor tabs.
}

bool rigCodeEditor::init() {
	spdlog::info("[rigCodeEditor] init");
	return true;
}

void rigCodeEditor::setup() {
	auto* engine = getEngine();
	if (!engine) {
		return;
	}
	auto* ui = engine->getUiManager();
	if (!ui) {
		spdlog::warn("[rigCodeEditor] no IMui — skip Code Editor (need rigImGui)");
		return;
	}

	// One hook per pack instance; reloadFonts still re-runs registered hooks.
	if (!m_fontHookRegistered) {
		ui->registerFontAtlasHook([this](ImFontAtlas& atlas) {
			m_font = loadCodeEditorFont(atlas, m_opts.fontSize);
			if (m_window && m_font) {
				m_window->setEditorFont(m_font);
			}
			for (auto& [id, slot] : m_propEditors) {
				(void)id;
				if (m_font) {
					slot.editor.SetFont(m_font);
				}
			}
		});
		m_fontHookRegistered = true;
	}

	wirePropertiesHooks();

	if (!m_opts.registerWindow) {
		spdlog::info("[rigCodeEditor] registerWindow=false — Properties light edit only");
		return;
	}

	auto* wm = ui->getWindowManager();
	if (!wm) {
		return;
	}

	auto win = wm->createWindow<CodeEditorWindow>();
	m_window = win.get();

	win->panel().setDialogCallbacks(
		[ui](const std::string& title, std::vector<std::string> filters,
			 TextEditorPanel::ConfirmPath onConfirm) {
			ui->openFileDialog(title, std::move(filters), std::move(onConfirm));
		},
		[ui](const std::string& title, std::vector<std::string> filters,
			 TextEditorPanel::ConfirmPath onConfirm) {
			ui->saveFileDialog(title, std::move(filters), std::move(onConfirm));
		});

	if (m_font) {
		win->setEditorFont(m_font);
	}

	if (m_opts.windowVisible) {
		wm->showWindow("Code Editor");
	}

	// Re-wire now that m_window exists (Open in Code Editor needs it).
	wirePropertiesHooks();

	if (auto* ecs = engine->getECSManager()) {
		ecs->registerSystem("rigCodeEditor.codeBuffers", SystemPhase::Update,
							[this](MEcs& e) { syncCodeBuffers(e); });
	}

	spdlog::info("[rigCodeEditor] Code Editor window registered (visible={})",
				 m_opts.windowVisible);
}

void rigCodeEditor::wirePropertiesHooks() {
	auto* engine = getEngine();
	auto* ui = engine ? engine->getUiManager() : nullptr;
	auto* wm = ui ? ui->getWindowManager() : nullptr;
	if (!wm) {
		return;
	}
	auto props = wm->getWindow<PropertiesWindow>("Properties");
	if (!props) {
		return;
	}

	props->setOnOpenCodeEditor([this, wm](uint32_t entity) {
		if (!m_window) {
			return;
		}
		wm->showWindow("Code Editor");
		if (auto* ecs = getEngine() ? getEngine()->getECSManager() : nullptr) {
			syncCodeBuffers(*ecs);
		}
		m_window->panel().selectSidebarEntryById(std::to_string(entity));
	});

	props->setCodeLightEditDraw([this](uint32_t entity, std::string& text,
									   const std::string& language, float height, bool readOnly) {
		return drawPropertiesLightEdit(entity, text, language, height, readOnly);
	});
}

bool rigCodeEditor::drawPropertiesLightEdit(uint32_t entity, std::string& text,
											const std::string& language, float height,
											bool readOnly) {
	auto& slot = m_propEditors[entity];
	if (slot.language != language || !slot.seeded) {
		slot.editor.SetLanguageDefinition(languageFromId(language));
		slot.language = language;
	}
	if (m_font) {
		slot.editor.SetFont(m_font);
	}

	// Seed on first use / resync when the buffer changed underneath and we aren't typing.
	const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
	if (!slot.seeded || (!focused && slot.editor.GetText() != text)) {
		slot.editor.SetText(text);
		slot.seeded = true;
	}

	ImGui::PushID(static_cast<int>(entity));
	const bool changed = TextEditorView::drawEditor(slot.editor, height, readOnly);
	ImGui::PopID();
	if (changed) {
		text = slot.editor.GetText();
	}
	return changed;
}

void rigCodeEditor::syncCodeBuffers(MEcs& ecs) {
	if (!m_window) {
		return;
	}
	auto* reg = &ecs.registry();

	std::vector<entt::entity> buffers;
	for (auto e : reg->view<ecs::CCode>()) {
		buffers.push_back(e);
	}
	std::sort(buffers.begin(), buffers.end(), [reg](entt::entity a, entt::entity b) {
		const auto& ca = reg->get<ecs::CCode>(a);
		const auto& cb = reg->get<ecs::CCode>(b);
		if (ca.order != cb.order) {
			return ca.order < cb.order;
		}
		return ca.name < cb.name;
	});

	std::string signature;
	for (auto e : buffers) {
		const auto& code = reg->get<ecs::CCode>(e);
		signature += std::to_string(static_cast<std::uint32_t>(e)) + ':' + code.name + ':' +
					 std::to_string(code.epoch) + (code.dirty ? "*" : "") + ';';
	}
	if (signature == m_bufferSignature) {
		return;
	}
	m_bufferSignature = signature;

	std::vector<TextEditorPanel::SidebarEntry> entries;
	entries.reserve(buffers.size());
	for (auto e : buffers) {
		const auto& code = reg->get<ecs::CCode>(e);
		TextEditorPanel::SidebarEntry entry;
		entry.id = std::to_string(static_cast<std::uint32_t>(e));
		entry.label = code.dirty ? code.name + " *" : code.name;
		if (reg->all_of<ecs::CAssetRef>(e)) {
			entry.path = reg->get<ecs::CAssetRef>(e).path;
		}
		entry.lang = languageFromId(code.language);
		entry.revision = code.epoch;
		entry.read = [reg, e]() -> std::string {
			return reg->valid(e) && reg->all_of<ecs::CCode>(e) ? reg->get<ecs::CCode>(e).text
															   : std::string{};
		};
		if (!code.readOnly) {
			entry.write = [reg, e](const std::string& text) {
				if (!reg->valid(e) || !reg->all_of<ecs::CCode>(e)) {
					return;
				}
				auto& target = reg->get<ecs::CCode>(e);
				if (target.text == text) {
					return;
				}
				target.text = text;
				target.dirty = true;
			};
		}
		entries.push_back(std::move(entry));
	}
	m_window->panel().setSidebarEntries(std::move(entries));
}

} // namespace rigkit

namespace {
struct rigCodeEditorRegistrar {
	rigCodeEditorRegistrar() {
		rigkit::PackRegistry::instance().addFactory("rigCodeEditor", []() {
			return std::shared_ptr<rigkit::IPack>(std::make_shared<rigkit::rigCodeEditor>());
		});
	}
};
static rigCodeEditorRegistrar rigCodeEditor_auto_reg;
} // namespace
