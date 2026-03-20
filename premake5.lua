workspace "fps"
	architecture "x86_64"
	startproject "fps"

	configurations {
		"Debug",
		"Release",
		"Dist"
	}

project "fps"
    kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"
    multiprocessorcompile "on"

    targetdir ("%{wks.location}/build/%{cfg.buildcfg}")
	objdir ("%{wks.location}/build-int/%{cfg.buildcfg}/%{prj.name}")

	pchheader "pch.h"
	pchsource "src/pch.cpp"

    includedirs {
        "external/include",
        "%{wks.location}/tracy",
    }

    libdirs {
        "external/lib"
    }

    files {
        "src/**.h",
        "src/**.cpp",
        "external/src/imgui/**.cpp",
        "%{wks.location}/tracy/TracyClient.cpp"
    }

    defines {
		"_CRT_SECURE_NO_WARNINGS",
        "ENABLE_IMGUI",
	}

    filter "files:external/src/imgui/**.cpp"
	    enablepch "off"

    filter "files:tracy/TracyClient.cpp"
	    enablepch "off"

    filter "system:windows"
		systemversion "latest"
        buildoptions { "/Zc:strictStrings-" }

        defines { "RENDER_D3D11" }

        links {
            "winmm.lib",
            "vulkan-1.lib",
            "SDL2.lib",
            "SDL2main.lib",
            "EASTL.lib",
            "freetype.lib",
        }

    filter "configurations:Debug"
		defines { "BUILD_DEBUG", "TRACY_ENABLE" }
		runtime "Debug"
		symbols "on"

        libdirs { "external/lib/Debug" }
        
	filter "configurations:Release"
		defines { "BUILD_RELEASE", "TRACY_ENABLE" }
		runtime "Release"
		optimize "on"

        libdirs { "external/lib/Release" }

	filter "configurations:Dist"
		defines "BUILD_DIST"
		runtime "Release"
		optimize "on"

        libdirs { "external/lib/Release" }
