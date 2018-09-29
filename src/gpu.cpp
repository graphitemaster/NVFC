#include <sstream> // std::stringstream
#include <iomanip> // std::setfill, std::setw

#include "gpu.h"

struct GPU::DataSet {
	std::array<NV_CLOCK_FREQUENCIES_V2, static_cast<std::size_t>(NV_CLOCK_FREQUENCY_TYPE::LAST)> m_frequencies;
	NV_DYNAMIC_PSTATES_V1 m_dynamic_pstates;
	NV_GPU_PSTATES20_V2 m_pstates20;
	NV_GPU_POWER_POLICIES_INFO_V1 m_power_policies_info;
	NV_GPU_POWER_POLICIES_STATUS_V1 m_power_policies_status;
	NV_GPU_VOLTAGE_DOMAINS_STATUS_V1 m_voltage_domain_status;
	NV_GPU_THERMAL_SETTINGS_V2 m_thermal_settings;
	NV_GPU_THERMAL_POLICIES_INFO_V2 m_thermal_policies_info;
	NV_GPU_THERMAL_POLICIES_STATUS_V2 m_thermal_policies_status;
	NV_GPU_COOLER_SETTINGS_V2 m_cooler_settings;
	NV_MEMORY_INFO_V2 m_memory_info;
};

GPU::OverclockSetting::OverclockSetting() : OverclockSetting(0.0f, 0.0f, 0.0f, false)
{
}

GPU::OverclockSetting::OverclockSetting(float min, float current, float max, bool editable)
	: editable      { editable }
	, min_value     { min }
	, current_value { current }
	, max_value     { max }
{
}

bool LoadClockFrequencies(
	NV_PHYSICAL_GPU_HANDLE physical_gpu_handle,
	NV_CLOCK_FREQUENCIES_V2 *frequencies,
	NV_CLOCK_FREQUENCY_TYPE type)
{
	*frequencies = {};
	frequencies->clock_type = static_cast<NV_U32>(type);
	return NvAPI_GPU_GetAllClockFrequencies(physical_gpu_handle, frequencies) == 0;
}

bool LoadGPUThermalSettingsV2(
	NV_PHYSICAL_GPU_HANDLE physical_gpu_handle,
	NV_GPU_THERMAL_SETTINGS_V2 *thermal_settings)
{
	*thermal_settings = {};
	return NvAPI_GPU_GetThermalSettings(physical_gpu_handle, NV_THERMAL_TARGET::ALL, thermal_settings) == 0;
}

bool LoadGPUDynamicPStates(
	NV_PHYSICAL_GPU_HANDLE physical_gpu_handle,
	NV_DYNAMIC_PSTATES_V1 *pstates)
{
	*pstates = {};
	return NvAPI_GPU_GetDynamicPStates(physical_gpu_handle, pstates) == 0;
}

bool LoadGPUPStates20V2(
	NV_PHYSICAL_GPU_HANDLE physical_gpu_handle,
	NV_GPU_PSTATES20_V2 *pstates20)
{
	*pstates20 = { };
	return NvAPI_GPU_GetPStates20(physical_gpu_handle, pstates20) == 0;
}

bool LoadGPUPowerPoliciesInfo(
	NV_PHYSICAL_GPU_HANDLE physical_gpu_handle,
	NV_GPU_POWER_POLICIES_INFO_V1 *power_policies_info)
{
	*power_policies_info = {};
	return NvAPI_GPU_GetPowerPoliciesInfo(physical_gpu_handle, power_policies_info) == 0;
}

bool LoadGPUPowerPoliciesStatus(
	NV_PHYSICAL_GPU_HANDLE physical_gpu_handle,
	NV_GPU_POWER_POLICIES_STATUS_V1 *power_policies_status)
{
	*power_policies_status = {};
	return NvAPI_GPU_GetPowerPoliciesStatus(physical_gpu_handle, power_policies_status) == 0;
}

bool LoadGPUVoltageDomainsStatus(
	NV_PHYSICAL_GPU_HANDLE physical_gpu_handle,
	NV_GPU_VOLTAGE_DOMAINS_STATUS_V1 *voltage_domain_status)
{
	*voltage_domain_status = {};
	return NvAPI_GPU_GetVoltageDomainStatus(physical_gpu_handle, voltage_domain_status) == 0;
}

