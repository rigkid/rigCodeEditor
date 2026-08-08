# rigCodeEditor


![preview](examples/codeEditor/img/preview.png)


Shared **ImGui code editor** for RigKit tool apps.

Pack role: `*Editor` capability over app-owned text (same family as rigNodeEditor). Buffer
lives on the panel / window API (`getText` / `setText`) until a product needs `.rig`
serialization — then promote a POD later.

## Features

- **TextEditor** (ImGuiColorTextEdit) + language definitions
- Full panel chrome: File / Edit / View menus, find & replace (regex), open-files sidebar
- **JetBrains Mono** Regular via `IMui::registerFontAtlasHook`
- Host dialogs through `IMui` (`std::vector<std::string>` extension filters)

## Use

```cpp
rigkit::rigCodeEditor::Options opts;
opts.windowVisible = true; // false if the app shows the window later
opts.fontSize = 14.f;
packs->registerPack(std::make_shared<rigkit::rigCodeEditor>(opts));
// after setupAll:
auto ed = ui->getWindowManager()->getWindow<rigkit::CodeEditorWindow>("Code Editor");
ed->setText(src, TextEditor::LanguageDefinitionId::Python);
ed->panel().showFind(true);
```

Depends on **rigImGui** (`IMui` / `MWindow` / file dialogs / font atlas hooks).

## Example

```bash
cmake -S /examples/codeEditor -B /examples/codeEditor/build
cmake --build /examples/codeEditor/build
```

## Pi note

JetBrains embedded Regular (~270KB) + TextEditor are desktop-tool weight — fine for author
apps. Keep the pack opt-in via `app.json` for lean install binaries.

## License

Pack MIT. TextEditor: ImGuiColorTextEdit (MIT). JetBrains Mono: SIL OFL
(`third_party/fonts/OFL.txt`).

[API/docs](https://rigkid.github.io/rigCodeEditor/)
