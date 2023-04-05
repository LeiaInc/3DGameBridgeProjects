﻿#include "pch.h"
#include "igraphicsapi.h"
#include "directx11weaver.h"
#include "directx12weaver.h"
#include "hotkeymanager.h"

#include <chrono>
#include <functional>
#include <thread>
#include <vector>
#include <iostream>
#include <unordered_map>

#define CHAR_BUFFER_SIZE 256

using namespace std;

IGraphicsApi* weaverImplementation = nullptr;
SR::SRContext* srContext = nullptr;
SR::SwitchableLensHint* lensHint = nullptr;
HotKeyManager* hotKeyManager = nullptr;

//Currently we use this string to determine if we should toggle this shader on press of the shortcut. We can expand this to a list later.
static const std::string depth3DShaderName = "SuperDepth3D";
static const std::string fxaaShaderName = "FXAA";
static char g_charBuffer[CHAR_BUFFER_SIZE];
static size_t g_charBufferSize = CHAR_BUFFER_SIZE;

struct DeviceDataContainer {
    reshade::api::effect_runtime* current_runtime = nullptr;
    unordered_map<std::string, bool> allEnabledTechniques;
};

static void enumerateTechniques(reshade::api::effect_runtime* runtime, std::function<void(reshade::api::effect_runtime*, reshade::api::effect_technique, string&)> func)
{
    runtime->enumerate_techniques(nullptr, [func](reshade::api::effect_runtime* rt, reshade::api::effect_technique technique) {
        g_charBufferSize = CHAR_BUFFER_SIZE;
        rt->get_technique_name(technique, g_charBuffer, &g_charBufferSize);
        string name(g_charBuffer);
        func(rt, technique, name);
        });
}

//Todo: Move this function outside of the dllmain. 
//It was placed here because it needed access to the SRContext but it should be moved to another class for cleanliness sake.
static void executeHotKeyFunctionByType(std::map<shortcutType, bool> hotKeyList, reshade::api::effect_runtime* runtime) {
    std::map<shortcutType, bool>::iterator i;
    vector<reshade::api::effect_technique> togglable3DEffects = {};
    map<shortcutType, bool> toggleMap;

    for (i = hotKeyList.begin(); i != hotKeyList.end(); i++) {
        switch (i->first) {
        case shortcutType::toggleSR:
            //Here we want to completely disable all SR related functions including the eye tracker, weaver, context etc.
            break;
        case shortcutType::toggleLens:
            //Here we want to toggle to the lens and toggle weaving
            if (i->second) {
                lensHint->enable();
                //Bypass weave() call
                weaverImplementation->do_weave(true);
            }
            else {
                lensHint->disable();
                //Bypass weave() call
                weaverImplementation->do_weave(false);
            }
            break;
        case shortcutType::toggle3D:
            //Here we want to toggle Depth3D or any other 3D effect we use to create our second eye image.
            enumerateTechniques(runtime, [&togglable3DEffects](reshade::api::effect_runtime* runtime, reshade::api::effect_technique technique, string& name) {
                if (!name.compare(depth3DShaderName)) {
                        togglable3DEffects.push_back(technique);
                }
            });

            for (int effectIterator = 0; effectIterator < togglable3DEffects.size(); effectIterator++) {
                if (i->second) {
                    runtime->set_technique_state(togglable3DEffects[effectIterator], true);
                }
                else {
                    runtime->set_technique_state(togglable3DEffects[effectIterator], false);
                }
            }
            break;
        case shortcutType::toggleLensAnd3D:
            //Todo: This should look at the current state of the lens toggle and 3D toggle, then, flip those.This toggle having its own state isn't great.
            if (i->second) {
                toggleMap = { {shortcutType::toggleLens, true}, {shortcutType::toggle3D, true} };
            }
            else {
                toggleMap = { {shortcutType::toggleLens, false}, {shortcutType::toggle3D, false} };
            }
            executeHotKeyFunctionByType(toggleMap, runtime);
            break;
        case shortcutType::toggleLatencyMode:
            //Here we want to toggle the eye tracker latency mode between framerate-adaptive and latency-in-frames.
            if (i->second) {
                weaverImplementation->set_latency_mode(LatencyModes::latencyInFrames);
                // Todo: The amount of buffers set here should be configurable!
                // Set the latency only once.
                weaverImplementation->set_latency_in_frames(2);

                //Log the current mode:
                reshade::log_message(3, "Current latency mode set to: LATENCY_IN_FRAMES");
            }
            else {
                // Make sure to update the current frametime every frame when using this!

                // Todo: This method was not satisfactory, revisit this and move the STATIC 40000 microseconds mode from below!
                weaverImplementation->set_latency_mode(LatencyModes::framerateAdaptive);
                //reshade::log_message(3, "Current latency mode set to: FRAMERATE_ADAPTIVE");

                // Set the latency to the SR default of 40000 microseconds (Tuned for 60Hz)
                weaverImplementation->set_latency_framerate_adaptive(40000);
                reshade::log_message(3, "Current latency mode set to: STATIC 40000 Microseconds");
            }
            
        default:
            break;
        }
    }
}

