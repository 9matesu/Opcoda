add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/juce)

juce_add_plugin(Opcoda
    COMPANY_NAME "Opcoda"
    PLUGIN_MANUFACTURER_CODE Opc0
    PLUGIN_CODE Opcg
    FORMATS VST3 Standalone
    PRODUCT_NAME "Opcoda"
    # O Opcoda e' granular: gera som a partir do binario e nao recebe audio.
    # Sem IS_SYNTH o bundle sai com Sub Categories ["Fx"] e o Ableton lista o
    # plugin em Audio Effects, com uma pista de entrada inutil na cadeia.
    IS_SYNTH TRUE
    # Obrigatorio junto com IS_SYNTH TRUE. O Ableton recusa um instrumento sem
    # barramento de eventos MIDI, e o erro e' criptico:
    #   "plugin has instrument category, but no valid event input bus"
    # Com FALSE, o wrapper VST3 do JUCE devolve zero buses kEvent na entrada e
    # o plugin e' listado pelo moduleinfo.json mas nunca chega a abrir. O
    # barramento de eventos nao tem nada a ver com entrada de audio: e' o que
    # permite ao host entregar notas.
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Opcoda
    PRIVATE
        source/plugin_processor.cpp
        source/plugin_processor.h
        source/plugin_editor.cpp
        source/plugin_editor.h
        source/look_and_feel.h
        source/knob.h
        source/led.h
        source/boxed_label.h
        source/display_panel.h
        source/palette.h)

target_link_libraries(Opcoda
    PRIVATE
        opcoda_core
        juce::juce_audio_processors
        # O Standalone exige audio_utils e audio_devices; o VST3 nao, mas os dois
        # formatos sao do mesmo alvo. O gui_extra traz o FileChooser do editor.
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

target_compile_definitions(Opcoda PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
