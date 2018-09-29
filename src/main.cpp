#include <algorithm>
#include <unordered_map>

#include "nvapi.h"
#include "log.h"
#include "gpu.h"

static const char *ThermalController(NV_THERMAL_CONTROLLER controller) {
	switch (controller) {
	case NV_THERMAL_CONTROLLER::NONE:          
		return "none";
	case NV_THERMAL_CONTROLLER::GPU_INTERNAL: 
		return "internal";
	case NV_THERMAL_CONTROLLER::ADM103:       
		return "ADM103";
	case NV_THERMAL_CONTROLLER::MAX6649:      
		return "MAX6649";
	case NV_THERMAL_CONTROLLER::MAX1617:      
		return "MAX1617";
	case NV_THERMAL_CONTROLLER::LM99:         
		return "LM99";
	case NV_THERMAL_CONTROLLER::LM89:         
		return "LM89";
	case NV_THERMAL_CONTROLLER::LM64:          
		return "LM64";
	case NV_THERMAL_CONTROLLER::ADT7473:       
		return "ADT7473";
	case NV_THERMAL_CONTROLLER::SBMAX6649:    
		return "SBMAX6649";
	case NV_THERMAL_CONTROLLER::VBIOSEVT:     
		return "VBIOSEVT";
	case NV_THERMAL_CONTROLLER::OS:           
		return "OS";
	}
	return "Unknown";
}

static const char *ThermalTarget(NV_THERMAL_TARGET target) {
	switch (target) {
	case NV_THERMAL_TARGET::NONE:
		return "none";
	case NV_THERMAL_TARGET::GPU:
		return "gpu";
	case NV_THERMAL_TARGET::MEMORY:
		return "memory";
	case NV_THERMAL_TARGET::POWER_SUPPLY:
		return "power supply";
	case NV_THERMAL_TARGET::BOARD:
		return "board";
	case NV_THERMAL_TARGET::ALL:
		return "all";
	case NV_THERMAL_TARGET::UNKNOWN:
		break;
	}

	return "unknown";
}

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_PRIVATE
#include "nuklear.h"

#define NK_GDI_IMPLEMENTATION
#include "nuklear_gdi.h"

int WINDOW_WIDTH = 650;
int WINDOW_HEIGHT = 400;

static LRESULT CALLBACK
WindowProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		WINDOW_WIDTH = LOWORD(lparam);
		WINDOW_HEIGHT = HIWORD(lparam);
		break;
	}

	if (nk_gdi_handle_event(wnd, msg, wparam, lparam)) {
		return 0;
	}

	return DefWindowProcW(wnd, msg, wparam, lparam);
}

int main();

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char*, int nShowCmd)
{
	main();
}

