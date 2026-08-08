file(READ "${CMAKE_CURRENT_LIST_DIR}/pack.json" PACK_JSON)
string(JSON PACK_NAME GET "${PACK_JSON}" name)

foreach(_dep IN ITEMS rigImGui rigComponent)
	if(NOT TARGET ${_dep})
		if(DEFINED RIGKIT_ROOT AND NOT RIGKIT_ROOT STREQUAL "")
			set(_dep_dir "${RIGKIT_ROOT}/packs/${_dep}")
		else()
			set(_dep_dir "${CMAKE_SOURCE_DIR}/packs/${_dep}")
		endif()
		if(EXISTS "${_dep_dir}/CMakeLists.txt")
			add_subdirectory("${_dep_dir}" "${CMAKE_BINARY_DIR}/packs/${_dep}")
		else()
			message(FATAL_ERROR "rigCodeEditor requires ${_dep} under packs/${_dep}")
		endif()
	endif()
endforeach()

add_library(${PACK_NAME} STATIC
	src/rigCodeEditor.cpp
	src/rigCodeEditor.h
	src/CodeEditorWindow.cpp
	src/CodeEditorWindow.h
	src/CodeEditorFonts.cpp
	src/CodeEditorFonts.h
	src/TextEditorPanel.cpp
	src/TextEditorPanel.h
	src/TextEditorFindReplace.cpp
	src/TextEditorFindReplace.h
	src/TextEditorView.cpp
	src/TextEditorView.h
	third_party/ImGuiColorTextEdit/TextEditor.cpp
	third_party/ImGuiColorTextEdit/LanguageDefinitions.cpp
)
target_link_libraries(${PACK_NAME} PUBLIC rigkit)
foreach(_dep IN ITEMS rigImGui rigComponent)
	if(TARGET ${_dep})
		target_link_libraries(${PACK_NAME} PUBLIC ${_dep})
	endif()
endforeach()
target_include_directories(${PACK_NAME} PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/third_party/ImGuiColorTextEdit>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/third_party/fonts>
)
target_compile_definitions(${PACK_NAME} PRIVATE IMGUI_DEFINE_MATH_OPERATORS)
message(STATUS "Configured pack: ${PACK_NAME}")
