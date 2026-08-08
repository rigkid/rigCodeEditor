#include "CodeEditorWindow.h"

#include <imgui.h>

namespace rigkit {

CodeEditorWindow::CodeEditorWindow(const std::string& title)
	: IWindow(title, ImGuiWindowFlags_MenuBar) {
	setCategory("Edit");
	m_panel.setup();
}

void CodeEditorWindow::render() {
	if (isVisible()) {
		// Undocked default footprint for heroes / first open (dock layout may override).
		ImGui::SetNextWindowPos(ImVec2(20.f, 44.f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(1040.f, 640.f), ImGuiCond_FirstUseEver);
	}
	IWindow::render();
}

void CodeEditorWindow::renderContents() {
	m_panel.drawContents();
}

} // namespace rigkit
