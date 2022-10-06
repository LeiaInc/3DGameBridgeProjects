// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H
#define ImTextureID unsigned long long // Change ImGui texture ID type to that of a 'reshade::api::resource_view' handle

#include <imgui.h>
#include <reshade.hpp>

#include <d3d12.h>
#include <d3dx12.h>
#include <combaseapi.h>
#include "sr/weaver/dx12weaver.h"

#include "sr/sense/eyetracker/eyetracker.h"
#include "sr/sense/core/inputstream.h"
#include "sr/sense/system/systemsense.h"
#include "sr/sense/system/systemevent.h"
#include "sr/utility/exception.h"
#include <DirectXMath.h>

#include "sr/types.h"

// add headers that you want to pre-compile here
#include "framework.h"
#endif //PCH_H
