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

// Advertiser Mode - Publishes Wi-Fi Direct advertisement and accepts connections
// Converted from Scenario1_Advertiser.xaml.cpp (C++/CX) to C++/WinRT CLI

#pragma once

#include "pch.h"
#include "SocketReaderWriter.h"

using namespace winrt;
using namespace winrt::Windows::Devices::WiFiDirect;
using namespace winrt::Windows::Networking::Sockets;
using namespace winrt::Windows::Storage::Streams;

inline void PrintAdvertiserMenu()
{
    std::wcout << L"\n--- Advertiser Mode ---" << std::endl;
    std::wcout << L"  1. Start Advertisement" << std::endl;
    std::wcout << L"  2. Stop Advertisement" << std::endl;
    std::wcout << L"  3. Add Information Element (IE)" << std::endl;
    std::wcout << L"  4. Send Message to connected device" << std::endl;
    std::wcout << L"  5. List connected devices" << std::endl;
    std::wcout << L"  6. Close a connected device" << std::endl;
    std::wcout << L"  0. Back to main menu" << std::endl;
    std::wcout << L"Select: ";
}

inline void RunAdvertiser()
{
    WiFiDirectAdvertisementPublisher publisher{ nullptr };
    WiFiDirectConnectionListener listener{ nullptr };
    StreamSocketListener listenerSocket{ nullptr };

    std::vector<std::shared_ptr<ConnectedDevice>> connectedDevices;
    std::mutex devicesMutex;

    winrt::event_token connectionRequestedToken;
    winrt::event_token statusChangedToken;

    bool advertisementStarted = false;
    bool enableListener = true;
    bool isGO = false;
    int listenStateMode = 0; // 0=Normal, 1=Intensive, 2=None
    int goIntent = 0;

    while (true)
    {
        PrintAdvertiserMenu();

        int choice = 0;
        std::wcin >> choice;
        std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');

        switch (choice)
        {
        case 1: // Start Advertisement
        {
            try
            {
                // Prompt user for settings
                std::wcout << L"Enable listener for incoming connections? (y/n): ";
                wchar_t ch;
                std::wcin >> ch;
                std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');
                enableListener = (ch == L'y' || ch == L'Y');

                std::wcout << L"Prefer Group Owner mode? (y/n): ";
                std::wcin >> ch;
                std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');
                isGO = (ch == L'y' || ch == L'Y');

                std::wcout << L"Listen State Discoverability (0=Normal, 1=Intensive, 2=None): ";
                std::wcin >> listenStateMode;
                std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');

                std::wcout << L"Group Owner Intent (0-15): ";
                std::wcin >> goIntent;
                std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');

                if (!publisher)
                {
                    publisher = WiFiDirectAdvertisementPublisher();
                }

                if (enableListener)
                {
                    if (!listener)
                    {
                        listener = WiFiDirectConnectionListener();
                    }

                    connectionRequestedToken = listener.ConnectionRequested(
                        [&connectedDevices, &devicesMutex, &listenerSocket, goIntent](
                            WiFiDirectConnectionListener const& /* sender */,
                            WiFiDirectConnectionRequestedEventArgs const& args)
                    {
                        try
                        {
                            auto connectionRequest = args.GetConnectionRequest();
                            std::wstring deviceName = connectionRequest.DeviceInformation().Name().c_str();
                            LogMessage(L"Connection request received from " + deviceName);

                            // Auto-accept connection in CLI mode
                            LogMessage(L"Auto-accepting connection from " + deviceName + L"...");

                            WiFiDirectConnectionParameters connectionParams;
                            connectionParams.GroupOwnerIntent(static_cast<int16_t>(goIntent));

                            auto wfdDevice = WiFiDirectDevice::FromIdAsync(
                                connectionRequest.DeviceInformation().Id(), connectionParams).get();

                            // Register for connection status changed
                            wfdDevice.ConnectionStatusChanged(
                                [](WiFiDirectDevice const& sender, auto const& /* args */)
                            {
                                LogMessage(L"Connection status changed: " +
                                    std::to_wstring(static_cast<int>(sender.ConnectionStatus())));
                            });

                            auto connectedDevice = std::make_shared<ConnectedDevice>(
                                L"Waiting for client to connect...", wfdDevice, nullptr);

                            {
                                std::lock_guard<std::mutex> lock(devicesMutex);
                                connectedDevices.push_back(connectedDevice);
                            }

                            auto endpointPairs = wfdDevice.GetConnectionEndpointPairs();

                            listenerSocket = StreamSocketListener();
                            listenerSocket.ConnectionReceived(
                                [&connectedDevices, &devicesMutex](
                                    StreamSocketListener const& /* sender */,
                                    StreamSocketListenerConnectionReceivedEventArgs const& args)
                            {
                                LogMessage(L"Connecting to remote side on L4 layer...");
                                auto serverSocket = args.Socket();

                                try
                                {
                                    auto socketRW = std::make_shared<SocketReaderWriter>(serverSocket);
                                    socketRW->ReadMessage();

                                    winrt::hstring sessionId = socketRW->GetCurrentMessage();
                                    if (!sessionId.empty())
                                    {
                                        LogMessage(L"Connected with remote side on L4 layer");
                                        std::lock_guard<std::mutex> lock(devicesMutex);
                                        for (auto& device : connectedDevices)
                                        {
                                            if (device->DisplayName == L"Waiting for client to connect...")
                                            {
                                                device->DisplayName = std::wstring(sessionId.c_str());
                                                device->SocketRW = socketRW;
                                                break;
                                            }
                                        }
                                    }

                                    // Start continuous reading in background
                                    socketRW->StartReading();
                                }
                                catch (const winrt::hresult_error& e)
                                {
                                    LogMessage(L"Connection failed: " + std::wstring(e.message().c_str()), true);
                                }
                            });

                            listenerSocket.BindEndpointAsync(
                                endpointPairs.GetAt(0).LocalHostName(), strServerPort).get();

                            LogMessage(L"Devices connected on L2, listening on IP Address: " +
                                std::wstring(endpointPairs.GetAt(0).LocalHostName().ToString().c_str()) +
                                L" Port: " + std::wstring(strServerPort.c_str()));
                        }
                        catch (const winrt::hresult_error& e)
                        {
                            LogMessage(L"Connect operation threw an exception: " + std::wstring(e.message().c_str()), true);
                        }
                    });
                }

                publisher.Advertisement().IsAutonomousGroupOwnerEnabled(isGO);

                switch (listenStateMode)
                {
                case 0:
                    publisher.Advertisement().ListenStateDiscoverability(WiFiDirectAdvertisementListenStateDiscoverability::Normal);
                    break;
                case 1:
                    publisher.Advertisement().ListenStateDiscoverability(WiFiDirectAdvertisementListenStateDiscoverability::Intensive);
                    break;
                case 2:
                    publisher.Advertisement().ListenStateDiscoverability(WiFiDirectAdvertisementListenStateDiscoverability::None);
                    break;
                }

                statusChangedToken = publisher.StatusChanged(
                    [](WiFiDirectAdvertisementPublisher const& /* sender */,
                       WiFiDirectAdvertisementPublisherStatusChangedEventArgs const& args)
                {
                    LogMessage(L"Advertisement Status: " +
                        std::to_wstring(static_cast<int>(args.Status())) +
                        L" Error: " + std::to_wstring(static_cast<int>(args.Error())));
                });

                publisher.Start();
                advertisementStarted = true;

                LogMessage(L"Advertisement started, waiting for StatusChanged callback...");
            }
            catch (const winrt::hresult_error& e)
            {
                LogMessage(L"Error starting Advertisement: " + std::wstring(e.message().c_str()), true);
            }
            break;
        }

        case 2: // Stop Advertisement
        {
            try
            {
                if (publisher)
                {
                    publisher.Stop();
                    publisher.StatusChanged(statusChangedToken);
                    advertisementStarted = false;
                    LogMessage(L"Advertisement stopped successfully");
                }

                if (listener)
                {
                    listener.ConnectionRequested(connectionRequestedToken);
                }
            }
            catch (const winrt::hresult_error& e)
            {
                LogMessage(L"Error stopping Advertisement: " + std::wstring(e.message().c_str()), true);
            }
            break;
        }

        case 3: // Add IE
        {
            try
            {
                if (!publisher)
                {
                    publisher = WiFiDirectAdvertisementPublisher();
                }

                std::wcout << L"Enter Information Element text: ";
                std::wstring ieText;
                std::getline(std::wcin, ieText);

                if (ieText.empty())
                {
                    LogMessage(L"Please specify an IE", true);
                    break;
                }

                WiFiDirectInformationElement ie;

                // IE blob
                DataWriter dataWriter;
                dataWriter.UnicodeEncoding(winrt::Windows::Storage::Streams::UnicodeEncoding::Utf8);
                dataWriter.ByteOrder(winrt::Windows::Storage::Streams::ByteOrder::LittleEndian);
                winrt::hstring ieStr(ieText);
                dataWriter.WriteUInt32(dataWriter.MeasureString(ieStr));
                dataWriter.WriteString(ieStr);
                ie.Value(dataWriter.DetachBuffer());

                // OUI
                DataWriter dataWriterOUI;
                dataWriterOUI.WriteBytes(CustomOui);
                ie.Oui(dataWriterOUI.DetachBuffer());

                // OUI Type
                ie.OuiType(CustomOuiType);

                publisher.Advertisement().InformationElements().Append(ie);

                LogMessage(L"IE added successfully");
            }
            catch (const winrt::hresult_error& e)
            {
                LogMessage(L"Error adding IE: " + std::wstring(e.message().c_str()), true);
            }
            break;
        }

        case 4: // Send Message
        {
            std::lock_guard<std::mutex> lock(devicesMutex);
            if (connectedDevices.empty())
            {
                LogMessage(L"No connected devices", true);
                break;
            }

            // List devices
            for (size_t i = 0; i < connectedDevices.size(); i++)
            {
                std::wcout << L"  " << i << L". " << connectedDevices[i]->DisplayName << std::endl;
            }

            std::wcout << L"Select device index: ";
            size_t idx = 0;
            std::wcin >> idx;
            std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');

            if (idx >= connectedDevices.size())
            {
                LogMessage(L"Invalid index", true);
                break;
            }

            std::wcout << L"Enter message: ";
            std::wstring msg;
            std::getline(std::wcin, msg);

            if (msg.empty())
            {
                LogMessage(L"Please type a message to send", true);
                break;
            }

            try
            {
                if (connectedDevices[idx]->SocketRW)
                {
                    connectedDevices[idx]->SocketRW->WriteMessage(winrt::hstring(msg));
                }
                else
                {
                    LogMessage(L"Socket not ready for this device", true);
                }
            }
            catch (const winrt::hresult_error& e)
            {
                LogMessage(L"Send threw an exception: " + std::wstring(e.message().c_str()), true);
            }
            break;
        }

        case 5: // List connected devices
        {
            std::lock_guard<std::mutex> lock(devicesMutex);
            if (connectedDevices.empty())
            {
                LogMessage(L"No connected devices");
                break;
            }
            for (size_t i = 0; i < connectedDevices.size(); i++)
            {
                std::wcout << L"  " << i << L". " << connectedDevices[i]->DisplayName << std::endl;
            }
            break;
        }

        case 6: // Close a device
        {
            std::lock_guard<std::mutex> lock(devicesMutex);
            if (connectedDevices.empty())
            {
                LogMessage(L"No connected devices to close", true);
                break;
            }

            for (size_t i = 0; i < connectedDevices.size(); i++)
            {
                std::wcout << L"  " << i << L". " << connectedDevices[i]->DisplayName << std::endl;
            }

            std::wcout << L"Select device index to close: ";
            size_t idx = 0;
            std::wcin >> idx;
            std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');

            if (idx >= connectedDevices.size())
            {
                LogMessage(L"Invalid index", true);
                break;
            }

            try
            {
                auto& device = connectedDevices[idx];
                if (device->SocketRW)
                {
                    device->SocketRW->Close();
                }
                device->WfdDevice.Close();
                LogMessage(device->DisplayName + L" closed successfully");
                connectedDevices.erase(connectedDevices.begin() + idx);
            }
            catch (const winrt::hresult_error& e)
            {
                LogMessage(L"Close threw an exception: " + std::wstring(e.message().c_str()), true);
            }
            break;
        }

        case 0: // Back
            // Cleanup
            if (advertisementStarted && publisher)
            {
                try
                {
                    publisher.Stop();
                }
                catch (...) {}
            }
            return;

        default:
            std::wcout << L"Invalid choice." << std::endl;
            break;
        }
    }
}
