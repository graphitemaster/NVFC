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

int main() {
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

	system("pause");
}