int main() {
	GdiFont* font;
	struct nk_context *ctx;

	WNDCLASSW wc;
	ATOM atom;
	RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
	DWORD style = WS_OVERLAPPEDWINDOW;
	DWORD exstyle = WS_EX_APPWINDOW;
	HWND wnd;
	HDC dc;
	int running = 1;
	int needs_refresh = 1;

	memset(&wc, 0, sizeof(wc));
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = GetModuleHandleW(0);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = L"NuklearWindowClass";
	atom = RegisterClassW(&wc);

	AdjustWindowRectEx(&rect, style, FALSE, exstyle);
	wnd = CreateWindowExW(0, wc.lpszClassName, L"NVFC",
		style | WS_VISIBLE | WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top,
		NULL, NULL, wc.hInstance, NULL);
	dc = GetDC(wnd);

	/* GUI */
	font = nk_gdifont_create("Roboto", 18);
	ctx = nk_gdi_init(font, dc, WINDOW_WIDTH, WINDOW_HEIGHT);

	NV_STATUS initialize = NvAPI_Initialize();

	NV_SHORT_STRING version = {};
	if (NvAPI_GetInterfaceVersionString(version) == 0) {
		Log::write("NvAPI version: %s", version);
	}

	NV_PHYSICAL_GPU_HANDLE gpu_handles[64];
	NV_S32 gpu_count = 0;
	if (NvAPI_EnumPhysicalGPUs(gpu_handles, &gpu_count) != 0) {
		Log::write("failed to enumerate GPU(s)");
		return 1;
	}

	std::unordered_map<NV_PHYSICAL_GPU_HANDLE, NV_DISPLAY_HANDLE> display_handles;
	for (NV_S32 i = 0; i < gpu_count; i++) {
		NV_DISPLAY_HANDLE display_handle;
		if (NvAPI_EnumDisplayHandle(i, &display_handle) != 0) {
			break;
		}

		NV_PHYSICAL_GPU_HANDLE handles_from_display[64];
		NV_U32 count_from_display = 0;
		if (NvAPI_GetPhysicalGPUsFromDisplay(display_handle, handles_from_display, &count_from_display) == 0) {
			for (NV_U32 handle = 0; handle < count_from_display; handle++) {
				display_handles.try_emplace(handles_from_display[handle], display_handle);
			}
		}
	}

	Log::write("discovered %d GPU(s)", gpu_count);

	std::vector<GPU*> gpus;
	for (NV_S32 i = 0; i < gpu_count; i++) {
		auto search = display_handles.find(gpu_handles[i]);
		if (search != display_handles.end()) {
			gpus.push_back(new GPU(i, gpu_handles[i], search->second));
		}
	}

	int close = 0;
	int once = 0;
	while (running) {
		nk_input_begin(ctx);
		MSG msg;
		if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				running = 0;
			} else {
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}
		nk_input_end(ctx);

		struct nk_style *s = &ctx->style;
		s->text.color = nk_rgba(255, 255, 255, 255);
		s->window.border_color = nk_rgba(45, 45, 45, 255);
		s->window.header.active = nk_style_item_color(nk_rgba(45, 45, 45, 255));
		s->window.background = nk_rgba(50, 57, 61, 255);
		s->window.fixed_background = nk_style_item_color(nk_rgba(50, 57, 61, 255));

		for (GPU *gpu : gpus) {
			gpu->Update();

			if (close) break;

			std::string name = gpu->GetName();

			if (nk_window_is_closed(ctx, &name[0])) {
				if (!once) once = 1; else running = 0;
			}

			if (nk_begin(ctx, &name[0], nk_rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT),
				NK_WINDOW_BORDER||NK_WINDOW_TITLE))
			{
				nk_layout_row_begin(ctx, NK_STATIC, 16, 2);
				{
					nk_layout_row_push(ctx, 150);
					nk_label(ctx, "Serial:", NK_TEXT_LEFT);

					nk_layout_row_push(ctx, 100);
					nk_label(ctx, gpu->GetSerialNumber().c_str(), NK_TEXT_LEFT);
				}
				nk_layout_row_end(ctx);

				auto identifiers = gpu->GetPCIIdentifiers();
				nk_layout_row_begin(ctx, NK_STATIC, 16, 8);
				{
					nk_layout_row_push(ctx, 150);
					nk_label(ctx, "PCI:", NK_TEXT_LEFT);

					char buffer[1024];
					nk_layout_row_push(ctx, 100);
					snprintf(buffer, sizeof buffer, "0x%08x", identifiers[0]);
					nk_label(ctx, buffer, NK_TEXT_LEFT);

					nk_layout_row_push(ctx, 100);
					snprintf(buffer, sizeof buffer, "0x%08x", identifiers[1]);
					nk_label(ctx, buffer, NK_TEXT_LEFT);

					nk_layout_row_push(ctx, 100);
					snprintf(buffer, sizeof buffer, "0x%08x", identifiers[2]);
					nk_label(ctx, buffer, NK_TEXT_LEFT);

					nk_layout_row_push(ctx, 100);
					snprintf(buffer, sizeof buffer, "0x%08x", identifiers[3]);
					nk_label(ctx, buffer, NK_TEXT_LEFT);
				}
				nk_layout_row_end(ctx);

				auto temp_gpu          = gpu->GetTemperature(NV_THERMAL_TARGET::GPU);
				auto temp_memory       = gpu->GetTemperature(NV_THERMAL_TARGET::MEMORY);
				auto temp_power_supply = gpu->GetTemperature(NV_THERMAL_TARGET::POWER_SUPPLY);
				auto temp_board        = gpu->GetTemperature(NV_THERMAL_TARGET::BOARD);

				auto temp = [&](const char *type, float value) {
					nk_layout_row_begin(ctx, NK_STATIC, 16, 3);
					{
						nk_layout_row_push(ctx, 150);
						nk_label(ctx, "Temperature:", NK_TEXT_LEFT);

						char fmt[1024];
						snprintf(fmt, sizeof fmt, "%.2fC", value);
						nk_layout_row_push(ctx, 100); // 100px
						nk_label(ctx, fmt, NK_TEXT_LEFT);

						nk_layout_row_push(ctx, 100);
						nk_label(ctx, type, NK_TEXT_LEFT);
					}
					nk_layout_row_end(ctx);
				};

				if (temp_gpu)          temp("GPU", *temp_gpu);
				if (temp_memory)       temp("MEMORY", *temp_memory);
				if (temp_power_supply) temp("POWER SUPPLY", *temp_power_supply);
				if (temp_board)        temp("BOARD", *temp_board);

				auto voltage = gpu->GetVoltage();
				if (voltage) {
					nk_layout_row_begin(ctx, NK_STATIC, 16, 2);
					{
						nk_layout_row_push(ctx, 150);
						nk_label(ctx, "Voltage:", NK_TEXT_LEFT);

						char format[1024];
						snprintf(format, sizeof format, "%.2fV", *voltage);

						nk_layout_row_push(ctx, 100);
						nk_label(ctx, format, NK_TEXT_LEFT);
					}
					nk_layout_row_end(ctx);		
				}

				auto current_clocks = gpu->GetCurrentClocks();
				auto base_clocks = gpu->GetBaseClocks();
				auto boost_clocks = gpu->GetBoostClocks();

				auto clock = [&](const char *line, const char *type, float value) {
					nk_layout_row_begin(ctx, NK_STATIC, 16, 3);
					{
						nk_layout_row_push(ctx, 150);
						nk_label(ctx, line, NK_TEXT_LEFT);

						char fmt[1024];
						snprintf(fmt, sizeof fmt, "%.2f MHz", value);
						nk_layout_row_push(ctx, 100); // 100px
						nk_label(ctx, fmt, NK_TEXT_LEFT);

						nk_layout_row_push(ctx, 100);
						nk_label(ctx, type, NK_TEXT_LEFT);
					}
					nk_layout_row_end(ctx);
				};

				if (current_clocks->core_clock)    clock("Current Clock:", "CORE", *current_clocks->core_clock);//Log::write(" Current Clock: %.2f MHz (Core)", *current_clocks->core_clock);
				if (current_clocks->memory_clock)  clock("Current Clock:", "MEMORY", *current_clocks->memory_clock);//Log::write(" Current Clock: %.2f MHz (Memory)", *current_clocks->memory_clock);
				if (current_clocks->shader_clock)  clock("Current Clock:", "SHADER", *current_clocks->shader_clock);//Log::write(" Current Clock: %.2f MHz (Shader)", *current_clocks->shader_clock);
				if (base_clocks->core_clock)       clock("Base Clock:",    "CORE", *base_clocks->core_clock);//Log::write(" Base Clock:    %.2f MHz (Core)", *base_clocks->core_clock);
				if (base_clocks->memory_clock)     clock("Base Clock:",    "MEMORY", *base_clocks->memory_clock);//Log::write(" Base Clock:    %.2f MHz (Memory)", *base_clocks->memory_clock);
				if (base_clocks->shader_clock)     clock("Base Clock:",    "SHADER", *base_clocks->shader_clock);//Log::write(" Base Clock:    %.2f MHz (Shader)", *base_clocks->shader_clock);
				if (boost_clocks->core_clock)      clock("Boost Clock:",   "CORE", *boost_clocks->core_clock);//Log::write(" boost Clock:    %.2f MHz (Core)", *boost_clocks->core_clock);
				if (boost_clocks->memory_clock)    clock("Boost Clock:",   "MEMORY", *boost_clocks->memory_clock);//Log::write(" boost Clock:    %.2f MHz (Memory)", *boost_clocks->memory_clock);
				if (boost_clocks->shader_clock)    clock("Boost Clock:",   "SHADER", *boost_clocks->shader_clock);//Log::write(" boost Clock:    %.2f MHz (Shader)", *boost_clocks->shader_clock);

				auto usage_widget = [&](const char *type, float usage) {
					nk_layout_row_begin(ctx, NK_STATIC, 16, 2);
					{
						nk_layout_row_push(ctx, 150);
						nk_label(ctx, type, NK_TEXT_LEFT);

						char format[1024];
						snprintf(format, sizeof format, "%.2f%%", usage);

						nk_layout_row_push(ctx, 150);
						nk_label(ctx, format, NK_TEXT_LEFT);
					}
					nk_layout_row_end(ctx);		
				};

				auto usage = gpu->GetUsage();
				if (usage) {
					if (usage->gpu_usage) usage_widget("GPU:", *usage->gpu_usage);
					if (usage->fb_usage)  usage_widget("Framebuffer:", *usage->fb_usage);
					if (usage->vid_usage) usage_widget("Video Engine:", *usage->vid_usage);
					if (usage->bus_usage) usage_widget("Bus:", *usage->bus_usage);
				}

				auto memory_widget = [&](const char *type, float value) {
					nk_layout_row_begin(ctx, NK_STATIC, 16, 2);
					{
						nk_layout_row_push(ctx, 150);
						nk_label(ctx, type, NK_TEXT_LEFT);

						char fmt[1024];
						snprintf(fmt, sizeof fmt, "%.2f MiB", value);
						nk_layout_row_push(ctx, 150); // 100px
						nk_label(ctx, fmt, NK_TEXT_LEFT);
					}
					nk_layout_row_end(ctx);
				};

				auto memory = gpu->GetMemory();
				if (memory) {
					memory_widget("Total Memory:", memory->total_memory);
					memory_widget("Free Memory:", memory->free_memory);
					memory_widget("Used Memory:", memory->used_memory);

					nk_layout_row_begin(ctx, NK_STATIC, 16, 2);
					{
						nk_layout_row_push(ctx, 150);
						nk_label(ctx, "Memory Load:", NK_TEXT_LEFT);

						char fmt[1024];
						snprintf(fmt, sizeof fmt, "%.2f%%", 100.0f * memory->used_memory / memory->total_memory);
						nk_layout_row_push(ctx, 150); // 100px
						nk_label(ctx, fmt, NK_TEXT_LEFT);
					}
					nk_layout_row_end(ctx);
				}
			}
			nk_end(ctx);

			break;
		}

		nk_gdi_render(nk_rgb(45,45,45));
	}

