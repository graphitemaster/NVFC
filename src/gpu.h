#ifndef GPU_H
#define GPU_H
#include <array>    // std::array
#include <optional> // std::optional

#include "nvapi.h"

class GPU {
public:
	using PCIIdentifiers = std::array<NV_U32, 4>;

	struct OverclockSetting {
		bool editable;
		float current_value;
		float min_value;
		float max_value;

		OverclockSetting();
		OverclockSetting(float min, float current, float max, bool editable = false);
	};

	struct OverclockFlag {
		bool editable;
		bool value;
	};

	struct OverclockProfile {
		OverclockSetting core_overclock;
		OverclockSetting memory_overclock;
		OverclockSetting shader_overclock;
		OverclockSetting overvolt;
		OverclockSetting power_limit;
		OverclockSetting thermal_limit;
		OverclockFlag thermal_limit_priority;
	};

	struct Usage {
		std::optional<float> gpu_usage;
		std::optional<float> fb_usage;
		std::optional<float> vid_usage;
		std::optional<float> bus_usage;
	};

	struct Clocks {
		std::optional<float> core_clock;
		std::optional<float> memory_clock;
		std::optional<float> shader_clock;
	};

	struct Memory {
		float total_memory;
		float free_memory;
		float used_memory;
	};

	GPU(NV_S32 adapter_index, NV_PHYSICAL_GPU_HANDLE physical_gpu_handle, NV_DISPLAY_HANDLE display_handle);

	const std::string &GetName() const;
	const std::string &GetSerialNumber() const;
	const PCIIdentifiers &GetPCIIdentifiers() const;
	std::optional<float> GetVoltage() const;
	std::optional<float> GetTemperature(NV_THERMAL_TARGET target) const;
	std::optional<Clocks> GetCurrentClocks() const;
	std::optional<Clocks> GetDefaultClocks() const;
	std::optional<Clocks> GetBaseClocks() const;
	std::optional<Clocks> GetBoostClocks() const;
	std::optional<Usage> GetUsage() const;
	std::optional<Memory> GetMemory() const;

	std::optional<OverclockProfile> GetOverclockProfile() const;

	bool SetDefaultFanSpeed();
	bool SetCustomFanSpeed(NV_U32 value);

	bool Update();

private:
	std::optional<Clocks> GetClocks(NV_CLOCK_FREQUENCY_TYPE type, bool compenate_for_over_clock = false) const;

	struct DataSet;

	NV_S32 m_adapter_index;
	NV_PHYSICAL_GPU_HANDLE m_physical_gpu_handle;
	NV_DISPLAY_HANDLE m_display_handle;
	std::unique_ptr<GPU::DataSet> m_data_set;
	std::string m_name;
	std::string m_serial_number;
	PCIIdentifiers m_pci_identifiers;
};

inline const std::string &GPU::GetName() const
{
	return m_name;
}

inline const std::string &GPU::GetSerialNumber() const
{
	return m_serial_number;
}

inline const GPU::PCIIdentifiers &GPU::GetPCIIdentifiers() const
{
	return m_pci_identifiers;
}

#endif