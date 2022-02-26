/*
	This file is part of libswirl
        original code by flyinghead
*/
#include "license/bsd"

#include "utils/http.h"



#include "gui.h"

#include <algorithm>
#include <math.h>
#ifdef _MSC_VER
#include "dirent/dirent.h"
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#else
#include <dirent.h>
#endif
#include <sys/stat.h>
#include <fstream>
#include "gui.h"
#include "oslib/oslib.h"
#include "cfg/cfg.h"

#include "imgui/imgui.h"
#include "rend/gles/imgui_impl_opengl3.h"
#include "imgui/roboto_medium.h"
#include "rend/gles/gles.h"
#include "linux-dist/main.h"	// FIXME for kcode[]
#include "gui_util.h"
#include "gui_android.h"

#include "version.h"
#include "oslib/audiostream.h"
#include "hw/pvr/Renderer_if.h"
#include "libswirl.h"
#include "utils/cloudrom.h"

#include "gui_partials.h"
#include "input/keyboard_device.h"
#include "hw/maple/maple_if.h"

#include "libswirl.h"
#include "gui/gui_renderer.h"

bool game_started;

int screen_dpi = 96;

static bool inited = false;
float scaling = 1;

GuiState gui_state = Welcome;

bool settings_opening;
#ifdef _ANDROID
static bool touch_up;
#endif

extern bool subfolders_read;

static std::string file_toRename = ""; //used for rename function
static std::string path_toRename = ""; //same
static std::string currentDirectory = ""; //this stores the current file open
std::vector<std::string> dirs; //store of all the paths that contain a relative file (inside the selected content path(s))
std::vector<std::string> content_paths; //store of the content path(s)


static double last_render;
std::vector<float> render_times;

bool game_list_done;		// Set to false to refresh the game list
bool maple_devices_changed;

ImVec2 normal_padding;
int dynarec_enabled;

struct GameMedia {
    std::string name;
    std::string path;
};

static bool operator<(const GameMedia& left, const GameMedia& right)
{
    return left.name < right.name;
}

static std::vector<GameMedia> game_list;

static unique_ptr<OnlineRomsProvider> reicastCloudRoms(OnlineRomsProvider::CreateHttpProvider("http://cloudroms.reicast.com", "/homebrew.lst"));
static unique_ptr<OnlineRomsProvider> archiveChdCloudRoms(OnlineRomsProvider::CreateHttpProvider("http://cloudroms.reicast.com", "/archive.org-chd.lst"));
static unique_ptr<OnlineRomsProvider> archiveCueCloudRoms(OnlineRomsProvider::CreateHttpProvider("http://cloudroms.reicast.com", "/archive.org-cue.lst"));


#define VMU_WIDTH (70 * 48 * scaling / 32)
#define VMU_HEIGHT (70 * scaling)
#define VMU_PADDING (8 * scaling)
static u32 vmu_lcd_data[8][48 * 32];
static bool vmu_lcd_status[8];
static ImTextureID vmu_lcd_tex_ids[8];


static float LastFPSTime;
static int lastFrameCount = 0;
static float fps = -1;

static std::string osd_message;
static double osd_message_end;

static const int vmu_coords[8][2] = {
        { 0 , 0 },
        { 0 , 0 },
        { 1 , 0 },
        { 1 , 0 },
        { 0 , 1 },
        { 0 , 1 },
        { 1 , 1 },
        { 1 , 1 },
};

#ifdef _ANDROID
static std::string current_library_path("/storage/emulated/0/Download");
#else
static std::string current_library_path("/home/raph/RetroPie/roms/dreamcast/");
#endif

void reset_vmus()
{
    for (int i = 0; i < ARRAY_SIZE(vmu_lcd_status); i++)
        vmu_lcd_status[i] = false;
}

void push_vmu_screen(int bus_id, int bus_port, u8* buffer)
{
    int vmu_id = bus_id * 2 + bus_port;
    if (vmu_id < 0 || vmu_id >= ARRAY_SIZE(vmu_lcd_data))
        return;
    u32* p = &vmu_lcd_data[vmu_id][0];
    for (int i = 0; i < ARRAY_SIZE(vmu_lcd_data[vmu_id]); i++, buffer++)
        * p++ = *buffer != 0 ? 0xFFFFFFFFu : 0xFF000000u;
    vmu_lcd_status[vmu_id] = true;
}


void ImGui_Impl_NewFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::GetIO().DisplaySize.x = screen_width;
    ImGui::GetIO().DisplaySize.y = screen_height;

    ImGuiIO& io = ImGui::GetIO();

    UpdateInputState(0);

    // Read keyboard modifiers inputs
    io.KeyCtrl = (kb_shift & (0x01 | 0x10)) != 0;
    io.KeyShift = (kb_shift & (0x02 | 0x20)) != 0;
    io.KeyAlt = false;
    io.KeySuper = false;

    memset(&io.KeysDown[0], 0, sizeof(io.KeysDown));
    for (int i = 0; i < IM_ARRAYSIZE(kb_key); i++)
        if (kb_key[i] != 0)
            io.KeysDown[kb_key[i]] = true;
        else
            break;
    float scale = screen_height / 480.0f;
    float x_offset = (screen_width - 640.0f * scale) / 2;
    int real_x = mo_x_abs * scale + x_offset;
    int real_y = mo_y_abs * scale;
    if (real_x < 0 || real_x >= screen_width || real_y < 0 || real_y >= screen_height)
        io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
    else
        io.MousePos = ImVec2(real_x, real_y);
