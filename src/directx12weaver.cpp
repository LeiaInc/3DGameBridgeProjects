#include "directx12weaver.h"

bool srContextInitialized = false;
bool weaverInitialized = false;
SR::PredictingDX12Weaver* weaver = nullptr;
reshade::api::device* d3d12device = nullptr;

DirectX12Weaver::DirectX12Weaver(SR::SRContext* context)
{
    //Set context here.
    if (!srContextInitialized) {
        srContext = context;
        srContextInitialized = true;
    }
}

ID3D12Resource* CreateTestResourceTexture(ID3D12Device* device, unsigned int width, unsigned int height) {
    D3D12_CLEAR_VALUE ClearMask = {};
    ClearMask.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D12_HEAP_PROPERTIES HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_RESOURCE_DESC ResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(ClearMask.Format, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    ID3D12Resource* Framebuffer;
    device->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, &ClearMask, IID_PPV_ARGS(&Framebuffer));
    return Framebuffer;
}

ID3D12Resource* CreateTestResourceTexture(ID3D12Device* device, D3D12_RESOURCE_DESC ResourceDesc) {
    D3D12_CLEAR_VALUE ClearMask = {};
    ClearMask.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D12_HEAP_PROPERTIES HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    ID3D12Resource* Framebuffer;
    device->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, &ClearMask, IID_PPV_ARGS(&Framebuffer));
    return Framebuffer;
}

ID3D12DescriptorHeap* CreateRTVTestHeap(ID3D12Device* device, UINT& heapSize) {
    // Create RTV heap
    ID3D12DescriptorHeap* Heap;
    D3D12_DESCRIPTOR_HEAP_DESC RTVHeapDesc = {};
    RTVHeapDesc.NumDescriptors = 1;
    RTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    RTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    device->CreateDescriptorHeap(&RTVHeapDesc, IID_PPV_ARGS(&Heap));
    heapSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    return Heap;
}

void DirectX12Weaver::init_weaver(reshade::api::effect_runtime* runtime, reshade::api::resource rtv, reshade::api::resource back_buffer) {
    if (weaverInitialized) {
        return;
    }

    ID3D12CommandAllocator* CommandAllocator;
    ID3D12Device* dev = ((ID3D12Device*)d3d12device->get_native());
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocator));

    if (d3d12device) {
        reshade::log_message(3, "Can cast a device! to native Dx12 Device!!!!");
    }
    else {
        reshade::log_message(3, "Could not cast device!!!!");
        return;
    }

    // Describe and create the command queue.
    ID3D12CommandQueue* CommandQueue;
    D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
    QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    dev->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&CommandQueue));

    if (CommandQueue == nullptr)
    {
        return;
    }

    uint64_t back_buffer_index = runtime->get_current_back_buffer_index();
    char buffer[1000];
    sprintf(buffer, "backbuffer num: %l", back_buffer_index);
    reshade::api::resource res = runtime->get_current_back_buffer();
    uint64_t handle = res.handle;

    sprintf(buffer, "resource value: %l", handle);
    reshade::log_message(4, buffer);

    ID3D12Resource* native_frame_buffer = (ID3D12Resource*)rtv.handle;
    ID3D12Resource* native_back_buffer = (ID3D12Resource*)back_buffer.handle;

    if (runtime->get_current_back_buffer().handle == (uint64_t)INVALID_HANDLE_VALUE) {
        reshade::log_message(3, "backbuffer handle is invalid");
    }

    try {
        weaver = new SR::PredictingDX12Weaver(*srContext, dev, CommandAllocator, CommandQueue/**(ID3D12CommandQueue*)runtime->get_command_queue()->get_native()**/, native_frame_buffer, native_back_buffer, (HWND)runtime->get_hwnd());
        srContext->initialize();
        reshade::log_message(3, "Initialized weaver");
    }
    catch (std::exception e) {
        reshade::log_message(3, e.what());
    }
    catch (...) {
        reshade::log_message(3, "Couldn't initialize weaver");
    }

    weaverInitialized = true;
}

