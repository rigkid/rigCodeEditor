#pragma once
#include "core/U_core.h"

class CodeEditorApp : public rigkit::IApp {
  public:
	CodeEditorApp() {
		window().width = 1100;
		window().height = 720;
		window().title = "rigCodeEditor — codeEditor";
	}
	void setup() override;
	void update(float) override {}
	void draw() override {}
};