static void draw_debug_overlay(reshade::api::effect_runtime* runtime) {
    //weaverImplementation->draw_debug_overlay(runtime);
}

static void draw_sr_settings_overlay(reshade::api::effect_runtime* runtime) {
    //weaverImplementation->draw_sr_settings_overlay(runtime);
}

static void draw_settings_overlay(reshade::api::effect_runtime* runtime) {
    //weaverImplementation->draw_settings_overlay(runtime);
}

static void on_reshade_reload_effects(reshade::api::effect_runtime* runtime) {
    vector<reshade::api::effect_technique> fxaaTechnique = {};

    // Todo: This is not a nice way of forcing on_finish_effects to trigger. Maybe make a dummy shader that you always turn on instead (or use a different callback)
    // Toggle FXAA.fx on
    enumerateTechniques(runtime, [&fxaaTechnique](reshade::api::effect_runtime* runtime, reshade::api::effect_technique technique, string& name) {
        if (!name.compare(fxaaShaderName)) {
            reshade::log_message(3, "Found FXAA.fx shader!");
            fxaaTechnique.push_back(technique);
        }
        });

    for (int effectIterator = 0; effectIterator < fxaaTechnique.size(); effectIterator++) {
        runtime->set_technique_state(fxaaTechnique[effectIterator], true);
        reshade::log_message(3, "Toggled FXAA to ensure on_finish_effects gets called.");
    }
}

static void on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view rtv_srgb) {
    std::map<shortcutType, bool> hotKeyList;

    //Check if certain hotkeys are being pressed
    if (hotKeyManager != nullptr) {
        //Find out which hotkeys have changed their toggled state, then execute their respective code.
        hotKeyList = hotKeyManager->checkHotKeys(runtime, srContext);
        executeHotKeyFunctionByType(hotKeyList, runtime);
    }

    weaverImplementation->on_reshade_finish_effects(runtime, cmd_list, rtv, rtv_srgb);
}

static void init_sr() {
    //Construct SR Context and senses
    if (srContext == nullptr) {
        srContext = new SR::SRContext;
        lensHint = SR::SwitchableLensHint::create(*srContext);
        srContext->initialize();
    }
}

static void on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    init_sr();

    //Todo: Move these hard-coded hotkeys to user-definable hotkeys in the .ini file
    //Register some standard hotkeys
    if (hotKeyManager == nullptr) {
        hotKeyManager = new HotKeyManager();
    }

    //Then, check the active graphics API and pass it a new context.
    if (weaverImplementation == nullptr) {
        switch (runtime->get_device()->get_api()) {
        case reshade::api::device_api::d3d11:
            weaverImplementation = new DirectX11Weaver(srContext);
            break;
        case reshade::api::device_api::d3d12:
            weaverImplementation = new DirectX12Weaver(srContext);
            break;
        default:
            //Games will be DX11 in the majority of cases.
            //Todo: This may still crash our code so we should leave the API switching to user input if we cannot detect it ourselves.
            reshade::log_message(3, "Unable to determine graphics API, attempting to switch to DX11...");
            weaverImplementation = new DirectX11Weaver(srContext);
            break;
        }
    }

    weaverImplementation->on_init_effect_runtime(runtime);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:

        if (!reshade::register_addon(hModule))
            return FALSE;

        reshade::register_event<reshade::addon_event::init_effect_runtime>(&on_init_effect_runtime);
        reshade::register_event<reshade::addon_event::reshade_finish_effects>(&on_reshade_finish_effects);
        reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(&on_reshade_reload_effects);

        //reshade::register_overlay("Test", &draw_debug_overlay);
        //reshade::register_overlay(nullptr, &draw_sr_settings_overlay);
        //reshade::log_message(3, "registered: draw_sr_settings_overlay");

        break;
    case DLL_PROCESS_DETACH:
        reshade::unregister_addon(hModule);
        //reshade::unregister_event<reshade::addon_event::reshade_finish_effects>(on_reshade_finish_effects);
        //reshade::unregister_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
        break;
    }
    return TRUE;
}