#ifdef _ANDROID
    // Put the "mouse" outside the screen one frame after a touch up
    // This avoids buttons and the like to stay selected
    if ((mo_buttons & 0xf) == 0xf)
    {
        if (touch_up)
        {
            io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
            touch_up = false;
        }
        else if (io.MouseDown[0])
            touch_up = true;
    }
#endif
    if (io.WantCaptureMouse)
    {
        io.MouseWheel = -mo_wheel_delta / 16;
        // Reset all relative mouse positions
        mo_x_delta = 0;
        mo_y_delta = 0;
        mo_wheel_delta = 0;
    }
    io.MouseDown[0] = (mo_buttons & (1 << 2)) == 0;
    io.MouseDown[1] = (mo_buttons & (1 << 1)) == 0;
    io.MouseDown[2] = (mo_buttons & (1 << 3)) == 0;
    io.MouseDown[3] = (mo_buttons & (1 << 0)) == 0;

    io.NavInputs[ImGuiNavInput_Activate] = (kcode[0] & DC_BTN_A) == 0;
    io.NavInputs[ImGuiNavInput_Cancel] = (kcode[0] & DC_BTN_B) == 0;
    io.NavInputs[ImGuiNavInput_Input] = (kcode[0] & DC_BTN_X) == 0;
    //io.NavInputs[ImGuiNavInput_Menu] = (kcode[0] & DC_BTN_Y) == 0;
    io.NavInputs[ImGuiNavInput_DpadLeft] = (kcode[0] & DC_DPAD_LEFT) == 0;
    io.NavInputs[ImGuiNavInput_DpadRight] = (kcode[0] & DC_DPAD_RIGHT) == 0;
    io.NavInputs[ImGuiNavInput_DpadUp] = (kcode[0] & DC_DPAD_UP) == 0;
    io.NavInputs[ImGuiNavInput_DpadDown] = (kcode[0] & DC_DPAD_DOWN) == 0;
    io.NavInputs[ImGuiNavInput_LStickLeft] = joyx[0] < 0 ? -(float)joyx[0] / 128 : 0.f;
    if (io.NavInputs[ImGuiNavInput_LStickLeft] < 0.1f)
        io.NavInputs[ImGuiNavInput_LStickLeft] = 0.f;
    io.NavInputs[ImGuiNavInput_LStickRight] = joyx[0] > 0 ? (float)joyx[0] / 128 : 0.f;
    if (io.NavInputs[ImGuiNavInput_LStickRight] < 0.1f)
        io.NavInputs[ImGuiNavInput_LStickRight] = 0.f;
    io.NavInputs[ImGuiNavInput_LStickUp] = joyy[0] < 0 ? -(float)joyy[0] / 128.f : 0.f;
    if (io.NavInputs[ImGuiNavInput_LStickUp] < 0.1f)
        io.NavInputs[ImGuiNavInput_LStickUp] = 0.f;
    io.NavInputs[ImGuiNavInput_LStickDown] = joyy[0] > 0 ? (float)joyy[0] / 128.f : 0.f;
    if (io.NavInputs[ImGuiNavInput_LStickDown] < 0.1f)
        io.NavInputs[ImGuiNavInput_LStickDown] = 0.f;

    if (KeyboardDevice::GetInstance() != NULL)
    {
    std:string input_text = KeyboardDevice::GetInstance()->get_character_input();
        if (io.WantCaptureKeyboard)
            io.AddInputCharactersUTF8(input_text.c_str());
    }
}

std::unique_ptr<GUI> g_GUI;

struct ReicastUI_impl : GUI {
    
    ImFont* font17;
    ImFont* font64;

    void Init()
    {
        if (inited)
            return;
        inited = true;

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;

        io.IniFilename = NULL;

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

        io.KeyMap[ImGuiKey_Tab] = 0x2B;
        io.KeyMap[ImGuiKey_LeftArrow] = 0x50;
        io.KeyMap[ImGuiKey_RightArrow] = 0x4F;
        io.KeyMap[ImGuiKey_UpArrow] = 0x52;
        io.KeyMap[ImGuiKey_DownArrow] = 0x51;
        io.KeyMap[ImGuiKey_PageUp] = 0x4B;
        io.KeyMap[ImGuiKey_PageDown] = 0x4E;
        io.KeyMap[ImGuiKey_Home] = 0x4A;
        io.KeyMap[ImGuiKey_End] = 0x4D;
        io.KeyMap[ImGuiKey_Insert] = 0x49;
        io.KeyMap[ImGuiKey_Delete] = 0x4C;
        io.KeyMap[ImGuiKey_Backspace] = 0x2A;
        io.KeyMap[ImGuiKey_Space] = 0x2C;
        io.KeyMap[ImGuiKey_Enter] = 0x28;
        io.KeyMap[ImGuiKey_Escape] = 0x29;
        io.KeyMap[ImGuiKey_A] = 0x04;
        io.KeyMap[ImGuiKey_C] = 0x06;
        io.KeyMap[ImGuiKey_V] = 0x19;
        io.KeyMap[ImGuiKey_X] = 0x1B;
        io.KeyMap[ImGuiKey_Y] = 0x1C;
        io.KeyMap[ImGuiKey_Z] = 0x1D;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsClassic();
        ImGui::GetStyle().TabRounding = 0;
        ImGui::GetStyle().ItemSpacing = ImVec2(8, 8);		// from 8,4
        ImGui::GetStyle().ItemInnerSpacing = ImVec2(4, 6);	// from 4,4
        //ImGui::GetStyle().WindowRounding = 0;
#ifdef _ANDROID
        ImGui::GetStyle().GrabMinSize = 20.0f;				// from 10
        ImGui::GetStyle().ScrollbarSize = 24.0f;			// from 16
        ImGui::GetStyle().TouchExtraPadding = ImVec2(1, 1);	// from 0,0
#endif

    // Setup Platform/Renderer bindings
        // ImGui_ImplOpenGL3_Init(); -> this is donwe on the thread now

        // Load Fonts
        // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
        // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
        // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
        // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
        // - Read 'misc/fonts/README.txt' for more instructions and details.
        // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
        //io.Fonts->AddFontDefault();
        //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
        //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
        //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
        //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
        //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
        //IM_ASSERT(font != NULL);

        scaling = max(1.f, screen_dpi / 100.f * 0.75f);
        if (scaling > 1)
            ImGui::GetStyle().ScaleAllSizes(scaling);

        font17 = io.Fonts->AddFontFromMemoryCompressedTTF(roboto_medium_compressed_data, roboto_medium_compressed_size, 17 * scaling);
        // don't use scaling for this one to avoid too big textures

        font64 = io.Fonts->AddFontFromMemoryCompressedTTF(roboto_medium_compressed_data, roboto_medium_compressed_size, 96);
        printf("Screen DPI is %d, size %d x %d. Scaling by %.2f\n", screen_dpi, screen_width, screen_height, scaling);
    }

