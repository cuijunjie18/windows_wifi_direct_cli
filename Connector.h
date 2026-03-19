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

// Connector Mode - Discovers and connects to Wi-Fi Direct devices
// Converted from Scenario2_Connector.xaml.cpp (C++/CX) to C++/WinRT CLI

#pragma once

#include "pch.h"
#include "SocketReaderWriter.h"

using namespace winrt;
using namespace winrt::Windows::Devices::WiFiDirect;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Networking::Sockets;
using namespace winrt::Windows::Storage::Streams;

inline void PrintConnectorMenu()
{
    std::wcout << L"\n--- Connector Mode ---" << std::endl;
    std::wcout << L"  1. Start Device Watcher (discover devices)" << std::endl;
    std::wcout << L"  2. Stop Device Watcher" << std::endl;
    std::wcout << L"  3. List discovered devices" << std::endl;
    std::wcout << L"  4. Show Information Elements of a device" << std::endl;
    std::wcout << L"  5. Connect to a device" << std::endl;
    std::wcout << L"  6. Send Message to connected device" << std::endl;
    std::wcout << L"  7. List connected devices" << std::endl;
    std::wcout << L"  8. Close a connected device" << std::endl;
    std::wcout << L"  0. Back to main menu" << std::endl;
    std::wcout << L"Select: ";
}

