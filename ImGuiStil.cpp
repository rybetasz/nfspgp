#include <windows.h>
#include <d3d9.h>
#include "Patches.h"
#include "Console.h"
#include "imgui-1.92.5/imgui.h"
#include "imgui-1.92.5/backends/imgui_impl_dx9.h"
#include "imgui-1.92.5/backends/imgui_impl_win32.h"
void SetupImGuiStyle(){

	// Comfortable Dark Cyan style by SouthCraftX from ImThemes

	ImGuiStyle& style = ImGui::GetStyle();


	style.Alpha = 1.0f;

	style.DisabledAlpha = 1.0f;

	style.WindowPadding = ImVec2(20.0f, 20.0f);

	style.WindowRounding = 11.5f;

	style.WindowBorderSize = 0.0f;

	style.WindowMinSize = ImVec2(20.0f, 20.0f);

	style.WindowTitleAlign = ImVec2(0.5f, 0.5f);

	style.WindowMenuButtonPosition = ImGuiDir_None;

	style.ChildRounding = 20.0f;

	style.ChildBorderSize = 1.0f;

	style.PopupRounding = 17.4f;

	style.PopupBorderSize = 1.0f;

	style.FramePadding = ImVec2(20.0f, 3.4f);

	style.FrameRounding = 11.9f;

	style.FrameBorderSize = 0.0f;

	style.ItemSpacing = ImVec2(8.9f, 13.4f);

	style.ItemInnerSpacing = ImVec2(7.1f, 1.8f);

	style.CellPadding = ImVec2(12.1f, 9.2f);

	style.IndentSpacing = 0.0f;

	style.ColumnsMinSpacing = 8.7f;

	style.ScrollbarSize = 11.6f;

	style.ScrollbarRounding = 15.9f;

	style.GrabMinSize = 3.7f;

	style.GrabRounding = 20.0f;

	style.TabRounding = 9.8f;

	style.TabBorderSize = 0.0f;

	style.ColorButtonPosition = ImGuiDir_Right;

	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);

	style.SelectableTextAlign = ImVec2(0.0f, 0.0f);


	style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.27450982f, 0.31764707f, 0.4509804f, 1.0f);

	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);

	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.09411765f, 0.101960786f, 0.11764706f, 1.0f);

	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);

	style.Colors[ImGuiCol_Border] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);

	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);

	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.11372549f, 0.1254902f, 0.15294118f, 1.0f);

	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);

	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);

	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);

	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);

	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);

	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.09803922f, 0.105882354f, 0.12156863f, 1.0f);

	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);

	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);

	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);

	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);

	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.03137255f, 0.9490196f, 0.84313726f, 1.0f);

	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.03137255f, 0.9490196f, 0.84313726f, 1.0f);

	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.6f, 0.9647059f, 0.03137255f, 1.0f);

	style.Colors[ImGuiCol_Button] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);

	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.18039216f, 0.1882353f, 0.19607843f, 1.0f);

	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.15294118f, 0.15294118f, 0.15294118f, 1.0f);

	style.Colors[ImGuiCol_Header] = ImVec4(0.14117648f, 0.16470589f, 0.20784314f, 1.0f);

	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.105882354f, 0.105882354f, 0.105882354f, 1.0f);

	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
}