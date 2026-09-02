#include <coherent/coherent.hpp>
#include <component/ip2/ip2_duart.hpp>

namespace Motion
{
    // Persistent per-line input boxes for the "type into this port" harness below. Indexed by serial line number
    // (see DUART68681::GetLineIndex) rather than being function-local statics, since each line needs its own buffer.
    static char rxInputBuf[SERIAL_MAX_LINES][STRING_MAX_LONG] = {0};
    static float rxOutputHeight[SERIAL_MAX_LINES] = {0.0f};

    void CoherentExtensionDUART68681::DrawStatusFlag(const char* name, bool set, bool badWhenSet)
    {
        ImVec4 onColor = badWhenSet ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
        ImVec4 offColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

        ImGui::TextColored(set ? onColor : offColor, "%s", name);
    }

    /// @brief Draws a small interactive terminal for one raw serial line: everything it's sent so far, and a
    /// box to type a line into its receiver - Enter (or the Send button) submits it terminated with a carriage
    /// return, and refocuses itself afterwards so you can keep typing without having to re-click on the thing
    /// @param line The serial line to interact with.
    /// @param lineNum Index of the line (see DUART68681::GetLineIndex) - keys the persistent per-line input buffer and widget IDs.
    /// @param outputHeight Height of the scrolling output area.
    void CoherentExtensionDUART68681::DrawConsole(SerialLine& line, int32_t lineNum, float outputHeight)
    {
        if (rxOutputHeight[lineNum] <= 0.0f)
            rxOutputHeight[lineNum] = outputHeight;

        ImGui::PushID(lineNum);

        ImGui::BeginChild("output", ImVec2(-1, rxOutputHeight[lineNum]), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY);
        rxOutputHeight[lineNum] = ImGui::GetWindowSize().y;
        ImGui::TextUnformatted(line.GetTxLog().c_str());
        ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();

        if (ImGui::SmallButton("Clear output"))
            line.ClearTxLog();

        ImGui::SetNextItemWidth(-70.0f);

        bool textSubmitted = ImGui::InputTextWithHint("##input", "Type a line and press Enter...", rxInputBuf[lineNum], STRING_MAX_LONG, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        bool buttonSubmitted = ImGui::Button("Send");

        if (textSubmitted 
        || buttonSubmitted)
        {
            line.AddRxString(rxInputBuf[lineNum]);
            line.AddRxByte('\r');
            rxInputBuf[lineNum][0] = '\0';

            if (textSubmitted)
                ImGui::SetKeyboardFocusHere(-1);
        }

        ImGui::PopID();
    }

    // DRAw the channel UI
    void CoherentExtensionDUART68681::DrawChannelUI(DUART68681* duartComponent, int32_t duartId, int32_t channelId)
    {
        DUART68681::UARTChannel& channel = duartComponent->duarts[duartId].channels[channelId];
        int32_t lineNum = duartComponent->GetLineIndex(duartId, channelId);

        ImGui::PushID(lineNum);
        ImGui::SeparatorText(channelId ? "Channel B" : "Channel A");

        ImGui::Text("MR1: 0x%02X   MR2: 0x%02X   CSR: 0x%02X", channel.mode1, channel.mode2, channel.clocksel);
        ImGui::Text("Baud rate: RX %d   TX %d", channel.baudRateRX, channel.baudRateTX);
        ImGui::Text("Framing: %d data bits, %s parity, %d stop bit period(s)",
            channel.dataBits, channel.parityEnabled ? "with" : "no", channel.stopBitPeriods);

        ImGui::Text("Transmitter: %s   Receiver: %s", channel.txEnabled ? "Enabled" : "Disabled", channel.rxEnabled ? "Enabled" : "Disabled");

        DrawStatusFlag("RxRDY", channel.status & DUART_STATUS_RECEIVER_READY);
        ImGui::SameLine();
        DrawStatusFlag("TxRDY", channel.status & DUART_STATUS_TRANSMITTER_READY);
        ImGui::SameLine();
        DrawStatusFlag("TxEMT", channel.status & DUART_STATUS_TRANSMITTER_EMPTY);
        ImGui::SameLine();
        DrawStatusFlag("FFULL", channel.status & DUART_STATUS_FIFO_FULL, true);
        ImGui::SameLine();
        DrawStatusFlag("OVERRUN", channel.status & DUART_STATUS_OVERRUN_ERROR, true);
        ImGui::SameLine();
        DrawStatusFlag("BREAK", channel.status & DUART_STATUS_RECEIVED_BREAK, true);

        ImGui::Text("RX FIFO: %d / %d bytes buffered", channel.rxFifoFree, DUART_FIFO_SIZE);

        SerialLine& line = duartComponent->GetLine(lineNum);

        ImGui::Text("Output:");
        DrawConsole(line, lineNum, 100.0f);

        ImGui::PopID();
    }

    void CoherentExtensionDUART68681::AddUI()
    {
        DUART68681* duartComponent = (DUART68681*)component;

        ImGui::SetNextWindowSize(ImVec2(520, 700), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Serial Console & DUART State", &enabled))
        {
            // The PROM in serial console mode uses DUART 0 Channel B, or as sgi called it, Port2.
            int32_t consoleLineNum = duartComponent->GetLineIndex(DUART_PORT2_DUART_INDEX, DUART_PORT2_CHANNEL);
            SerialLine& consoleLine = duartComponent->GetLine(consoleLineNum);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.8f, 1.0f, 1.0f));
            ImGui::Text("PROM Console (DUART%d channel %c)", DUART_PORT2_DUART_INDEX, DUART_PORT2_CHANNEL ? 'B' : 'A');
            ImGui::PopStyleColor();

            DrawConsole(consoleLine, consoleLineNum, 380.0f);

            ImGui::Separator();

            for (int32_t d = 0; d < 2; d++)
            {
                DUART68681::DUART& duart = duartComponent->duarts[d];

                if (ImGui::CollapsingHeader(std::format("DUART{} @ {}", d, d ? "0x32800000" : "0x32000000").c_str()))
                {
                    ImGui::Text("ISR: 0x%02X   IMR: 0x%02X   IVR: 0x%02X   ACR: 0x%02X   OPR: 0x%02X   OPCR: 0x%02X",
                        duart.isr, duart.imr, duart.ivr, duart.auxControl, duart.opr, duart.opcr);

                    DrawChannelUI(duartComponent, d, 0);
                    DrawChannelUI(duartComponent, d, 1);
                }
            }
        }

        ImGui::End();
    }
}