inline void RunConnector()
{
    std::vector<std::shared_ptr<DiscoveredDevice>> discoveredDevices;
    std::vector<std::shared_ptr<ConnectedDevice>> connectedDevices;
    std::mutex discoveryMutex;
    std::mutex connectedMutex;

    DeviceWatcher deviceWatcher{ nullptr };
    bool watcherStarted = false;
    int goIntent = 0;

    winrt::event_token deviceAddedToken;
    winrt::event_token deviceRemovedToken;
    winrt::event_token deviceUpdatedToken;
    winrt::event_token enumerationCompletedToken;
    winrt::event_token stoppedToken;

    while (true)
    {
        PrintConnectorMenu();

        int choice = 0;
        std::wcin >> choice;
        std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');

        switch (choice)
        {
        case 1: // Start Watcher
        {
            if (watcherStarted)
            {
                LogMessage(L"Watcher is already running. Stop it first.", true);
                break;
            }

            try
            {
                std::wcout << L"Device selector type (0=DeviceInterface, 1=AssociationEndpoint): ";
                int selectorType = 0;
                std::wcin >> selectorType;
                std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');

                {
                    std::lock_guard<std::mutex> lock(discoveryMutex);
                    discoveredDevices.clear();
                }

                LogMessage(L"Finding Devices...");

                auto selectorTypeEnum = (selectorType == 0) ?
                    WiFiDirectDeviceSelectorType::DeviceInterface :
                    WiFiDirectDeviceSelectorType::AssociationEndpoint;

                auto deviceSelector = WiFiDirectDevice::GetDeviceSelector(selectorTypeEnum);

                // Request WiFi Direct Information Elements property
                auto requestedProperties = winrt::single_threaded_vector<winrt::hstring>();
                requestedProperties.Append(L"System.Devices.WiFiDirect.InformationElements");

                deviceWatcher = DeviceInformation::CreateWatcher(deviceSelector, requestedProperties);

                deviceAddedToken = deviceWatcher.Added(
                    [&discoveredDevices, &discoveryMutex](DeviceWatcher const& /* sender */, DeviceInformation const& info)
                {
                    std::lock_guard<std::mutex> lock(discoveryMutex);
                    discoveredDevices.push_back(std::make_shared<DiscoveredDevice>(info));
                    LogMessage(L"Device found: " + std::wstring(info.Name().c_str()));
                });

                deviceRemovedToken = deviceWatcher.Removed(
                    [&discoveredDevices, &discoveryMutex](DeviceWatcher const& /* sender */, DeviceInformationUpdate const& update)
                {
                    std::lock_guard<std::mutex> lock(discoveryMutex);
                    auto it = std::remove_if(discoveredDevices.begin(), discoveredDevices.end(),
                        [&update](const std::shared_ptr<DiscoveredDevice>& d)
                    {
                        return d->DeviceInfo.Id() == update.Id();
                    });
                    if (it != discoveredDevices.end())
                    {
                        LogMessage(L"Device removed: " + std::wstring(update.Id().c_str()));
                        discoveredDevices.erase(it, discoveredDevices.end());
                    }
                });

                deviceUpdatedToken = deviceWatcher.Updated(
                    [&discoveredDevices, &discoveryMutex](DeviceWatcher const& /* sender */, DeviceInformationUpdate const& update)
                {
                    std::lock_guard<std::mutex> lock(discoveryMutex);
                    for (auto& device : discoveredDevices)
                    {
                        if (device->DeviceInfo.Id() == update.Id())
                        {
                            device->DeviceInfo.Update(update);
                            break;
                        }
                    }
                });

                enumerationCompletedToken = deviceWatcher.EnumerationCompleted(
                    [](DeviceWatcher const& /* sender */, auto const& /* args */)
                {
                    LogMessage(L"DeviceWatcher enumeration completed");
                });

                stoppedToken = deviceWatcher.Stopped(
                    [](DeviceWatcher const& /* sender */, auto const& /* args */)
                {
                    LogMessage(L"DeviceWatcher stopped");
                });

                deviceWatcher.Start();
                watcherStarted = true;
                LogMessage(L"Device watcher started.");
            }
            catch (const winrt::hresult_error& e)
            {
                LogMessage(L"Error starting watcher: " + std::wstring(e.message().c_str()), true);
            }
            break;
        }

        case 2: // Stop Watcher
        {
            if (!watcherStarted || !deviceWatcher)
            {
                LogMessage(L"Watcher is not running.", true);
                break;
            }

            try
            {
                deviceWatcher.Added(deviceAddedToken);
                deviceWatcher.Removed(deviceRemovedToken);
                deviceWatcher.Updated(deviceUpdatedToken);
                deviceWatcher.EnumerationCompleted(enumerationCompletedToken);
                deviceWatcher.Stopped(stoppedToken);
                deviceWatcher.Stop();
                watcherStarted = false;
                LogMessage(L"Device watcher stopped.");
            }
            catch (const winrt::hresult_error& e)
            {
                LogMessage(L"Error stopping watcher: " + std::wstring(e.message().c_str()), true);
            }
            break;
        }

        case 3: // List discovered devices
        {
            std::lock_guard<std::mutex> lock(discoveryMutex);
            if (discoveredDevices.empty())
            {
                LogMessage(L"No devices discovered yet.");
                break;
            }
            std::wcout << L"\nDiscovered Devices:" << std::endl;
            for (size_t i = 0; i < discoveredDevices.size(); i++)
            {
                std::wcout << L"  " << i << L". " << discoveredDevices[i]->DisplayName
                           << L" [" << std::wstring(discoveredDevices[i]->DeviceInfo.Id().c_str()) << L"]" << std::endl;
            }
            break;
        }

        case 4: // Show IEs
        {
            std::lock_guard<std::mutex> lock(discoveryMutex);
            if (discoveredDevices.empty())
            {
                LogMessage(L"No devices discovered. Start watcher first.", true);
                break;
            }

            for (size_t i = 0; i < discoveredDevices.size(); i++)
            {
                std::wcout << L"  " << i << L". " << discoveredDevices[i]->DisplayName << std::endl;
            }

            std::wcout << L"Select device index: ";
            size_t idx = 0;
            std::wcin >> idx;
            std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');

            if (idx >= discoveredDevices.size())
            {
                LogMessage(L"Invalid index", true);
                break;
            }

            try
            {
                auto ieCollection = WiFiDirectInformationElement::CreateFromDeviceInformation(
                    discoveredDevices[idx]->DeviceInfo);

                if (ieCollection.Size() == 0)
                {
                    LogMessage(L"No Information Elements found");
                    break;
                }

                std::wstring strIeOutput;
                for (uint32_t i = 0; i < ieCollection.Size(); i++)
                {
                    auto ie = ieCollection.GetAt(i);
                    std::wstring strIe = L"N/A";

                    // Read OUI bytes to compare
                    auto ouiBuf = ie.Oui();
                    auto ouiReader = DataReader::FromBuffer(ouiBuf);
                    std::array<uint8_t, 3> ouiBytes{};
                    if (ouiBuf.Length() >= 3)
                    {
                        ouiReader.ReadBytes(ouiBytes);
                    }

                    if (ouiBytes == MsftOui)
                    {
                        strIeOutput += L"Microsoft IE: ";
                    }
                    else if (ouiBytes == WfaOui)
                    {
                        strIeOutput += L"WFA IE: ";
                    }
                    else if (ouiBytes == CustomOui)
                    {
                        strIeOutput += L"Custom IE: ";

                        auto dataReader = DataReader::FromBuffer(ie.Value());
                        dataReader.UnicodeEncoding(winrt::Windows::Storage::Streams::UnicodeEncoding::Utf8);
                        dataReader.ByteOrder(winrt::Windows::Storage::Streams::ByteOrder::LittleEndian);
                        strIe = dataReader.ReadString(dataReader.ReadUInt32()).c_str();
                    }
                    else
                    {
                        strIeOutput += L"Unknown IE: ";
                    }

                    strIeOutput += L"OUI Type: " + std::to_wstring(ie.OuiType()) +
                        L" IE Data: " + strIe + L"\n";
                }

                std::wcout << strIeOutput << std::endl;
            }
            catch (const winrt::hresult_error& e)
            {
                LogMessage(L"No Information Element found: " + std::wstring(e.message().c_str()), true);
            }
            break;
        }

        case 5: // Connect to device
        {
            {
                std::lock_guard<std::mutex> lock(discoveryMutex);
                if (discoveredDevices.empty())
                {
                    LogMessage(L"No devices discovered. Start watcher first.", true);
                    break;
                }

                for (size_t i = 0; i < discoveredDevices.size(); i++)
                {
                    std::wcout << L"  " << i << L". " << discoveredDevices[i]->DisplayName << std::endl;
                }
            }

            std::wcout << L"Select device index to connect: ";
            size_t idx = 0;
            std::wcin >> idx;
            std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');

            std::wcout << L"Group Owner Intent (0-15): ";
            std::wcin >> goIntent;
            std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');

            std::shared_ptr<DiscoveredDevice> targetDevice;
            {
                std::lock_guard<std::mutex> lock(discoveryMutex);
                if (idx >= discoveredDevices.size())
                {
                    LogMessage(L"Invalid index", true);
                    break;
                }
                targetDevice = discoveredDevices[idx];
            }

            LogMessage(L"Connecting to " + targetDevice->DisplayName + L"...");

            try
            {
                WiFiDirectConnectionParameters connectionParams;
                connectionParams.GroupOwnerIntent(static_cast<int16_t>(goIntent));

                auto wfdDevice = WiFiDirectDevice::FromIdAsync(
                    targetDevice->DeviceInfo.Id(), connectionParams).get();

                // Register for connection status changed
                wfdDevice.ConnectionStatusChanged(
                    [](WiFiDirectDevice const& sender, auto const& /* args */)
                {
                    LogMessage(L"Connection status changed: " +
                        std::to_wstring(static_cast<int>(sender.ConnectionStatus())));
                });

                auto endpointPairs = wfdDevice.GetConnectionEndpointPairs();

                LogMessage(L"Devices connected on L2 layer, connecting to IP Address: " +
                    std::wstring(endpointPairs.GetAt(0).RemoteHostName().ToString().c_str()) +
                    L" Port: " + std::wstring(strServerPort.c_str()));

                // Wait for server to start listening on a socket
                LogMessage(L"Waiting for server socket (5 seconds)...");
                std::this_thread::sleep_for(std::chrono::seconds(5));

                // Connect to Advertiser on L4 layer
                StreamSocket clientSocket;
                clientSocket.ConnectAsync(endpointPairs.GetAt(0).RemoteHostName(), strServerPort).get();

                auto socketRW = std::make_shared<SocketReaderWriter>(clientSocket);

                std::wstring sessionId = L"Session: " + std::to_wstring(rand());

                auto connDevice = std::make_shared<ConnectedDevice>(sessionId, wfdDevice, socketRW);

                {
                    std::lock_guard<std::mutex> lock(connectedMutex);
                    connectedDevices.push_back(connDevice);
                }

                // Start reading messages
                socketRW->StartReading();

                // Send session ID
                socketRW->WriteMessage(winrt::hstring(sessionId));

                LogMessage(L"Connected with remote side on L4 layer");
            }
            catch (const winrt::hresult_error& e)
            {
                LogMessage(L"Connect operation threw an exception: " + std::wstring(e.message().c_str()), true);
            }
            break;
        }

        case 6: // Send Message
        {
            std::lock_guard<std::mutex> lock(connectedMutex);
            if (connectedDevices.empty())
            {
                LogMessage(L"No connected devices", true);
                break;
            }

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

        case 7: // List connected devices
        {
            std::lock_guard<std::mutex> lock(connectedMutex);
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

        case 8: // Close device
        {
            std::lock_guard<std::mutex> lock(connectedMutex);
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
            // Cleanup: stop watcher if running
            if (watcherStarted && deviceWatcher)
            {
                try
                {
                    deviceWatcher.Stop();
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
