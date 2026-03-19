//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

// SocketReaderWriter - Helper class for reading/writing messages over StreamSocket
// Converted from C++/CX to C++/WinRT

#pragma once

#include "pch.h"

using namespace winrt;
using namespace winrt::Windows::Networking::Sockets;
using namespace winrt::Windows::Storage::Streams;

// Constants
inline const std::array<uint8_t, 3> CustomOui = { 0xAA, 0xBB, 0xCC };
inline const uint8_t CustomOuiType = 0xDD;
inline const std::array<uint8_t, 3> WfaOui = { 0x50, 0x6F, 0x9A };
inline const std::array<uint8_t, 3> MsftOui = { 0x00, 0x50, 0xF2 };
inline const winrt::hstring strServerPort = L"50001";

// Simple message logger (replaces rootPage->NotifyUser)
inline void LogMessage(const std::wstring& message, bool isError = false)
{
    if (isError)
    {
        std::wcerr << L"[ERROR] " << message << std::endl;
    }
    else
    {
        std::wcout << L"[INFO]  " << message << std::endl;
    }
}

class SocketReaderWriter
{
public:
    SocketReaderWriter(StreamSocket const& socket)
        : m_streamSocket(socket),
          m_currentMessage(L"")
    {
        m_socketReader = DataReader(socket.InputStream());
        m_socketReader.UnicodeEncoding(winrt::Windows::Storage::Streams::UnicodeEncoding::Utf8);
        m_socketReader.ByteOrder(winrt::Windows::Storage::Streams::ByteOrder::LittleEndian);

        m_socketWriter = DataWriter(socket.OutputStream());
        m_socketWriter.UnicodeEncoding(winrt::Windows::Storage::Streams::UnicodeEncoding::Utf8);
        m_socketWriter.ByteOrder(winrt::Windows::Storage::Streams::ByteOrder::LittleEndian);
    }

    // Write a message to the socket
    void WriteMessage(const winrt::hstring& message)
    {
        try
        {
            m_socketWriter.WriteUInt32(m_socketWriter.MeasureString(message));
            m_socketWriter.WriteString(message);
            uint32_t numBytesWritten = m_socketWriter.StoreAsync().get();
            if (numBytesWritten > 0)
            {
                LogMessage(L"Sent message: " + std::wstring(message.c_str()));
            }
            else
            {
                LogMessage(L"The remote side closed the socket", true);
            }
        }
        catch (const winrt::hresult_error& e)
        {
            LogMessage(L"Failed to send message: " + std::wstring(e.message().c_str()), true);
        }
    }

    // Read a message from the socket (blocking)
    void ReadMessage()
    {
        try
        {
            uint32_t bytesRead = m_socketReader.LoadAsync(sizeof(uint32_t)).get();
            if (bytesRead > 0)
            {
                uint32_t strLength = m_socketReader.ReadUInt32();
                uint32_t stringBytesRead = m_socketReader.LoadAsync(strLength).get();
                if (stringBytesRead > 0)
                {
                    m_currentMessage = m_socketReader.ReadString(strLength);
                    LogMessage(L"Got message: " + std::wstring(m_currentMessage.c_str()));
                }
                else
                {
                    LogMessage(L"The remote side closed the socket", true);
                }
            }
            else
            {
                LogMessage(L"The remote side closed the socket", true);
            }
        }
        catch (const winrt::hresult_error& e)
        {
            LogMessage(L"Failed to read from socket: " + std::wstring(e.message().c_str()), true);
        }
    }

    // Continuously read messages in a loop (runs in background thread)
    void StartReading()
    {
        m_reading = true;
        m_readThread = std::thread([this]()
        {
            while (m_reading)
            {
                try
                {
                    uint32_t bytesRead = m_socketReader.LoadAsync(sizeof(uint32_t)).get();
                    if (bytesRead > 0)
                    {
                        uint32_t strLength = m_socketReader.ReadUInt32();
                        uint32_t stringBytesRead = m_socketReader.LoadAsync(strLength).get();
                        if (stringBytesRead > 0)
                        {
                            m_currentMessage = m_socketReader.ReadString(strLength);
                            LogMessage(L"Got message: " + std::wstring(m_currentMessage.c_str()));
                        }
                        else
                        {
                            LogMessage(L"The remote side closed the socket", true);
                            m_reading = false;
                        }
                    }
                    else
                    {
                        LogMessage(L"The remote side closed the socket", true);
                        m_reading = false;
                    }
                }
                catch (...)
                {
                    m_reading = false;
                }
            }
        });
        m_readThread.detach();
    }

    void Close()
    {
        m_reading = false;
        m_socketReader.Close();
        m_socketWriter.Close();
        m_streamSocket.Close();
    }

    winrt::hstring GetCurrentMessage() const
    {
        return m_currentMessage;
    }

private:
    DataReader m_socketReader{ nullptr };
    DataWriter m_socketWriter{ nullptr };
    StreamSocket m_streamSocket{ nullptr };
    winrt::hstring m_currentMessage;
    std::atomic<bool> m_reading{ false };
    std::thread m_readThread;
};

// Represents a discovered Wi-Fi Direct device
struct DiscoveredDevice
{
    winrt::Windows::Devices::Enumeration::DeviceInformation DeviceInfo;
    std::wstring DisplayName;

    DiscoveredDevice(winrt::Windows::Devices::Enumeration::DeviceInformation const& info)
        : DeviceInfo(info), DisplayName(info.Name().c_str())
    {
    }
};

// Represents a connected Wi-Fi Direct device
struct ConnectedDevice
{
    std::wstring DisplayName;
    winrt::Windows::Devices::WiFiDirect::WiFiDirectDevice WfdDevice;
    std::shared_ptr<SocketReaderWriter> SocketRW;

    ConnectedDevice(const std::wstring& displayName,
                    winrt::Windows::Devices::WiFiDirect::WiFiDirectDevice const& wfdDevice,
                    std::shared_ptr<SocketReaderWriter> socketRW)
        : DisplayName(displayName), WfdDevice(wfdDevice), SocketRW(socketRW)
    {
    }
};
