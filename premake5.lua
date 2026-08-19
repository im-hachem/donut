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
IncludeDir["glm"]            = "Vendor/glm"
IncludeDir["glfw"]           = "Vendor/glfw/include"
IncludeDir["glad"]           = "Vendor/glad/include"
IncludeDir["imgui"]          = "Vendor/imgui"
IncludeDir["imgui_backends"] = "Vendor/imgui/backends"
IncludeDir["imguizmo"]       = "Vendor/ImGuizmo"
IncludeDir["toml11"]         = "Vendor/toml11/include"
IncludeDir["nlohmann"]       = "Vendor/json/single_include"
IncludeDir["stb"]            = "Vendor/stb"

group "Dependencies"

project "GLAD"
    kind "StaticLib"
    language "C"
    staticruntime "on"
    targetdir ("bin/" .. outputdir)
    objdir ("bin-int/" .. outputdir)

    files
    {
        "Vendor/glad/include/glad/glad.h",
        "Vendor/glad/include/KHR/khrplatform.h",

        "Vendor/glad/src/glad.c"
    }

    includedirs
    {
        "Vendor/glad/include"
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
		"Vendor/glfw/src/context.c",
		"Vendor/glfw/src/init.c",
		"Vendor/glfw/src/input.c",
		"Vendor/glfw/src/monitor.c",
		"Vendor/glfw/src/platform.c",
		"Vendor/glfw/src/vulkan.c",
		"Vendor/glfw/src/window.c",

		"Vendor/glfw/src/internal.h",
		"Vendor/glfw/src/platform.h",
		"Vendor/glfw/src/mappings.h",

		"Vendor/glfw/src/null_init.c",
		"Vendor/glfw/src/null_joystick.c",
		"Vendor/glfw/src/null_joystick.h",
		"Vendor/glfw/src/null_monitor.c",
		"Vendor/glfw/src/null_platform.h",
		"Vendor/glfw/src/null_window.c",
	}

	filter "system:linux"
		pic "On"
		systemversion "latest"

		files
		{
			"Vendor/glfw/src/x11_init.c",
			"Vendor/glfw/src/x11_monitor.c",
			"Vendor/glfw/src/x11_platform.h",
			"Vendor/glfw/src/x11_window.c",
			"Vendor/glfw/src/xkb_unicode.c",
			"Vendor/glfw/src/xkb_unicode.h",

			"Vendor/glfw/src/wl_init.c",
			"Vendor/glfw/src/wl_monitor.c",
			"Vendor/glfw/src/wl_platform.h",
			"Vendor/glfw/src/wl_window.c",

			"Vendor/glfw/src/posix_module.c",
			"Vendor/glfw/src/posix_time.c",
			"Vendor/glfw/src/posix_time.h",
			"Vendor/glfw/src/posix_thread.c",
			"Vendor/glfw/src/posix_thread.h",

			"Vendor/glfw/src/glx_context.c",
			"Vendor/glfw/src/egl_context.c",
			"Vendor/glfw/src/osmesa_context.c",

			"Vendor/glfw/src/linux_joystick.c",
			"Vendor/glfw/src/linux_joystick.h"
		}

		defines
		{
			"_GLFW_X11"
		}

	filter "system:macosx"
		pic "On"

		files
		{
			"Vendor/glfw/src/cocoa_init.m",
			"Vendor/glfw/src/cocoa_joystick.h",
			"Vendor/glfw/src/cocoa_joystick.m",
			"Vendor/glfw/src/cocoa_monitor.m",
			"Vendor/glfw/src/cocoa_platform.h",
			"Vendor/glfw/src/cocoa_time.c",
			"Vendor/glfw/src/cocoa_time.h",
			"Vendor/glfw/src/cocoa_window.m",

			"Vendor/glfw/src/nsgl_context.m",
			"Vendor/glfw/src/egl_context.c",
			"Vendor/glfw/src/osmesa_context.c",

			"Vendor/glfw/src/posix_module.c",
			"Vendor/glfw/src/posix_thread.c",
			"Vendor/glfw/src/posix_thread.h"
		}

		defines
		{
			"_GLFW_COCOA"
		}

	filter "system:windows"
		systemversion "latest"

		files
		{
			"Vendor/glfw/src/win32_init.c",
			"Vendor/glfw/src/win32_joystick.c",
			"Vendor/glfw/src/win32_joystick.h",
			"Vendor/glfw/src/win32_module.c",
			"Vendor/glfw/src/win32_monitor.c",
			"Vendor/glfw/src/win32_platform.h",
			"Vendor/glfw/src/win32_thread.c",
			"Vendor/glfw/src/win32_thread.h",
			"Vendor/glfw/src/win32_time.c",
			"Vendor/glfw/src/win32_time.h",
			"Vendor/glfw/src/win32_window.c",
			"Vendor/glfw/src/wgl_context.c",
			"Vendor/glfw/src/egl_context.c",
			"Vendor/glfw/src/osmesa_context.c"
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
		"Vendor/imgui/imgui.cpp",
		"Vendor/imgui/imgui_draw.cpp",
		"Vendor/imgui/imgui_tables.cpp",
		"Vendor/imgui/imgui_widgets.cpp",
		"Vendor/imgui/imgui_demo.cpp"
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
		"Vendor/ImGuizmo/ImGuizmo.cpp",
		"Vendor/ImGuizmo/ImGuizmo.h"
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
		"Vendor/glm/glm/**.hpp",
		"Vendor/glm/glm/**.inl",

		"src/**.h",
		"src/**.cpp",
		"src/Rendering/Framebuffer.h",
		"src/Rendering/Framebuffer.cpp",
		"src/Platform/OpenGL/OpenGLFramebuffer.h",
		"src/Platform/OpenGL/OpenGLFramebuffer.cpp",

		"Vendor/imgui/backends/imgui_impl_glfw.cpp",
		"Vendor/imgui/backends/imgui_impl_opengl3.cpp",

		"Assets/Fonts/Inter/static/Inter_18pt-Regular.ttf",
		"Assets/Fonts/Inter/static/Inter_18pt-Bold.ttf",
		"Assets/Fonts/Inter/static/Inter_18pt-Light.ttf"
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
			"Vendor/imgui/backends/imgui_impl_vulkan.cpp"
		}

		-- Compile the Slang shaders to Assets/Shaders/generated/ before building.
		-- Requires Tools/slang (Tools/fetch-slang.sh) and spirv-cross on PATH.
		prebuildcommands
		{
			'bash "%{wks.location}/Tools/compile-shaders.sh"'
		}

		-- Vulkan (via MoltenVK) from Homebrew; see Tools/vulkan-env.sh for runtime.
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
