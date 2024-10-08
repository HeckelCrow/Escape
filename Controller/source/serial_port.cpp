#pragma once
#include "serial_port.hpp"
#include "print.hpp"
#include "scope_exit.hpp"

#include <fmt/format.h>
#include <imgui.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define NO_CENUMERATESERIAL_USING_CREATEFILE
// #define NO_CENUMERATESERIAL_USING_QUERYDOSDEVICE
#define NO_CENUMERATESERIAL_USING_GETDEFAULTCOMMCONFIG
#define NO_CENUMERATESERIAL_USING_SETUPAPI1
#define NO_CENUMERATESERIAL_USING_SETUPAPI2
#define NO_CENUMERATESERIAL_USING_ENUMPORTS
#define NO_CENUMERATESERIAL_USING_WMI
#define NO_CENUMERATESERIAL_USING_COMDB
#define NO_CENUMERATESERIAL_USING_REGISTRY
#define NO_CENUMERATESERIAL_USING_GETCOMMPORTS
#include "CEnumerateSerial/enumser.h"

struct SerialPort
{
    SerialPort()
    {
        history.emplace_back();
    }
    // Str   name;
    u32   name   = 0;
    void* handle = nullptr;

    std::vector<Str> history;
    Str              input_buffer = Str(256, '\0');
};

struct SerialPortManager
{
    std::vector<SerialPort> ports;
};

SerialPortManager serial_manager;

void
ListSerialPorts()
{
    CEnumerateSerial::CPortsArray ports;
    if (CEnumerateSerial::UsingQueryDosDevice(ports))
    {
        PrintSuccess("Serial ports:\n");
        for (const auto& p : ports)
        {
            Print("  >COM{}\n", p);
        }
    }
    else
    {
        PrintError("CEnumerateSerial::UsingRegistry failed\n");
    }
}

SerialPort
OpenSerialPort(u32 com, u32 baud_rate)
{
    SerialPort serial = {};

    serial.name    = com;
    auto full_name = fmt::format("\\\\.\\COM{}\\", com);
    serial.handle = CreateFileA(full_name.c_str(), GENERIC_READ | GENERIC_WRITE,
                                0, NULL, OPEN_EXISTING, 0, NULL);
    if (serial.handle == INVALID_HANDLE_VALUE)
    {
        Print("Cannot open port\n");
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND)
        {
            Print("Port doesn't exist\n");
        }
        else
        {
            Print("Error code : {}\n", err);
        }
        serial.handle = nullptr;
        return serial;
    }

    DCB dcb;
    if (GetCommState(serial.handle, &dcb))
    {
        dcb.BaudRate = baud_rate;
        dcb.ByteSize = 8;
        dcb.Parity   = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        dcb.fBinary  = TRUE;
        dcb.fParity  = TRUE;

        if (SetCommState(serial.handle, &dcb))
        {
            // Print("Serial port settings modified!\n");
        }
        else
        {
            Print("Cannot set port settings\n");
        }
    }
    else
    {
        Print("Cannot get port settings\n");
    }

    COMMTIMEOUTS timeout;
    if (GetCommTimeouts(serial.handle, &timeout))
    {
        timeout.ReadIntervalTimeout        = MAXDWORD;
        timeout.ReadTotalTimeoutConstant   = 0;
        timeout.ReadTotalTimeoutMultiplier = 0;
        // timeout.WriteTotalTimeoutConstant   = 1;
        // timeout.WriteTotalTimeoutMultiplier = 1;

        if (SetCommTimeouts(serial.handle, &timeout))
        {
            // Print("Timeout settings modified\n");
        }
        else
        {
            Print("Cannot set timeout settings\n");
        }
    }
    else
    {
        Print("Cannot get timeout settings\n");
    }
    return serial;
}

void
CloseSerialPort(SerialPort& h)
{
    if (h.handle)
    {
        CloseHandle(h.handle);
        h.handle = nullptr;
    }
}

Str
ReadSerial(SerialPort& h)
{
    Str   message;
    char  c = 0;
    DWORD read;
    do
    {
        if (ReadFile(h.handle, &c, 1, &read, NULL))
        {
            if (read)
            {
                message += c;
            }
        }
        else
        {
            Print("ReadFile failed 0x{:X}\n", GetLastError());
            CloseSerialPort(h);
        }
    } while (read > 0);

    return message;
}

void
WriteSerial(SerialPort& h, StrPtr message)
{
    while (message.size())
    {
        DWORD written = 0;
        if (WriteFile(h.handle, &message[0], (DWORD)message.size(), &written,
                      NULL))
        {
            message.remove_prefix(written);
        }
        else
        {
            Print("WriteFile failed\n");
            CloseSerialPort(h);
        }
    }
}

void
InitSerial()
{}

void
TerminateSerial()
{
    for (auto& port : serial_manager.ports)
    {
        CloseSerialPort(port);
    }
    serial_manager = {};
}

void
DrawSerial(SerialPort& port)
{
    if (!ImGui::BeginTabItem(fmt::format("COM{}", port.name).c_str()))
        return;
    // Reserve enough left-over height for 1 separator + 1 input text
    const float footer_height_to_reserve = ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(4, 1)); // Tighten spacing

    for (const auto& str : port.history)
    {
        ImGui::TextUnformatted(str.data());
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue;

    bool grab_focus = false;
    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##SerialInput", port.input_buffer.data(),
                         port.input_buffer.size(), input_text_flags))
    {
        StrPtr command = port.input_buffer.data();

        WriteSerial(port, command);
        WriteSerial(port, "\n");

        port.input_buffer[0] = '\0';
        grab_focus           = true;
    }
    ImGui::PopItemWidth();

    if (grab_focus)
        ImGui::SetKeyboardFocusHere(-1);

    ImGui::EndTabItem();
}

void
UpdateSerial(bool scan_ports)
{
    std::erase_if(serial_manager.ports,
                  [](SerialPort& port) { return port.handle == nullptr; });

    if (scan_ports)
    {
        CEnumerateSerial::CPortsArray new_ports;
        if (CEnumerateSerial::UsingQueryDosDevice(new_ports))
        {
            for (const auto& new_port_name : new_ports)
            {
                bool found = false;
                for (auto& old_port : serial_manager.ports)
                {
                    if (old_port.name == new_port_name)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    PrintSuccess("New serial port COM{}\n", new_port_name);
                    serial_manager.ports.push_back(
                        OpenSerialPort(new_port_name, 115200));
                }
            }
        }
        else
        {
            PrintError("CEnumerateSerial::UsingRegistry failed\n");
        }
    }

    ImGui::Begin("Serial");
    if (ImGui::BeginTabBar("Serial tabs"))
    {
        for (auto& port : serial_manager.ports)
        {
            Str read = ReadSerial(port);
            if (read.size())
            {
                u64 start    = 0;
                u64 new_line = std::string::npos;
                while (true)
                {
                    new_line = read.find_first_of('\n', start);
                    if (new_line == std::string::npos)
                    {
                        port.history.back().append(StrPtr(read).substr(start));
                        break;
                    }

                    port.history.back().append(
                        StrPtr(read).substr(start, new_line - start + 1));
                    start = new_line + 1;
                    port.history.emplace_back();
                }
            }

            DrawSerial(port);
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}