#if 0
	

	for (GPU *gpu : gpus) {
		if (!gpu->Update()) {
			Log::write("failed to read data from GPU");
		}

		auto name = gpu->GetName();
		auto serial_number = gpu->GetSerialNumber();
		auto pci_identifiers = gpu->GetPCIIdentifiers();
		Log::write(" Name:          %s", name.c_str());
		Log::write(" Serial #:      %s", serial_number.c_str());
		Log::write(" PCI:           0x%08X | 0x%08X | 0x%08X | 0x%08X", pci_identifiers[0], pci_identifiers[1], pci_identifiers[2], pci_identifiers[3]);

		auto temp_gpu = gpu->GetTemperature(NV_THERMAL_TARGET::GPU);
		auto temp_memory = gpu->GetTemperature(NV_THERMAL_TARGET::MEMORY);
		auto temp_power_supply = gpu->GetTemperature(NV_THERMAL_TARGET::POWER_SUPPLY);
		auto temp_board = gpu->GetTemperature(NV_THERMAL_TARGET::BOARD);
		if (temp_gpu)          Log::write(" Temperature:   %.2fC (GPU)", *temp_gpu);
		if (temp_memory)       Log::write(" Temperature:   %.2fC (Memory)", *temp_memory);
		if (temp_power_supply) Log::write(" Temperature:   %.2fC (Power Supply)", *temp_power_supply);
		if (temp_board)        Log::write(" Temperature:   %.2fC (Board)", *temp_board);

		auto t = gpu->GetVoltage();
		if (t) {
			Log::write(" Voltage:       %.2fV", *t);
		}

		auto current_clocks = gpu->GetCurrentClocks();
		auto base_clocks = gpu->GetBaseClocks();
		auto boost_clocks = gpu->GetBoostClocks();
		if (current_clocks->core_clock)   Log::write(" Current Clock: %.2f MHz (Core)", *current_clocks->core_clock);
		if (current_clocks->memory_clock) Log::write(" Current Clock: %.2f MHz (Memory)", *current_clocks->memory_clock);
		if (current_clocks->shader_clock) Log::write(" Current Clock: %.2f MHz (Shader)", *current_clocks->shader_clock);
		if (base_clocks->core_clock)      Log::write(" Base Clock:    %.2f MHz (Core)", *base_clocks->core_clock);
		if (base_clocks->memory_clock)    Log::write(" Base Clock:    %.2f MHz (Memory)", *base_clocks->memory_clock);
		if (base_clocks->shader_clock)    Log::write(" Base Clock:    %.2f MHz (Shader)", *base_clocks->shader_clock);
		if (boost_clocks->core_clock)     Log::write(" Boost Clock:   %.2f MHz (Core)", *boost_clocks->core_clock);
		if (boost_clocks->memory_clock)   Log::write(" Boost Clock:   %.2f MHz (Memory)", *boost_clocks->memory_clock);
		if (boost_clocks->shader_clock)   Log::write(" Boost Clock:   %.2f MHz (Shader)", *boost_clocks->shader_clock);
		
		auto usage = gpu->GetUsage();
		if (usage) {
			if (usage->gpu_usage) Log::write(" GPU Usage:     %.2f%%", *usage->gpu_usage);
			if (usage->fb_usage)  Log::write(" FB Usage:      %.2f%%", *usage->fb_usage);
			if (usage->vid_usage) Log::write(" Video Usage:   %.2f%%", *usage->vid_usage);
			if (usage->bus_usage) Log::write(" Bus Usage:     %.2f%%", *usage->bus_usage);
		}

		auto memory_ = gpu->GetMemory();
		if (memory_) {
			auto memory = *memory_;
			Log::write(" Total Memory:  %.2f MiB", memory.total_memory);
			Log::write(" Free Memory:   %.2f MiB", memory.free_memory);
			Log::write(" Used Memory:   %.2f MiB", memory.used_memory);
			Log::write(" Memory Load:   %.2f%%", 100.0f * memory.used_memory / memory.total_memory);
		}
	}

	NvAPI_Unload();
#endif
	//system("pause");
}