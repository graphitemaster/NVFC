# NVFC


Open source tool for monitoring, configuring and overclocking NVIDIA GPUs

# Reverse engineering
All of the useful interfaces for controlling power states and fans with NvAPI is undocumented. This tool implements interfaces that were discovered by carefully watching functions obtained through `nvapi_QueryInterface` as called by existing programs like EVGA Precision X.


# Why
The result of this effort is that there is now a sufficent base to build an open source tool that can monitor, configure and overclock NVIDIA GPUs that is lightweight and isn't branded like a tacky gamer product.

Similarly, tools like EVGA Precision X and RivaTuner to name a few, often rely on injection techniques to provide functionality that most people don't need, such as an overlay. These injection techniques can often trigger anti-tamper and DRM solutions in video games, preventing them from ever running. NVFC doesn't have this problem.

Since the general interface is written like a library, this can also be used in video games to modify and tweak the GPU.


# Features
Currently the following features are supported

 * Getting display driver version and branch info
 * Enumerating all NVIDIA GPUs
 * Getting names
 * Getting serial numbers
 * Getting PCIe identifiers
 * Getting voltage
 * Getting temperatures for: gpu, memory, power supply and board
 * Getting current clocks for: core, memory and shader
 * Getting base clocks for: core, memory and shader
 * Getting default clocks for: core, memory and shader
 * Getting boost clocks for: core, memory and shader
 * Getting usages for: gpu, framebuffer, video engine and memory bus
 * Getting current overclock profile
 * Getting current fan usage and policy
 * Setting fan usage and policy
 

Currently still reverse engineering how to set overclock profiles and over volting

# System Requirements
 * NVIDIA display driver 384.76 or higher
 * Windows 7, 8 or 10
 * NVIDIA 400 Series or higher

# How To Build
Written in C++17 so Visual Studio 2017 solution file included.