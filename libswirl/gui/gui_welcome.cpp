/*
	This file is part of libswirl
*/
#include "license/bsd"


#include "types.h"
#include "version.h"
#include "gui.h"
#include "imgui/imgui.h"
#include "gui_partials.h"
#include "gui_util.h"
#include "rend/gles/imgui_impl_opengl3.h"
#include "cfg/cfg.h"
#include "oslib/oslib.h"

float CalcScreenScale(const char* headline) {
	auto size = ImGui::CalcTextSize(headline);

	float scale = 0;

	scale = screen_width/(size.x + 10);

	if (size.y * scale / screen_height > 0.4) {
		scale = (screen_height * 0.4) / size.y;
	}
	return scale;
}

void DrawTextCentered(const char* text) {
	auto size = ImGui::CalcTextSize(text);
	ImGui::SetCursorPosX(screen_width/2 - size.x / 2);
	ImGui::Text("%s", text);
}

void gui_welcome(ImFont* font64) {

	static auto endTime = os_GetSeconds() + 3;

	if (os_GetSeconds() > endTime) {
		gui_state = Main;
		return;
	}

	ImGui_Impl_NewFrame();
	ImGui::NewFrame();

	ImGui_ImplOpenGL3_DrawBackground();

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(screen_width, screen_height));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

	ImGui::PushFont(font64);
	ImGui::Begin("Welcome", NULL, /*ImGuiWindowFlags_AlwaysAutoResize |*/ ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
	
	
	ImGui::SetWindowFontScale(1);
	float scale = CalcScreenScale("Reicast");

	ImGui::SetWindowFontScale(scale);

	DrawTextCentered("Reicast");
	
		string ver = REICAST_VERSION;
	auto ver_numeric = ver.substr(0, ver.find_last_of("-"));
	auto ver_hash = "(" + ver.substr(ver.find_last_of("-") + 2) + ")";

	ImGui::SetWindowFontScale(scale / 3.5);
	DrawTextCentered(ver_numeric.c_str());
	
	ImGui::SetWindowFontScale(scale / 6);
	DrawTextCentered(ver_hash.c_str());

	ImGui::End();
	ImGui::PopFont();
	ImGui::PopStyleVar();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData(), false);
}