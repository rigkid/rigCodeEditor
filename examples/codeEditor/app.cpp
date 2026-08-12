#include "app.h"

#include <spdlog/spdlog.h>

#include "CodeEditorWindow.h"
#include "core/RigKitEngine.h"
#include "core/pack/MPack.h"
#include "packs/rigCodeEditor/src/rigCodeEditor.h"
#include "packs/rigComponent/src/rigComponent.h"
#include "packs/rigImGui/src/rigImGui.h"
#include "packs/rigSystems/src/rigSystems.h"

namespace {

constexpr const char* kSamplePython = R"py(
# rigCodeEditor hero — JetBrains Mono + panel chrome
# Try: File Open/Save, Edit Find, View Language / Show Whitespace

def greet(name: str) -> str:
	# tab-indented block (monospace spacing check)
	msg = f"hello, {name}"
	print(msg)
	return msg

if __name__ == "__main__":
	greet("RigKit")
)py";

} // namespace

void CodeEditorApp::setup() {
	spdlog::info("codeEditor — TextEditorPanel + JetBrains Mono");
	m_engine->setClearColor(0.10f, 0.11f, 0.14f, 1.0f);

	auto* packs = m_engine->getPackManager();
	if (!packs) {
		return;
	}
	packs->registerPack(std::make_shared<rigkit::rigComponent>());
	packs->registerPack(std::make_shared<rigkit::rigSystems>());
	packs->registerPack(std::make_shared<rigkit::rigImGui>());

	rigkit::rigCodeEditor::Options opts;
	opts.registerWindow = true;
	opts.windowVisible = true;
	opts.fontSize = 14.f;
	packs->registerPack(std::make_shared<rigkit::rigCodeEditor>(opts));

	packs->initAll();
	packs->setupAll();

	auto* ui = m_engine->getUiManager();
	if (!ui || !ui->getWindowManager()) {
		return;
	}
	auto ed = ui->getWindowManager()->getWindow<rigkit::CodeEditorWindow>("Code Editor");
	if (ed) {
		ed->setText(kSamplePython, TextEditor::LanguageDefinitionId::Python);
	}
}
