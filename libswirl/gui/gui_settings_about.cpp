#include "types.h"
#include "gui.h"
#include "stdclass.h"
#include "imgui/imgui.h"
#include "gui_partials.h"
#include "gui_util.h"
#include "version.h"
#include "glwrap/GLES.h"

void gui_settings_about()
{
	if (ImGui::BeginTabItem("About"))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
	    if (ImGui::CollapsingHeader("Reicast", ImGuiTreeNodeFlags_DefaultOpen))
	    {
			ImGui::Text("Version: %s", REICAST_VERSION);
			ImGui::Text("Git Hash: %s", GIT_HASH);
			ImGui::Text("Build Date: %s", BUILD_DATE);
			ImGui::Text("Target: %s",
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
					"Dreamcast"
#elif DC_PLATFORM == DC_PLATFORM_NAOMI
					"Naomi"
#elif DC_PLATFORM == DC_PLATFORM_ATOMISWAVE
					"Atomiswave"
#else
					"Unknown"
#endif
					);
	    }
	    if (ImGui::CollapsingHeader("Platform", ImGuiTreeNodeFlags_DefaultOpen))
	    {
	    	ImGui::Text("CPU: %s",
#if HOST_CPU == CPU_X86
				"x86"
#elif HOST_CPU == CPU_ARM
				"ARM"
#elif HOST_CPU == CPU_MIPS
				"MIPS"
#elif HOST_CPU == CPU_X64
				"x86/64"
#elif HOST_CPU == CPU_GENERIC
				"Generic"
#elif HOST_CPU == CPU_ARM64
				"ARM64"
#else
				"Unknown"
#endif
					);
	    	ImGui::Text("Operating System: %s",
#ifdef _ANDROID
				"Android"
#elif HOST_OS == OS_LINUX
				"Linux"
#elif HOST_OS == OS_DARWIN
#if TARGET_IPHONE
	    		"iOS"
#else
				"OSX"
#endif
#elif HOST_OS == OS_WINDOWS
				"Windows"
#else
				"Unknown"
#endif
					);
	    }
	    if (ImGui::CollapsingHeader("Open GL", ImGuiTreeNodeFlags_DefaultOpen))
	    {
			ImGui::Text("Renderer: %s", (const char *)glGetString(GL_RENDERER));
			ImGui::Text("Version: %s", (const char *)glGetString(GL_VERSION));
	    }
#ifdef _ANDROID
	    ImGui::Separator();
	    if (ImGui::Button("Send Logs")) {
	    	void android_send_logs();
	    	android_send_logs();
	    }
#endif
		ImGui::PopStyleVar();
		ImGui::EndTabItem();
	}
}