    void OpenSettings(function<void()> cb)
    {
        if (gui_state == Closed)
        {
            // FIXME: This feels like a hack
            // avoids assert if OpenSettings is called twice, before the core actually stops
            if (sh4_cpu->IsRunning()) {
                virtualDreamcast->Stop([this, cb] {
                    gui_state = Commands;
                    settings_opening = true;
                    HideOSD();
                    cb();
                });
            }
            else {
                cb();
            }
        }
        else if (gui_state == VJoyEdit)
        {
            gui_state = VJoyEditCommands;
            cb();
        }
        else
        {
            cb();
        }
    }

    void RenderUI()
    {
        switch (gui_state)
        {
        case Welcome: 
            gui_welcome(font64);
            break;
        case Settings:
            gui_settings();
            break;
        case Commands:
            gui_render_commands();
            break;
        case Main:
            //gui_display_demo();
        {
            std::string game_file = cfgLoadStr("config", "image", "");
            if (!game_file.empty())
            {
                if (gui_start_game(game_file))
                    gui_state = Closed;
            }
            else
                gui_render_content();
        }
        break;
        case Closed:
            break;
        case Onboarding:
            gui_render_onboarding();
            break;
        case VJoyEdit:
            break;
        case VJoyEditCommands:
#ifdef _ANDROID
            gui_display_vjoy_commands(screen_width, screen_height, scaling);
#endif
            break;
        }

        if (gui_state == Closed)
            gui_state = Closed;
    }

    void DisplayNotification(const char* msg, int duration)
    {
        osd_message = msg;
        osd_message_end = os_GetSeconds() + (double)duration / 1000.0;
    }