bool LoadGPUThermalPoliciesInfoV2(
	NV_PHYSICAL_GPU_HANDLE physical_gpu_handle,
	NV_GPU_THERMAL_POLICIES_INFO_V2 *thermal_policies_info)
{
	*thermal_policies_info = {};
	return NvAPI_GPU_GetThermalPoliciesInfo(physical_gpu_handle, thermal_policies_info) == 0;
}

bool LoadGPUThermalPoliciesStatusV2(
	NV_PHYSICAL_GPU_HANDLE physical_gpu_handle,
    NV_GPU_THERMAL_POLICIES_STATUS_V2 *thermal_policies_status)
{
	*thermal_policies_status = {};
	return NvAPI_GPU_GetThermalPoliciesStatus(physical_gpu_handle, thermal_policies_status) == 0;
}

bool LoadGPUCoolerSettingsV2(
	NV_PHYSICAL_GPU_HANDLE physical_gpu_handle,
	NV_S32 cooler_index,
	NV_GPU_COOLER_SETTINGS_V2 *cooler_settings)
{
	*cooler_settings = {};
	return NvAPI_GPU_GetCoolerSettings(physical_gpu_handle, cooler_index, cooler_settings) == 0;
}

GPU::OverclockSetting GetPowerLimit(
	const NV_GPU_POWER_POLICIES_INFO_V1 *const info,
	const NV_GPU_POWER_POLICIES_STATUS_V1 *const status,
	NV_U32 pstate = 0)
{
	for (NV_U32 i = 0; i < status->count; i++) {
		const auto &status_entry = status->entries[i];
		const auto &info_entry = info->entries[i];
		if (status_entry.pstate == pstate) {
			// NOTE(dweiler): unlock thermal limits, power policy values are multiples of 1000
			return {
				static_cast<float>(info_entry.min_power) / 1000.0f,
				static_cast<float>(status_entry.power) / 1000.0f,
				static_cast<float>(info_entry.max_power) / 1000.0f
			};
		}
	}
	return {};
}

std::tuple<GPU::OverclockSetting, GPU::OverclockFlag> GetThermalLimit(
	const NV_GPU_THERMAL_POLICIES_INFO_V2 *const info,
	const NV_GPU_THERMAL_POLICIES_STATUS_V2 *const status,
	const NV_THERMAL_CONTROLLER controller = NV_THERMAL_CONTROLLER::GPU_INTERNAL)
{
	for (NV_U32 i = 0; i < status->count; i++) {
		const auto &status_entry = status->entries[i];
		const auto &info_entry = info->entries[i];
		if (static_cast<NV_THERMAL_CONTROLLER>(status_entry.controller) == controller) {
			// NOTE(dweiler): unlike power limits, thermal policy values are multiples of 256
			return {
				{
					info_entry.min / 256.0f,
					status_entry.value / 256.0f,
					info_entry.max / 256.0f,
					info_entry.max > 0
				},
				{
					static_cast<bool>(info_entry.default_flags & 1),
					static_cast<bool>(status_entry.flags & 1)
				}
			};
		}
	}

	return {};
}

NV_U32 GetBestPStateIndex(const NV_GPU_PSTATES20_V2 *pstates)
{
	NV_U32 best_pstate_index = 0;
	NV_U32 best_pstate_state = UINT32_MAX;
	for (NV_U32 i = 0; i < pstates->state_count; i++) {
		if (pstates->states[i].state_num < best_pstate_state) {
			best_pstate_index = i;
			best_pstate_state = pstates->states[i].state_num;
		}
	}
	return best_pstate_index;
}

std::optional<float> GetUsageForSystem(NV_DYNAMIC_PSTATES_SYSTEM system, const NV_DYNAMIC_PSTATES_V1 *const pstates)
{
	const auto state = pstates->pstates[static_cast<size_t>(system)];
	if (state.present) {
		return static_cast<float>(state.value);
	}
	return std::nullopt;
}

