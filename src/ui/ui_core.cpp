#include <ui/ui.hpp>

namespace Motion
{
    void MotionUI::CreateMainMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            ImGui::MenuItem("Run");
            ImGui::MenuItem("Pause");
            ImGui::MenuItem("Stop");
            
            if (ImGui::BeginMenu("Settings"))
            {
                if (ImGui::BeginMenu("Disk Controller"))
                {
                    ImGui::MenuItem("DSD 5217");
                    ImGui::MenuItem("Storager 3030");
                }
            } 

            if (ImGui::BeginMenu("Hard Drive"))
            {
                ImGui::MenuItem("Current Disk: blahblah.img", NULL, false, false);
                ImGui::MenuItem("Insert Disk...");
            }
            

            ImGui::EndMainMenuBar();
        }
    }
};