    void RenderOSD()
    {
        if (gui_state == VJoyEdit)
            return;
        double now = os_GetSeconds();
        if (!osd_message.empty())
        {
            if (now >= osd_message_end)
                osd_message.clear();
        }
        std::string message;
        if (osd_message.empty())
        {
            message = getFPSNotification();
        }
        else
            message = osd_message;

        if (!message.empty() || settings.rend.FloatVMUs)
        {
            ImGui_Impl_NewFrame();
            ImGui::NewFrame();

            if (!message.empty())
            {
                ImGui::SetNextWindowBgAlpha(0);
                ImGui::SetNextWindowPos(ImVec2(0, screen_height), ImGuiCond_Always, ImVec2(0.f, 1.f));	// Lower left corner

                ImGui::Begin("##osd", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav
                    | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
                ImGui::SetWindowFontScale(1.5);
                ImGui::TextColored(ImVec4(1, 1, 0, 0.7), "%s", message.c_str());
                ImGui::End();
            }
            if (settings.rend.FloatVMUs)
                render_vmus();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
    }

    void OpenOnboarding()
    {
        gui_state = Onboarding;
    }

    ~ReicastUI_impl()
    {
        virtualDreamcast.reset();

        inited = false;
        term_vmus();
        //ImGui_ImplOpenGL3_Shutdown(); // -> this is done on GUI_RENDERER now
        ImGui::DestroyContext();
    }

    void RefreshFiles()
    {
        game_list_done = false;
        subfolders_read = false;
    }

    bool IsOpen() {
        return gui_state != Closed && gui_state != VJoyEdit;
    }

    bool IsVJoyEdit() {
        return gui_state == VJoyEdit;
    }

    bool IsContentBrowser() {
        return gui_state == Main;
    }


    private:

        void gui_dosmth(int width, int height)
        {
            if (last_render == 0)
            {
                last_render = os_GetSeconds();
                return;
            }
            double new_time = os_GetSeconds();
            render_times.push_back((float)(new_time - last_render));
            if (render_times.size() > 100)
                render_times.erase(render_times.begin());
            last_render = new_time;

            ImGui_Impl_NewFrame();
            ImGui::NewFrame();

            ImGui::PlotLines("Render Times", &render_times[0], (int)render_times.size(), 0, "", 0.0, 1.0 / 30.0, ImVec2(300, 50));

            // Render dear imgui into screen
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        static void systemdir_selected_callback(bool cancelled, std::string selection)
        {
            if (!cancelled)
            {
                set_user_config_dir(selection);
                set_user_data_dir(selection);
                if (cfgOpen())
                {
                    LoadSettings(false);
                    gui_state = Main;
                    if (settings.dreamcast.ContentPath.empty())
                        settings.dreamcast.ContentPath.push_back(selection);
                    SaveSettings();
                }
            }
        }

        void gui_render_onboarding()
        {
            ImGui_Impl_NewFrame();
            ImGui::NewFrame();

            ImGui::OpenPopup("Select System Directory");
            select_directory_popup("Select System Directory", scaling, &systemdir_selected_callback);

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData(), false);
        }


    void gui_render_commands()
    {
        verify(!sh4_cpu->IsRunning());
        ImGui_Impl_NewFrame();
        ImGui::NewFrame();
        if (!settings_opening)
            ImGui_ImplOpenGL3_DrawBackground();

        if (!settings.rend.FloatVMUs)
            // If floating VMUs, they are already visible on the background
            render_vmus();

        ImGui::SetNextWindowPos(ImVec2(screen_width / 2.f, screen_height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

        ImGui::Begin("Reicast", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);



        ImGui::Columns(2, "buttons", false);
        if (ImGui::Button("Load State", ImVec2(150 * scaling, 50 * scaling)))
        {
            virtualDreamcast->LoadState();
            gui_state = Closed;
            virtualDreamcast->Resume();
        }
        ImGui::NextColumn();


        if (ImGui::Button("Save State", ImVec2(150 * scaling, 50 * scaling)))
        {
            if (!settings.savepopup.isShown) {
                ImGui::OpenPopup("Warning!");
                settings.savepopup.isShown = true;
                SaveSettings();
            }
            else{
                virtualDreamcast->SaveState();
                gui_state = Closed;
                virtualDreamcast->Resume();
            }

        }

        if (ImGui::BeginPopupModal("Warning!"))
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 250.f * scaling);
            ImGui::Text("Save states are not guaranteed to be compatible between releases, please use with caution and always save to VMU");
            float currentwidth = ImGui::GetContentRegionAvailWidth();
            ImGui::SetCursorPosX((currentwidth - 80.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x);
            if (ImGui::Button("Okay", ImVec2(80.f * scaling, 0.f)))
            {
                ImGui::CloseCurrentPopup();
                virtualDreamcast->SaveState();
                gui_state = Closed;
                virtualDreamcast->Resume();

            }
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }


        ImGui::NextColumn();
        if (ImGui::Button("Settings", ImVec2(150 * scaling, 50 * scaling)))
        {
            gui_state = Settings;
        }
        ImGui::NextColumn();
        if (ImGui::Button("Resume", ImVec2(150 * scaling, 50 * scaling)))
        {
            virtualDreamcast->Resume();
            gui_state = Closed;
        }

        ImGui::NextColumn();
        if (ImGui::Button("Restart", ImVec2(150 * scaling, 50 * scaling)))
        {
            virtualDreamcast->Reset();
            virtualDreamcast->Resume();
            gui_state = Closed;
        }
        ImGui::NextColumn();
        if (ImGui::Button("Exit", ImVec2(150 * scaling, 50 * scaling)))
        {
            // FIXME THIS IS WRONG //
            // Exit to main menu
            gui_state = Main;
            game_started = false;
            virtualDreamcast.reset();

            cfgSetVirtual("config", "image", "");
        }
        ImGui::NextColumn();
        if (ImGui::Button("Report a game bug", ImVec2(150 * scaling, 50 * scaling)))
        {
            os_LaunchFromURL("http://report-games.reicast.com");
        }

        ImGui::NextColumn();
        if (ImGui::Button("Swap Disc", ImVec2(150 * scaling, 50 * scaling)))
        {
            cfgSetVirtual("config", "image", "");
            gui_state = Main;
        }

#if 0
        ImGui::NextColumn();
        if (ImGui::Button("RenderDone Int", ImVec2(150 * scaling, 50 * scaling)))
        {
            asic_RaiseInterrupt(holly_RENDER_DONE);
            virtualDreamcast->Resume();
            gui_state = Closed;
        }
#endif
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData(), settings_opening);
        settings_opening = false;
    }

    std::string error_msg;

    void error_popup()
    {
        if (!error_msg.empty())
        {
            if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 400.f * scaling);
                ImGui::TextWrapped("%s", error_msg.c_str());
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, 3 * scaling));
                float currentwidth = ImGui::GetContentRegionAvailWidth();
                ImGui::SetCursorPosX((currentwidth - 80.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x);
                if (ImGui::Button("OK", ImVec2(80.f * scaling, 0.f)))
                {
                    error_msg.clear();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SetItemDefaultFocus();
                ImGui::PopStyleVar();
                ImGui::EndPopup();
            }
            ImGui::OpenPopup("Error");
        }
    }

