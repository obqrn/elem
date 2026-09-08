add_executable(RichEditorClickRegression
   rich_editor_click_regression.cpp
)

target_compile_features(RichEditorClickRegression PRIVATE cxx_std_20)
target_compile_definitions(RichEditorClickRegression PRIVATE
   NOMINMAX
   UNICODE
   _UNICODE
)
if (MINGW)
   target_link_options(RichEditorClickRegression PRIVATE -municode)
endif()
target_link_libraries(RichEditorClickRegression PRIVATE
   user32
   gdi32
)
add_dependencies(RichEditorClickRegression RichEditor)

add_test(
   NAME RichEditor.ClickSwitch
   COMMAND RichEditorClickRegression $<TARGET_FILE:RichEditor>
)
add_test(
   NAME RichEditor.SelectionClear
   COMMAND RichEditorClickRegression $<TARGET_FILE:RichEditor> --selection
)
add_test(
   NAME RichEditor.KeyboardCaret
   COMMAND RichEditorClickRegression $<TARGET_FILE:RichEditor> --keyboard
)
add_test(
   NAME RichEditor.UnicodeInput
   COMMAND RichEditorClickRegression $<TARGET_FILE:RichEditor> --unicode
)
add_test(
   NAME RichEditor.LargePaste
   COMMAND RichEditorClickRegression $<TARGET_FILE:RichEditor> --paste
)
add_test(
   NAME RichEditor.DpiRaster
   COMMAND RichEditorClickRegression $<TARGET_FILE:RichEditor> --dpi
)
add_test(
   NAME RichEditor.EditHistory
   COMMAND RichEditorClickRegression $<TARGET_FILE:RichEditor> --history
)
set_tests_properties(
   RichEditor.ClickSwitch
   RichEditor.SelectionClear
   RichEditor.KeyboardCaret
   RichEditor.UnicodeInput
   RichEditor.LargePaste
   RichEditor.DpiRaster
   RichEditor.EditHistory
   PROPERTIES
   LABELS "windows;interactive"
   TIMEOUT 30
   RUN_SERIAL TRUE
)