GPU::GPU(NV_S32 adapter_index, NV_PHYSICAL_GPU_HANDLE physical_gpu_handle, NV_DISPLAY_HANDLE display_handle)
	: m_adapter_index       { adapter_index }
	, m_physical_gpu_handle { physical_gpu_handle }
	, m_display_handle      { display_handle }
{
	// Extracting the name is straight forward
	NV_SHORT_STRING name;
	if (NvAPI_GPU_GetFullName(m_physical_gpu_handle, name) == 0) {
		m_name = name;
	}
	
	// Extracting the serial takes some work because it's binary encoded in the string as unprintable character
	// here we have to reinterpret it as hex and terminate processing once we hit a zero byte
	NV_SHORT_STRING serial_number;
	if (NvAPI_GPU_GetSerialNumber(m_physical_gpu_handle, serial_number) == 0) {
		std::stringstream ss;
		ss << std::hex << std::setfill('0') << std::uppercase;
		for (auto ch : serial_number) {
			auto byte = static_cast<uint8_t>(ch);
			if (byte != 0) {
				ss << std::setw(2) << static_cast<uint16_t>(byte);
			} else {
				break;
			}
		}
		m_serial_number = ss.str();
	}

	// Extract PCI identifiers
	PCIIdentifiers pci_identifiers;
	if (NvAPI_GPU_GetPCIIdentifiers(m_physical_gpu_handle, &pci_identifiers[0], &pci_identifiers[1], &pci_identifiers[2], &pci_identifiers[3]) == 0) {
		m_pci_identifiers = std::move(pci_identifiers);
	}
}

std::optional<float> GPU::GetVoltage() const
{
	if (m_data_set) {
		for (NV_U32 i = 0; i < m_data_set->m_voltage_domain_status.count; i++) {
			// TODO(dweiler): figure out what the other voltage domains are for
			if (m_data_set->m_voltage_domain_status.entries[i].voltage_domain == 0) {
				return this->m_data_set->m_voltage_domain_status.entries[i].current_voltage / 1'000'000.0f;
			}
		}
	}
	return std::nullopt;
}

std::optional<float> GPU::GetTemperature(NV_THERMAL_TARGET target) const
{
	if (m_data_set) {
		for (NV_U32 i = 0; i < m_data_set->m_thermal_settings.count; i++) {
			if (m_data_set->m_thermal_settings.sensor[i].target == target) {
				return static_cast<float>(m_data_set->m_thermal_settings.sensor[i].current_temperature);
			}
		}
	}
	return std::nullopt;
}

std::optional<GPU::Clocks> GPU::GetCurrentClocks() const
{
	return GetClocks(NV_CLOCK_FREQUENCY_TYPE::CURRENT);
}

std::optional<GPU::Clocks> GPU::GetDefaultClocks() const
{
	return GetClocks(NV_CLOCK_FREQUENCY_TYPE::BASE);
}

std::optional<GPU::Clocks> GPU::GetBaseClocks() const
{
	return GetClocks(NV_CLOCK_FREQUENCY_TYPE::BASE, true);
}

std::optional<GPU::Clocks> GPU::GetBoostClocks() const
{
	return GetClocks(NV_CLOCK_FREQUENCY_TYPE::BOOST, true);
}

std::optional<GPU::Clocks> GPU::GetClocks(NV_CLOCK_FREQUENCY_TYPE type, bool compenate_for_over_clock) const
{
	const auto &data_source = m_data_set->m_frequencies[static_cast<int>(type)];

	auto fetch = [&](NV_CLOCK_SYSTEM clock_system) -> std::optional<float>
	{
		const auto index = static_cast<size_t>(clock_system);
		if (data_source.entries[index].present) {
			float compensation = 0.0f;
			if (compenate_for_over_clock) {
				// TODO(dweiler): implement
			}
			return data_source.entries[index].frequency / 1000.0f + compensation;
		}
		return std::nullopt;
	};

	return Clocks {
		fetch(NV_CLOCK_SYSTEM::GPU),
		fetch(NV_CLOCK_SYSTEM::MEMORY),
		fetch(NV_CLOCK_SYSTEM::SHADER)
	};
}

std::optional<GPU::Usage> GPU::GetUsage() const
{
	if (!m_data_set) {
		return std::nullopt;
	}

	return Usage {
		GetUsageForSystem(NV_DYNAMIC_PSTATES_SYSTEM::GPU, &m_data_set->m_dynamic_pstates),
		GetUsageForSystem(NV_DYNAMIC_PSTATES_SYSTEM::FB, &m_data_set->m_dynamic_pstates),
		GetUsageForSystem(NV_DYNAMIC_PSTATES_SYSTEM::VID, &m_data_set->m_dynamic_pstates),
		GetUsageForSystem(NV_DYNAMIC_PSTATES_SYSTEM::BUS, &m_data_set->m_dynamic_pstates)
	};
}