    void add_game_directory(const std::string& path, std::vector<GameMedia>& game_list)
    {
        //printf("Exploring %s\n", path.c_str());
        DIR* dir = opendir(path.c_str());
        if (dir == NULL)
            return;


        while (true)
        {
            struct dirent* entry = readdir(dir);
            if (entry == NULL)
                break;
        std:string name(entry->d_name);
            if (name == "." || name == "..")
                continue;
            std::string child_path = path + "/" + name;
            bool is_dir = false;
#ifndef _WIN32
            if (entry->d_type == DT_DIR)
                is_dir = true;
            if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK)
#endif
            {
                struct stat st;
                if (stat(child_path.c_str(), &st) != 0)
                    continue;
                if (S_ISDIR(st.st_mode))
                    is_dir = true;
            }
            if (is_dir)
            {
                add_game_directory(child_path, game_list);
            }
            else
            {
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
                if (name.size() >= 4)
                {
                    std::string extension = name.substr(name.size() - 4).c_str();
                    //printf("  found game %s ext %s\n", entry->d_name, extension.c_str());
                    if (stricmp(extension.c_str(), ".cdi") && stricmp(extension.c_str(), ".gdi") && stricmp(extension.c_str(), ".chd") && stricmp(extension.c_str(), ".cue"))
                        continue;
                    game_list.push_back({ name, child_path });
                }
#else
                std::string::size_type dotpos = name.find_last_of(".");
                if (dotpos == std::string::npos || dotpos == name.size() - 1)
                    continue;
                std::string extension = name.substr(dotpos);
                if (stricmp(extension.c_str(), ".zip") && stricmp(extension.c_str(), ".7z") && stricmp(extension.c_str(), ".bin")
                    && stricmp(extension.c_str(), ".lst") && stricmp(extension.c_str(), ".dat"))
                    continue;
                game_list.push_back({ name, child_path });
#endif
            }
        }
        closedir(dir);
    }

    void fetch_game_list()
    {
        if (game_list_done)
            return;

        game_list.clear();
        content_paths.clear();
        dirs.clear();
        bool isMultiple = false;

        if (settings.dreamcast.ContentPath.size() > 1)
            isMultiple = true;

        for (auto path : settings.dreamcast.ContentPath) {
            add_game_directory(path, game_list);
            if (isMultiple) 
                currentDirectory = "ShowAll";
            else 
                currentDirectory = path;
            
            content_paths.push_back(path.c_str());
        }
        initDirectoryVector();
        std::stable_sort(game_list.begin(), game_list.end());
        game_list_done = true;
    }

    void fetch_game_list(const std::string& path, bool multiplePaths) //this fetch_game_list runs when browsing from directories bar
    {
        if (multiplePaths) {
            currentDirectory = "ShowAll";
            game_list.clear();
            for (auto all_dirs : settings.dreamcast.ContentPath) {
                add_game_directory(all_dirs, game_list);
                std::stable_sort(game_list.begin(), game_list.end());
            }
            game_list_done = true;
        }
        else {
            currentDirectory = path;
            game_list.clear();
            add_game_directory(path, game_list);
            std::stable_sort(game_list.begin(), game_list.end());
            game_list_done = true;
        }
        
        
    }

    void initDirectoryVector() {  //initialization of the vectors used
        for (auto parent_dirs : game_list) {
            std::string splitter = parent_dirs.path.substr(0, parent_dirs.path.find_last_of("/\\"));
            dirs.push_back(splitter.c_str());
        }

        for (auto path : settings.dreamcast.ContentPath)
            dirs.push_back(path);

        if (settings.dreamcast.ContentPath.size() > 1) {
            content_paths.push_back("ShowAll");
            dirs.push_back("ShowAll");
        }
        
        std::sort(dirs.begin(), dirs.end());
        dirs.erase(std::unique(dirs.begin(), dirs.end()), dirs.end());
        for (int i = 0; i < dirs.size(); i++) {
            if (dirs[i].empty()) {
                dirs.erase(dirs.begin() + i);
            }
        }

    }

    std::string getFormattedDirs(const std::string dir_str) { //formatting the appearence of vectors elements
        std::string forTitle;
        bool compare = false;

        if (dir_str.compare("ShowAll") == 0) 
            return "All Roms";

       

        for (auto paths : content_paths) {
            if (paths.compare(dir_str) == 0) {
                compare = true;
            }
        }

        if (!compare) {
            for (auto paths : content_paths) {
                if (dir_str.find(paths.c_str()) != std::string::npos) {
                    std::size_t foundinTitle = paths.find_last_of("/\\");
                    std::string temp_holder = dir_str.substr(foundinTitle + 1);
                    return forTitle.append("/").append(temp_holder);
                }
            }
        }
        else {
            std::size_t foundinTitle = dir_str.find_last_of("/\\");
            return forTitle = dir_str.substr(foundinTitle + 1);
        }

    }

    void showDirectoriesBar() {  //Directories bar itself!
       
        ImGui::Text("Current Directory: ");
        ImGui::SameLine();
        std::string forTitle = getFormattedDirs(currentDirectory);
        if (ImGui::BeginCombo(" ", forTitle.c_str(), ImGuiComboFlags_None))
        {
            for (int i = 0; i < dirs.size(); i++)
            {
                std::string forSelectable = getFormattedDirs(dirs[i]);
                bool is_selected = false;
                if (ImGui::Selectable(forSelectable.c_str(), &is_selected)) {
                    if (forSelectable.compare("All Roms") != 0) {
                        RefreshFiles();
                        fetch_game_list(dirs[i], false);
                    }
                    else {
                        RefreshFiles();
                        fetch_game_list(dirs[i] , true);
                    }
                }
            }
            ImGui::EndCombo();
        }
    }

    void gui_render_demo()
    {
        ImGui_Impl_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData(), false);
    }

    bool gui_start_game(const std::string& path)
    {
        // do disc swap
        if (game_started)
        {
            cfgSetVirtual("config", "image", path.c_str());
            g_GDRDisc->Swap();

            verify(virtualDreamcast != nullptr);
            virtualDreamcast->Resume();
            return true;
        }

        auto bios_path = get_readonly_data_path(DATA_PATH);

        virtualDreamcast.reset(VirtualDreamcast::Create());

        virtualDreamcast->Init();

        int rc = virtualDreamcast->StartGame(path);
        
        if (rc != 0)
        {
            gui_state = Main;
            game_started = false;
            virtualDreamcast.reset();

            cfgSetVirtual("config", "image", "");
            switch (rc) {
            case -3:
                error_msg = "Audio/video initialization failed";
                break;
            case -5:
                error_msg = "Please put Dreamcast BIOS in " + bios_path;
                break;
            case -6:
                error_msg = "Cannot load NAOMI rom or BIOS";
                break;
            default:
                break;
            }

            return false;
        }
        else
        {
            game_started = true;
            return true;
        }
    }

    void downloading_popup(OnlineRomsProvider* onlineRoms)
    {
        if (onlineRoms->downloadingAny())
        {
            if (ImGui::BeginPopupModal("Downloading", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 400.f * scaling);

                if (onlineRoms->hasDownloadErrored())
                {
                    ImGui::TextWrapped("Download failed");
                }
                else
                {
                    ImGui::TextWrapped("%s\n\n%.2f %%", onlineRoms->getDownloadName().c_str(), onlineRoms->getDownloadPercent());
                }

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * scaling, 3 * scaling));
                float currentwidth = ImGui::GetContentRegionAvailWidth();
                ImGui::SetCursorPosX((currentwidth - 80.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x);
                if (ImGui::Button(onlineRoms->hasDownloadErrored() ? "Close" : "Cancel", ImVec2(80.f * scaling, 0.f)))
                {
                    if (onlineRoms->hasDownloadErrored())
                    {
                        onlineRoms->remove(onlineRoms->getDownloadId());
                        onlineRoms->clearDownloadStatus();
                        RefreshFiles();
                        ImGui::CloseCurrentPopup();
                    }
                    else
                    {
                        onlineRoms->downloadCancel();
                    }
                }

                if (onlineRoms->hasDownloadFinished())
                {
                    onlineRoms->downloaded(onlineRoms->getDownloadId());
                    RefreshFiles();
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SetItemDefaultFocus();
                ImGui::PopStyleVar();
                ImGui::EndPopup();
            }
            ImGui::OpenPopup("Downloading");
        }
    }

    void gui_render_online_roms(bool showBuy, const char* name, OnlineRomsProvider* onlineRoms)
    {
        ImGui::TextColored(ImVec4(1, 1, 1, 0.7), "%s", name);
        ImGui::SameLine();

        auto status = onlineRoms->getStatus();

        if (status.length())
        {
            ImGui::TextColored(ImVec4(1, 1, 1, 0.7), "(%s)", status.c_str());
            ImGui::SameLine();
        }

        if (ImGui::Button((string("Load ") + name).c_str()))
        {
            if (!showBuy || msgboxf("By pressing OK I declare that I have reviewed the laws and regulations that apply to me, and that it is legal for me to download roms from https://archive.org.", MBX_OK) == MBX_RV_OK)
            {
                onlineRoms->fetchRomList();
            }
        }

        auto roms = onlineRoms->getRoms();

        for (auto it = roms.begin(); roms.end() != it; it++)
        {
            ImGui::PushID(it->id.c_str());
            ImGui::Text("%s (%s)", it->name.c_str(), it->type.c_str());
            
            ImGui::SameLine();
            
            #if EXPERIMENTAL_SHOW_BUY
            if (showBuy)
            {   
                if (ImGui::Button("Buy"))
                {
                    auto last_dot = it->name.find_last_of('.');
                    string name = it->name.substr(0, last_dot);

                    os_LaunchFromURL("https://www.amazon.com/s?k=" + url_encode("dreamcast " + name));
                }

                ImGui::SameLine();
            }
            #endif

            if (it->status == RS_DOWNLOADED)
            {
                if (ImGui::Button("Delete"))
                {
                    onlineRoms->remove(it->id);
                    RefreshFiles();
                }
            }
            else if (it->status == RS_MISSING)
            {
                #if EXPERIMENTAL_STREAM_HTTP
                // stream is too slow. We need to cache and batch and such.
                if (ImGui::Button("Stream"))
                {
                    if (gui_start_game(it->url))
                        gui_state = Closed;
                }
                #endif

                ImGui::SameLine();

                if (ImGui::Button("Download"))
                {
                    onlineRoms->download(it->id);
                    RefreshFiles();
                }
            }
            else
            {
                ImGui::Text("(Downloading...)");
            }
            ImGui::PopID();
        }
    }

    std::string convertToString(char* a, int size)
    {
        int i;
        string s = "";
        for (i = 0; i < size; i++) {
            if (a[i] != 0)
                s = s + a[i];
        }
        for (int i = 0; i < s.length(); i++) {
            
            if ((s[i] >= 48 && s[i] <= 57) ||
                (s[i] >= 65 && s[i] <= 90) ||
                (s[i] >= 97 && s[i] <= 122))
            {
                continue;
            }
            else
                return "";
        }
        return s;
    }

    void loadExplorerPopups() { //various popups, saved in this function to keep things (relatively) tidy

        if (!file_toRename.empty()) {
            static char buf[64]{ "" };
            std::string extension = file_toRename.substr(file_toRename.size() - 4).c_str();
            if (ImGui::BeginPopupModal("Rename File")) {
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 250.f * scaling);
                ImGui::Text("Please, enter a valid new name for the file (Cannot contain special symbols and spaces).");
                ImGui::Text(file_toRename.c_str());
                ImGui::InputText("##input", buf, IM_ARRAYSIZE(buf));
                ImGui::SameLine();
                ImGui::Text(extension.c_str());
                float currentwidth = ImGui::GetContentRegionAvailWidth();
                ImGui::SetCursorPosX(
                    (currentwidth - 80.f * scaling) / 3.f + ImGui::GetStyle().WindowPadding.x);
                if (ImGui::Button("Okay", ImVec2(50.f * scaling, 0.f))) {
                    int s_size = sizeof(buf) / sizeof(char);
                    std::string new_name = convertToString(buf, s_size);
                    if (!new_name.empty()) {
                        new_name.append(extension);
                        if (renameFile(path_toRename, new_name)) {
                            path_toRename = "";
                            file_toRename = "";
                            buf[0] = '\0';
                            ImGui::CloseCurrentPopup();
                            RefreshFiles();
                            
                        }
                        else  
                            buf[0] = '\0';
                    }
                    else {
                        buf[0] = '\0';
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(70.f * scaling, 0.f))) {
                    path_toRename = "";
                    file_toRename = "";
                    buf[0] = '\0';
                    ImGui::CloseCurrentPopup();
                    RefreshFiles();
                }

                ImGui::SetItemDefaultFocus();
                ImGui::EndPopup();
            }
        }

        if (ImGui::BeginPopupModal("Error on Delete"))
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 250.f * scaling);
            ImGui::Text("Error deleting file. Please, try removing it manually.");
            float currentwidth = ImGui::GetContentRegionAvailWidth();
            ImGui::SetCursorPosX((currentwidth - 80.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x);
            if (ImGui::Button("Okay", ImVec2(80.f * scaling, 0.f)))
            {
                ImGui::CloseCurrentPopup();
                RefreshFiles();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopupModal("File Deleted")) {
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 250.f * scaling);
            ImGui::Text("File Deleted Successfully!");
            float currentwidth = ImGui::GetContentRegionAvailWidth();
            ImGui::SetCursorPosX(
                (currentwidth - 80.f * scaling) / 2.f + ImGui::GetStyle().WindowPadding.x);
            if (ImGui::Button("Okay", ImVec2(80.f * scaling, 0.f))) {
                ImGui::CloseCurrentPopup();
                RefreshFiles();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }

    }

    void gui_render_content()
    {
        ImGui_Impl_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(screen_width, screen_height));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

        ImGui::Begin("##main", NULL, ImGuiWindowFlags_NoDecoration);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20 * scaling, 8 * scaling));		// from 8, 4
        ImGui::AlignTextToFramePadding();
        ImGui::Text("GAMES");

        static ImGuiTextFilter filter;
        if (KeyboardDevice::GetInstance() != NULL)
        {
            ImGui::SameLine(0, 32 * scaling);
            filter.Draw("Filter");
        }

        ImGui::SameLine(ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize("Settings").x - ImGui::GetStyle().FramePadding.x * 2.0f /*+ ImGui::GetStyle().ItemSpacing.x*/);
        if (ImGui::Button("Settings"))//, ImVec2(0, 30 * scaling)))
            gui_state = Settings;
        ImGui::PopStyleVar();

        fetch_game_list();

        // Only if Filter and Settings aren't focused... ImGui::SetNextWindowFocus();
        ImGui::BeginChild(ImGui::GetID("library"), ImVec2(0, 0), true);
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * scaling, 20 * scaling));		// from 8, 4

            if (!settings.social.HideCallToAction)
            {
                ImGui::PushID("discord");
                if (ImGui::Selectable("Join our Discord Server!"))
                {
                    os_LaunchFromURL("http://chat.reicast.com");
                }
                ImGui::PopID();

                ImGui::Separator();
            }