void DirectX12Weaver::draw_debug_overlay(reshade::api::effect_runtime* runtime)
{
    ImGui::TextUnformatted("Some text");

    if (ImGui::Button("Press me to open an additional popup window"))
        g_popup_window_visible = true;

    if (g_popup_window_visible)
    {
        ImGui::Begin("Popup", &g_popup_window_visible);
        ImGui::TextUnformatted("Some other text");
        ImGui::End();
    }
}

void DirectX12Weaver::draw_sr_settings_overlay(reshade::api::effect_runtime* runtime)
{
    ImGui::Checkbox("Turn on SR", &g_popup_window_visible);
    ImGui::SliderFloat("View Separation", &view_separation, -50.f, 50.f);
    ImGui::SliderFloat("Vertical Shift", &vertical_shift, -50.f, 50.f);
}

void DirectX12Weaver::draw_settings_overlay(reshade::api::effect_runtime* runtime)
{
}

void DirectX12Weaver::on_reshade_finish_effects(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list, reshade::api::resource_view rtv, reshade::api::resource_view) {
    if (srContextInitialized) {
        reshade::api::resource rtv_resource = d3d12device->get_resource_from_view(rtv);
        reshade::api::resource_desc desc = d3d12device->get_resource_desc(rtv_resource);

        if (!weaverInitialized) {
            reshade::log_message(3, "init effect buffer copy");
            desc.type = reshade::api::resource_type::texture_2d;
            desc.heap = reshade::api::memory_heap::gpu_only;
            desc.usage = reshade::api::resource_usage::copy_dest;

            if (d3d12device->create_resource(reshade::api::resource_desc(desc.texture.width, desc.texture.height, desc.texture.depth_or_layers, desc.texture.levels, desc.texture.format, 1, reshade::api::memory_heap::gpu_only, reshade::api::resource_usage::copy_dest),
                nullptr, reshade::api::resource_usage::copy_dest, &effect_frame_copy)) {
                reshade::log_message(3, "Created resource");
            }
            else {
                reshade::log_message(3, "Failed creating resource");
                return;
            }

            init_weaver(runtime, effect_frame_copy, d3d12device->get_resource_from_view(rtv));
        }

        if (weaverInitialized) {
            reshade::api::resource_view view;
            d3d12device->create_resource_view(runtime->get_current_back_buffer(), reshade::api::resource_usage::render_target, d3d12device->get_resource_view_desc(rtv), &view);

            //cmd_list->barrier(effect_frame_copy, reshade::api::resource_usage::cpu_access, reshade::api::resource_usage::copy_dest);
            cmd_list->barrier(rtv_resource, reshade::api::resource_usage::render_target, reshade::api::resource_usage::copy_source);
            cmd_list->copy_resource(rtv_resource, effect_frame_copy);
            //reshade::log_message(3, "Got a copy!");
            //cmd_list->barrier(effect_frame_copy, reshade::api::resource_usage::copy_dest, reshade::api::resource_usage::cpu_access);
            cmd_list->barrier(rtv_resource, reshade::api::resource_usage::copy_source, reshade::api::resource_usage::render_target);

            //const float color[] = { 1.f, 0.3f, 0.5f, 1.f };
            //cmd_list->clear_render_target_view(view, color);
            cmd_list->bind_render_targets_and_depth_stencil(1, &view);

            weaver->setInputFrameBuffer((ID3D12Resource*)effect_frame_copy.handle);
            ID3D12GraphicsCommandList* native_cmd_list = (ID3D12GraphicsCommandList*)cmd_list->get_native();
            weaver->setCommandList(native_cmd_list);
            weaver->weave(desc.texture.width, desc.texture.height);
        }
    }
}

void DirectX12Weaver::on_init_effect_runtime(reshade::api::effect_runtime* runtime) {
    d3d12device = runtime->get_device();
}

void DirectX12Weaver::set_context_validity(bool isValid) {
    srContextInitialized = isValid;
}

bool DirectX12Weaver::is_initialized() {
    return srContextInitialized;
}