std::optional<GPU::Memory> GPU::GetMemory() const
{
	if (!m_data_set) {
		return std::nullopt;
	}

	const auto total_memory = m_data_set->m_memory_info.values[0];
	const auto free_memory = m_data_set->m_memory_info.values[4];
	const float used_memory = static_cast<float>(std::max(total_memory - free_memory, static_cast<NV_U32>(0)));

	return Memory {
		static_cast<float>(total_memory) / 1024.0f,
		static_cast<float>(free_memory) / 1024.0f,
		used_memory / 1024.0f
	};
}

bool GPU::SetDefaultFanSpeed()
{
	bool result = true;
	NV_GPU_COOLER_LEVELS_V1 cooler_levels = {};
	for (NV_U32 i = 0; i < m_data_set->m_cooler_settings.count; i++) {
		cooler_levels.levels[i].policy = 0x20;
		result &= NvAPI_GPU_SetCoolerLevels(m_physical_gpu_handle, i, &cooler_levels) == 0;
	}
	return result;
}

bool GPU::SetCustomFanSpeed(NV_U32 value)
{
	bool result = true;
	NV_GPU_COOLER_LEVELS_V1 cooler_levels = {};
	for (NV_U32 i = 0; i < m_data_set->m_cooler_settings.count; i++) {
		cooler_levels.levels[i].policy = 0x01;
		cooler_levels.levels[i].level = value;
		result &= NvAPI_GPU_SetCoolerLevels(m_physical_gpu_handle, i, &cooler_levels) == 0;
	}
	return result;
}

bool GPU::Update()
{
	std::unique_ptr<DataSet> data_set(new DataSet);

	bool frequency_status = true;
	const auto current = static_cast<std::size_t>(NV_CLOCK_FREQUENCY_TYPE::CURRENT);
	const auto base = static_cast<std::size_t>(NV_CLOCK_FREQUENCY_TYPE::BASE);
	const auto boost = static_cast<std::size_t>(NV_CLOCK_FREQUENCY_TYPE::BOOST);
	frequency_status &= LoadClockFrequencies(m_physical_gpu_handle, &data_set->m_frequencies[current], NV_CLOCK_FREQUENCY_TYPE::CURRENT);
	frequency_status &= LoadClockFrequencies(m_physical_gpu_handle, &data_set->m_frequencies[base], NV_CLOCK_FREQUENCY_TYPE::BASE);
	frequency_status &= LoadClockFrequencies(m_physical_gpu_handle, &data_set->m_frequencies[boost], NV_CLOCK_FREQUENCY_TYPE::BOOST);

	// When we were able to load all frequency values load the remainder of the data set
	if (frequency_status) {
		bool status = true;
		status &= LoadGPUDynamicPStates(m_physical_gpu_handle, &data_set->m_dynamic_pstates);
		status &= LoadGPUPStates20V2(m_physical_gpu_handle, &data_set->m_pstates20);
		status &= LoadGPUPowerPoliciesInfo(m_physical_gpu_handle, &data_set->m_power_policies_info);

		// TODO(dweiler): how come this interface can be called by ASUS Precision XOC without crashing?
		// we really do need this interface, it used to work too, need to figure out what nvapi is doing
		// to prevent us from calling it, we can't monitor OC without it...
		//status &= LoadGPUPowerPoliciesStatus(m_physical_gpu_handle, &data_set->m_power_policies_status);
		status &= LoadGPUVoltageDomainsStatus(m_physical_gpu_handle, &data_set->m_voltage_domain_status);
		status &= LoadGPUThermalSettingsV2(m_physical_gpu_handle, &data_set->m_thermal_settings);
		status &= LoadGPUThermalPoliciesInfoV2(m_physical_gpu_handle, &data_set->m_thermal_policies_info);
		status &= LoadGPUThermalPoliciesStatusV2(m_physical_gpu_handle, &data_set->m_thermal_policies_status);
		status &= LoadGPUCoolerSettingsV2(m_physical_gpu_handle, 0, &data_set->m_cooler_settings);

		status &= NvAPI_GetMemoryInfo(m_display_handle, &data_set->m_memory_info) == 0;

		if (status) {
			m_data_set = std::move(data_set);
			return true;
		}
	}

	return false;
}