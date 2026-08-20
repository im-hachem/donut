workspace "Donut"
	architecture "x64"
	startproject "Donut"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	flags
	{
		"MultiProcessorCompile"
	}

	filter "system:windows"
		defines "DONUT_WINDOWS"
	filter "system:linux"
		defines "DONUT_LINUX"
	filter "system:macosx"
		defines "DONUT_MACOS"
		-- Build natively for Apple Silicon (was x86_64 under Rosetta). Native
		-- arm64 is required for clean Metal work and better performance.
		architecture "ARM64"
	filter {}

outputdir = "%{cfg.buildcfg}-%{cfg.system}/%{prj.name}"

IncludeDir = {}
IncludeDir["glm"]            = "ext/glm"
IncludeDir["glfw"]           = "ext/glfw/include"
IncludeDir["glad"]           = "ext/glad/include"
IncludeDir["imgui"]          = "ext/imgui"
IncludeDir["imgui_backends"] = "ext/imgui/backends"
IncludeDir["imguizmo"]       = "ext/ImGuizmo"
IncludeDir["toml11"]         = "ext/toml11/include"
IncludeDir["nlohmann"]       = "ext/json/single_include"
IncludeDir["stb"]            = "ext/stb"

group "Dependencies"

project "GLAD"
    kind "StaticLib"
    language "C"
    staticruntime "on"
    targetdir ("bin/" .. outputdir)
    objdir ("bin-int/" .. outputdir)

    files
    {
        "ext/glad/include/glad/glad.h",
        "ext/glad/include/KHR/khrplatform.h",

        "ext/glad/src/glad.c"
    }

    includedirs
    {
        "ext/glad/include"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

project "GLFW"
	kind "StaticLib"
	language "C"
	staticruntime "on"
	warnings "off"

	targetdir ("bin/" .. outputdir)
	objdir ("bin-int/" .. outputdir)

	files
	{
		"ext/glfw/src/context.c",
		"ext/glfw/src/init.c",
		"ext/glfw/src/input.c",
		"ext/glfw/src/monitor.c",
		"ext/glfw/src/platform.c",
		"ext/glfw/src/vulkan.c",
		"ext/glfw/src/window.c",

		"ext/glfw/src/internal.h",
		"ext/glfw/src/platform.h",
		"ext/glfw/src/mappings.h",

		"ext/glfw/src/null_init.c",
		"ext/glfw/src/null_joystick.c",
		"ext/glfw/src/null_joystick.h",
		"ext/glfw/src/null_monitor.c",
		"ext/glfw/src/null_platform.h",
		"ext/glfw/src/null_window.c",
	}

	filter "system:linux"
		pic "On"
		systemversion "latest"

		files
		{
			"ext/glfw/src/x11_init.c",
			"ext/glfw/src/x11_monitor.c",
			"ext/glfw/src/x11_platform.h",
			"ext/glfw/src/x11_window.c",
			"ext/glfw/src/xkb_unicode.c",
			"ext/glfw/src/xkb_unicode.h",

			"ext/glfw/src/wl_init.c",
			"ext/glfw/src/wl_monitor.c",
			"ext/glfw/src/wl_platform.h",
			"ext/glfw/src/wl_window.c",

			"ext/glfw/src/posix_module.c",
			"ext/glfw/src/posix_time.c",
			"ext/glfw/src/posix_time.h",
			"ext/glfw/src/posix_thread.c",
			"ext/glfw/src/posix_thread.h",

			"ext/glfw/src/glx_context.c",
			"ext/glfw/src/egl_context.c",
			"ext/glfw/src/osmesa_context.c",

			"ext/glfw/src/linux_joystick.c",
			"ext/glfw/src/linux_joystick.h"
		}

		defines
		{
			"_GLFW_X11"
		}

	filter "system:macosx"
		pic "On"

		files
		{
			"ext/glfw/src/cocoa_init.m",
			"ext/glfw/src/cocoa_joystick.h",
			"ext/glfw/src/cocoa_joystick.m",
			"ext/glfw/src/cocoa_monitor.m",
			"ext/glfw/src/cocoa_platform.h",
			"ext/glfw/src/cocoa_time.c",
			"ext/glfw/src/cocoa_time.h",
			"ext/glfw/src/cocoa_window.m",

			"ext/glfw/src/nsgl_context.m",
			"ext/glfw/src/egl_context.c",
			"ext/glfw/src/osmesa_context.c",

			"ext/glfw/src/posix_module.c",
			"ext/glfw/src/posix_thread.c",
			"ext/glfw/src/posix_thread.h"
		}

		defines
		{
			"_GLFW_COCOA"
		}

	filter "system:windows"
		systemversion "latest"

		files
		{
			"ext/glfw/src/win32_init.c",
			"ext/glfw/src/win32_joystick.c",
			"ext/glfw/src/win32_joystick.h",
			"ext/glfw/src/win32_module.c",
			"ext/glfw/src/win32_monitor.c",
			"ext/glfw/src/win32_platform.h",
			"ext/glfw/src/win32_thread.c",
			"ext/glfw/src/win32_thread.h",
			"ext/glfw/src/win32_time.c",
			"ext/glfw/src/win32_time.h",
			"ext/glfw/src/win32_window.c",
			"ext/glfw/src/wgl_context.c",
			"ext/glfw/src/egl_context.c",
			"ext/glfw/src/osmesa_context.c"
		}

		defines
		{
			"_GLFW_WIN32",
			"_CRT_SECURE_NO_WARNINGS",
		}

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter { "system:windows", "configurations:Debug-AS" }
		runtime "Debug"
		symbols "on"
		sanitize { "Address" }
		flags { "NoRuntimeChecks", "NoIncrementalLink" }

	filter "configurations:Release"
		runtime "Release"
		optimize "speed"

    filter "configurations:Dist"
		runtime "Release"
		optimize "speed"
        symbols "off"

project "ImGui"
	kind "StaticLib"
	language "C++"
	staticruntime "on"

	targetdir ("bin/" .. outputdir)
	objdir ("bin-int/" .. outputdir)

	files
	{
		"ext/imgui/imgui.cpp",
		"ext/imgui/imgui_draw.cpp",
		"ext/imgui/imgui_tables.cpp",
		"ext/imgui/imgui_widgets.cpp",
		"ext/imgui/imgui_demo.cpp"
	}

	filter "system:windows"
		systemversion "latest"
		cppdialect "C++17"

	filter "system:linux"
		pic "On"
		systemversion "latest"
		cppdialect "C++17"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		runtime "Release"
		optimize "on"
		symbols "off"

project "ImGuizmo"
	kind "StaticLib"
	language "C++"
	staticruntime "on"

	targetdir ("bin/" .. outputdir)
	objdir ("bin-int/" .. outputdir)

	files
	{
		"ext/ImGuizmo/ImGuizmo.cpp",
		"ext/ImGuizmo/ImGuizmo.h"
	}

	includedirs
	{
		"%{IncludeDir.imgui}",
		"%{IncludeDir.imguizmo}"
	}

	filter "system:windows"
		systemversion "latest"
		cppdialect "C++17"

	filter "system:linux"
		pic "On"
		systemversion "latest"
		cppdialect "C++17"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		runtime "Release"
		optimize "on"
		symbols "off"

group ""

project "Donut"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"

	targetdir ("bin/" .. outputdir)
	objdir ("bin-int/" .. outputdir)

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_NONE"
	}

	files
	{
		"ext/glm/glm/**.hpp",
		"ext/glm/glm/**.inl",

		"src/**.h",
		"src/**.cpp",
		"src/rendering/framebuffer.h",
		"src/rendering/framebuffer.cpp",
		"src/platform/opengl/opengl_framebuffer.h",
		"src/platform/opengl/opengl_framebuffer.cpp",

		"ext/imgui/backends/imgui_impl_glfw.cpp",
		"ext/imgui/backends/imgui_impl_opengl3.cpp",

		"assets/fonts/inter/static/Inter_18pt-Regular.ttf",
		"assets/fonts/inter/static/Inter_18pt-Bold.ttf",
		"assets/fonts/inter/static/Inter_18pt-Light.ttf"
	}

	includedirs
	{
		"src",

		"%{IncludeDir.glm}",
		"%{IncludeDir.glfw}",
		"%{IncludeDir.glad}",
		"%{IncludeDir.imgui}",
		"%{IncludeDir.imgui_backends}",
		"%{IncludeDir.imguizmo}",
		"%{IncludeDir.toml11}",
		"%{IncludeDir.nlohmann}",
		"%{IncludeDir.stb}",
	}

    links
    {
        "GLFW",
        "GLAD",
        "ImGui",
        "ImGuizmo"
    }

	filter "system:windows"
		systemversion "latest"

		links
		{
			"opengl32.lib",
		}

	filter "system:macosx"
		-- Objective-C++ (Metal) sources + the ImGui Vulkan backend compile only
		-- on macOS (Vulkan headers come from Homebrew there).
		files
		{
			"src/**.mm",
			"ext/imgui/backends/imgui_impl_vulkan.cpp"
		}

		-- Compile the Slang shaders to assets/shaders/generated/ before building.
		-- Requires tools/slang (tools/fetch-slang.sh) and spirv-cross on PATH.
		prebuildcommands
		{
			'bash "%{wks.location}/tools/compile-shaders.sh"'
		}

		-- Vulkan (via MoltenVK) from Homebrew; see tools/vulkan-env.sh for runtime.
		includedirs { "/opt/homebrew/include" }
		libdirs     { "/opt/homebrew/lib" }

		links
		{
			"Cocoa.framework",
			"IOKit.framework",
			"CoreFoundation.framework",
			"QuartzCore.framework",
			"Metal.framework",
			"Foundation.framework",
			"vulkan"
		}

	filter "configurations:Debug"
		defines "DONUT_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "DONUT_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "DONUT_DIST"
		runtime "Release"
		optimize "on"