#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
            ImGui::PushID("nodisk");
            if (ImGui::Selectable("Dreamcast BIOS"))
            {
                if (gui_start_game("nodisk"))
                    gui_state = Closed;
            }
            ImGui::PopID();
#endif


            showDirectoriesBar();

            ImGui::Text("%s", "");
            ImGui::TextColored(ImVec4(1, 1, 1, 0.7), "LOCAL ROMS");
            loadExplorerPopups();
            int gamelistSelection = -1;
            std::string game_name;
            std::string game_path;
            const char* rom_extras[] = { "Play", "Rename", "Delete" }; //new buttons
            for (auto game : game_list)
                if (filter.PassFilter(game.name.c_str()))
                {
                    ImGui::PushID(game.path.c_str());
                    if (ImGui::Selectable(game.name.c_str()))
                    {
                        if (gui_start_game(game.path))
                            gui_state = Closed;
                    }
                    ImGui::SetItemAllowOverlap();
                    ImGui::SameLine(ImGui::GetContentRegionAvailWidth() - ImGui::CalcTextSize(" ...").x  /*+ ImGui::GetStyle().ItemSpacing.x*/);
                    
                    if (ImGui::BeginCombo("##gamesCombo", " ... ", ImGuiComboFlags_NoArrowButton))  //combo inside PushID
                    {
                        for (int i = 0; i < IM_ARRAYSIZE(rom_extras); i++)
                        {
                            bool is_selected = false;
                            if (ImGui::Selectable(rom_extras[i], &is_selected)) {
                                game_name = game.name;
                                game_path = game.path;
                                gamelistSelection = i;
                            }
                            if (is_selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::PopID();
                }


            if (gamelistSelection != -1) { //drawing user selection out of loop, to avoid ImGUI scope issues and running unnecessary functions over and over again.
                if (gamelistSelection == 0) {
                    if (gui_start_game(game_path))
                        gui_state = Closed;
                }
                else if (gamelistSelection == 2) {
                    if (deleteFile(game_path))
                        ImGui::OpenPopup("File Deleted");
                    else
                        ImGui::OpenPopup("Error on Delete");
                }
                else {
                    
#ifdef _ANDROID
                    std::string showvirtualKeyboard(const std::string & new_vk);  //JNI function, can be found under NativeGLActivity.java and Android.cpp
                    std::string android_response = showvirtualKeyboard(game_name.c_str());
                    if (android_response.compare("..") != 0) {
                        if(renameFile(game_path, android_response))
                            RefreshFiles();
                            fetch_game_list();
                    }
#else
                    file_toRename = game_name;
                    path_toRename = game_path;
                    ImGui::OpenPopup("Rename File");
#endif

                }
            }




            if (!settings.cloudroms.HideHomebrew)
            {
                ImGui::Text("%s", "");

                gui_render_online_roms(false, "HOMEBREW", reicastCloudRoms.get());
            }

            if (settings.cloudroms.ShowArchiveOrg)
            {
                ImGui::Text("%s", "");
                gui_render_online_roms(true, "ARCHIVE.ORG (CHD)", archiveChdCloudRoms.get());

                ImGui::Text("%s", "");
                gui_render_online_roms(true, "ARCHIVE.ORG (CUE / .7z)", archiveCueCloudRoms.get());
            }

            ImGui::PopStyleVar();
        }
        ImGui::EndChild();
        ImGui::End();
        ImGui::PopStyleVar();

        error_popup();
        downloading_popup(reicastCloudRoms.get());

        downloading_popup(archiveChdCloudRoms.get());

        downloading_popup(archiveCueCloudRoms.get());

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData(), false);
    }
    
    std::string getFPSNotification()
    {
        if (settings.rend.ShowFPS)
        {
            double now = os_GetSeconds();
            if (now - LastFPSTime >= 1.0) {
                fps = (FrameCount - lastFrameCount) / (now - LastFPSTime);
                LastFPSTime = now;
                lastFrameCount = FrameCount;
            }
            if (fps >= 0.f && fps < 9999.f) {
                char text[32];
                snprintf(text, sizeof(text), "F:%.1f", fps);

                return std::string(text);
            }
        }
        return std::string("");
    }

    bool renameFile(const std::string& path, const std::string& newname) {
        std::string path_nofile = path.substr(0, path.find_last_of("\\/"));
        path_nofile.append("/").append(newname);
        if (rename(path.c_str(), path_nofile.c_str()) != 0) 
            return false;
        else 
            return true;
        

    }

    bool deleteFile(const std::string& path) {
        if (remove(path.c_str()) != 0) {
            return false;
        }
        else {
            return true;
        }
    }

    void render_vmus()
    {
        if (!game_started)
            return;
        ImGui::SetNextWindowBgAlpha(0);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(screen_width, screen_height));

        ImGui::Begin("vmu-window", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs
            | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing);
        for (int i = 0; i < 8; i++)
        {
            if (!vmu_lcd_status[i])
                continue;

            if (vmu_lcd_tex_ids[i] != (ImTextureID)0)
                ImGui_ImplOpenGL3_DeleteVmuTexture(vmu_lcd_tex_ids[i]);
            vmu_lcd_tex_ids[i] = ImGui_ImplOpenGL3_CreateVmuTexture(vmu_lcd_data[i]);

            int x = vmu_coords[i][0];
            int y = vmu_coords[i][1];
            ImVec2 pos;
            if (x == 0)
                pos.x = VMU_PADDING;
            else
                pos.x = screen_width - VMU_WIDTH - VMU_PADDING;
            if (y == 0)
            {
                pos.y = VMU_PADDING;
                if (i & 1)
                    pos.y += VMU_HEIGHT + VMU_PADDING;
            }
            else
            {
                pos.y = screen_height - VMU_HEIGHT - VMU_PADDING;
                if (i & 1)
                    pos.y -= VMU_HEIGHT + VMU_PADDING;
            }
            ImVec2 pos_b(pos.x + VMU_WIDTH, pos.y + VMU_HEIGHT);
            ImGui::GetWindowDrawList()->AddImage(vmu_lcd_tex_ids[i], pos, pos_b, ImVec2(0, 1), ImVec2(1, 0), 0xC0ffffff);
        }
        ImGui::End();
    }

    void term_vmus()
    {
        for (int i = 0; i < ARRAY_SIZE(vmu_lcd_status); i++)
        {
            if (vmu_lcd_tex_ids[i] != (ImTextureID)0)
            {
                ImGui_ImplOpenGL3_DeleteVmuTexture(vmu_lcd_tex_ids[i]);
                vmu_lcd_tex_ids[i] = (ImTextureID)0;
            }
        }
    }
};

GUI* GUI::Create() {

    return new ReicastUI_impl();
}

#if !defined(_ANDROID)
int msgboxf(const wchar* text, unsigned int type, ...) {
    va_list args;

    wchar temp[2048];
    va_start(args, type);
    vsnprintf(temp, sizeof(temp), text, args);
    va_end(args);
    printf("%s\n", temp);

    g_GUI->DisplayNotification(temp, 2000);

    return 1;
}
#endif
