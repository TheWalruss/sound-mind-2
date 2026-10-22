find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(sound-mind-studio-tests
    main.cpp
    test_about_dialog.cpp
    test_command_line.cpp
    test_audio_snippet_picker_dialog.cpp
    test_base_recipes.cpp
    test_axis_labels.cpp
    test_project_change.cpp
    test_export_controller.cpp
    test_import_controller.cpp
    test_playback_preparer.cpp
    test_background_job.cpp
    test_branch_curve_session.cpp
    test_canvas_widget.cpp
    test_checklist_dialog.cpp
    test_chord_generator_controller.cpp
    test_chord_generator_panel.cpp
    test_configure_devices_panel.cpp
    test_create_project_wizard.cpp
    test_device_combo_helpers.cpp
    test_device_configuration_widget.cpp
    test_equalizer_curve_widget.cpp
    test_filter_configuration_panel.cpp
    test_generator_dialog.cpp
    test_gradient_bar_widget.cpp
    test_gradient_editor_widget.cpp
    test_grid_config.cpp
    test_grid_panel.cpp
    test_harmonic_series_widget.cpp
    test_history_panel.cpp
    test_image_scale_picker_dialog.cpp
    test_polar_origin_dialog.cpp
    test_import_export.cpp
    test_import_helpers.cpp
    test_landing_page.cpp
    test_layer_controller.cpp
    test_layers_panel.cpp
    test_loop_panel.cpp
    test_macro_player.cpp
    test_macro_recorder.cpp
    test_main_window.cpp
    test_midi_configuration_panel.cpp
    test_midi_import.cpp
    test_midi_import_dialog.cpp
    test_mind_capture_dialog.cpp
    test_mind_wave_controller.cpp
    test_mind_wave_editor.cpp
    test_preset_indicator_label.cpp
    test_mind_waves_panel.cpp
    test_name_collision_dialog.cpp
    test_numeric_field.cpp
    test_paint_controller.cpp
    test_path_controller.cpp
    test_text_controller.cpp
    test_analysis_panel.cpp
    test_text_tool_panel.cpp
    test_path_edit_session.cpp
    test_pick_controller.cpp
    test_playback_controller.cpp
    test_selection_configuration_panel.cpp
    test_selection_controller.cpp
    test_playback_panel.cpp
    test_record_panel.cpp
    test_recent_projects.cpp
    test_resource_browser_controller.cpp
    test_resource_browser_panel.cpp
    test_rotary_dial_widget.cpp
    test_theme.cpp
    test_tone_curve_editor.cpp
    test_tool_configuration_panel.cpp
    test_tool_palette_controller.cpp
    test_project_history.cpp
    test_remaster.cpp
    test_video_export_dialog.cpp
    test_waveform_preview_widget.cpp
)

# The repository root - test_about_dialog.cpp checks the embedded licence and
# notices against the real LICENSE.md and THIRD_PARTY_NOTICES.md (v0.1.42.1).
target_compile_definitions(sound-mind-studio-tests PRIVATE SOUND_MIND_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

target_link_libraries(sound-mind-studio-tests
    PRIVATE
        sound-mind-studio-lib
        Qt6::Test
)

# One CTest test per QTest class, read from main.cpp's own
# SM_TEST_CLASS_ENTRY(...) list (the single source of truth), so each
# class gets its own timeout, pass/fail and --output-on-failure text
# instead of one all-or-nothing run. With no -o, main.cpp logs each class
# to a temporary file and copies it to stdout, since QTest's own text
# logger doesn't reach a piped stdout on Windows.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp" smStudioTestClassLines REGEX "SM_TEST_CLASS_ENTRY[(][A-Za-z0-9_]+[)],")
foreach(smStudioTestClassLine IN LISTS smStudioTestClassLines)
    string(REGEX MATCH "SM_TEST_CLASS_ENTRY[(]([A-Za-z0-9_]+)[)]" smStudioTestClassMatch "${smStudioTestClassLine}")
    set(smStudioTestClass "${CMAKE_MATCH_1}")
    add_test(NAME sound-mind-studio-tests.${smStudioTestClass}
             COMMAND sound-mind-studio-tests --class ${smStudioTestClass})
    # No real display in CI - the offscreen platform plugin ships with the
    # base Qt install, so QWidget-derived classes still construct and test.
    set_tests_properties(sound-mind-studio-tests.${smStudioTestClass} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 900)
endforeach()

# MainWindowTest builds and tears down a full MainWindow for each of its
# ~270 functions - about 38 minutes in a debug build on the dev machine,
# far past the per-class default above.
set_tests_properties(sound-mind-studio-tests.MainWindowTest PROPERTIES TIMEOUT 5400)
