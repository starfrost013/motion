#include <base/emulation.hpp>
#include <ui/ui.hpp>

namespace Motion
{
    void MotionUI::CreateMainMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::MenuItem("Run"))
                Emulation::SetPaused(false);

            if (ImGui::MenuItem("Pause"))
                Emulation::SetPaused(true);

            if (ImGui::MenuItem("Reset"))
                Emulation::Reset();
            
            if (ImGui::BeginMenu("Settings"))
            {
                if (ImGui::BeginMenu("Disk Controller"))
                {
                    ImGui::MenuItem("DSD 5217");
                    ImGui::MenuItem("Storager 3030");

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            } 

            if (ImGui::BeginMenu("Hard Drive"))
            {
                ImGui::MenuItem("Current Disk: blahblah.img", NULL, false, false);
                ImGui::MenuItem("Insert Disk...");

                ImGui::EndMenu();
            }
            

            ImGui::EndMainMenuBar();
        }
    }
};