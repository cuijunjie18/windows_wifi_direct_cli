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

// Wi-Fi Direct CLI Tool
// Converted from UWP sample (C++/CX) to C++/WinRT console application
//
// Two modes:
//   advertise - Publish Wi-Fi Direct advertisement and accept connections
//   connect   - Discover and connect to Wi-Fi Direct devices

#include "pch.h"
#include "SocketReaderWriter.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Devices::WiFiDirect;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Networking::Sockets;
using namespace winrt::Windows::Storage::Streams;

// =============================================
// Help
// =============================================
void PrintUsage()
{
    std::wcout << L"=== Wi-Fi Direct CLI Tool ===" << std::endl;
    std::wcout << L"Usage: WiFiDirectCLI.exe <mode>" << std::endl;
    std::wcout << std::endl;
    std::wcout << L"Modes:" << std::endl;
    std::wcout << L"  advertise   - Start as Wi-Fi Direct advertiser (server)" << std::endl;
    std::wcout << L"  connect     - Start as Wi-Fi Direct connector (client)" << std::endl;
    std::wcout << L"  help        - Show this help message" << std::endl;
    std::wcout << std::endl;
}

// =============================================
// Advertiser Mode
// =============================================
void RunAdvertiserMode()
{
    std::wcout << L"\n=== Wi-Fi Direct Advertiser Mode ===" << std::endl;

    // Prompt settings
    std::wcout << L"Listen State (0=Normal, 1=Intensive, 2=None) [0]: ";
    std::wstring input;
    std::getline(std::wcin, input);
    int listenStateMode = input.empty() ? 0 : std::stoi(input);

    std::wcout << L"Enable Autonomous Group Owner mode? (y/n) [y]: ";
    std::getline(std::wcin, input);
    bool isGO = (input.empty() || input[0] != L'n');

    std::wcout << L"Enable Legacy mode for Android compatibility? (y/n) [y]: ";
    std::getline(std::wcin, input);
    bool enableLegacy = (input.empty() || input[0] != L'n');

    std::wcout << L"Passphrase for Legacy mode (empty for auto) []: ";
    std::getline(std::wcin, input);
    std::wstring legacyPassphrase = input;

    std::wcout << L"Enable connection listener? (y/n) [y]: ";
    std::getline(std::wcin, input);
    bool enableListener = (input.empty() || input[0] != L'n');

    std::wcout << L"Group Owner Intent (0-15) [14]: ";
    std::getline(std::wcin, input);
    int16_t goIntent = input.empty() ? 14 : static_cast<int16_t>(std::stoi(input));
    if (goIntent < 0) goIntent = 0;
    if (goIntent > 15) goIntent = 15;

    // Data structures
    std::vector<std::shared_ptr<ConnectedDevice>> connectedDevices;
    std::mutex devicesMutex;

    // Create publisher
    WiFiDirectAdvertisementPublisher publisher;
    publisher.Advertisement().IsAutonomousGroupOwnerEnabled(isGO);

    // Configure Legacy mode for Android compatibility
    if (enableLegacy)
    {
        publisher.Advertisement().LegacySettings().IsEnabled(true);
        if (!legacyPassphrase.empty())
        {
            winrt::Windows::Security::Credentials::PasswordCredential credential;
            credential.Password(winrt::hstring(legacyPassphrase));
            publisher.Advertisement().LegacySettings().Passphrase(credential);
        }
        LogMessage(L"Legacy mode enabled (Android compatibility)");
    }

    switch (listenStateMode)
    {
    case 1:
        publisher.Advertisement().ListenStateDiscoverability(WiFiDirectAdvertisementListenStateDiscoverability::Intensive);
        break;
    case 2:
        publisher.Advertisement().ListenStateDiscoverability(WiFiDirectAdvertisementListenStateDiscoverability::None);
        break;
    default:
        publisher.Advertisement().ListenStateDiscoverability(WiFiDirectAdvertisementListenStateDiscoverability::Normal);
        break;
    }

    auto statusToken = publisher.StatusChanged(
        [](WiFiDirectAdvertisementPublisher const&,
           WiFiDirectAdvertisementPublisherStatusChangedEventArgs const& args)
    {
        std::wstring statusStr;
        switch (args.Status())
        {
        case WiFiDirectAdvertisementPublisherStatus::Created: statusStr = L"Created"; break;
        case WiFiDirectAdvertisementPublisherStatus::Started: statusStr = L"Started"; break;
        case WiFiDirectAdvertisementPublisherStatus::Stopped: statusStr = L"Stopped"; break;
        case WiFiDirectAdvertisementPublisherStatus::Aborted: statusStr = L"Aborted"; break;
        default: statusStr = L"Unknown(" + std::to_wstring(static_cast<int>(args.Status())) + L")"; break;
        }
        LogMessage(L"Advertisement Status: " + statusStr +
            L" Error: " + std::to_wstring(static_cast<int>(args.Error())));
    });

    // Connection listener
    WiFiDirectConnectionListener listener{ nullptr };
    winrt::event_token connectionToken{};
    StreamSocketListener listenerSocket{ nullptr };

    if (enableListener)
    {
        listener = WiFiDirectConnectionListener();

        connectionToken = listener.ConnectionRequested(
            [&connectedDevices, &devicesMutex, &listenerSocket, goIntent](
                WiFiDirectConnectionListener const&,
                WiFiDirectConnectionRequestedEventArgs const& args)
        {
            // Run connection handling in a separate thread to avoid blocking the event handler
            std::thread([&connectedDevices, &devicesMutex, &listenerSocket, goIntent, args]()
            {
            try
            {
                auto connectionRequest = args.GetConnectionRequest();
                std::wstring deviceName = connectionRequest.DeviceInformation().Name().c_str();
                std::wstring deviceId = connectionRequest.DeviceInformation().Id().c_str();
                LogMessage(L"Connection request received from " + deviceName);
                LogMessage(L"Device ID: " + deviceId);
                LogMessage(L"Auto-accepting connection...");

                WiFiDirectConnectionParameters connectionParams;
                connectionParams.GroupOwnerIntent(goIntent);

                // Configure preferred pairing procedure for Android compatibility
                // Accept all WPS methods: PBC (Push Button), PIN Display, PIN Keypad
                auto pairingKinds =
                    winrt::Windows::Devices::Enumeration::DevicePairingKinds::None |
                    winrt::Windows::Devices::Enumeration::DevicePairingKinds::ConfirmOnly |
                    winrt::Windows::Devices::Enumeration::DevicePairingKinds::ConfirmPinMatch;
                connectionParams.PreferredPairingProcedure(
                    WiFiDirectPairingProcedure::GroupOwnerNegotiation);

                LogMessage(L"Starting FromIdAsync for device: " + deviceName);

                auto asyncOp = WiFiDirectDevice::FromIdAsync(
                    connectionRequest.DeviceInformation().Id(), connectionParams);

                // Wait with timeout (30 seconds) to avoid hanging indefinitely
                auto status = asyncOp.wait_for(std::chrono::seconds(30));
                if (status != winrt::Windows::Foundation::AsyncStatus::Completed)
                {
                    LogMessage(L"FromIdAsync timed out or failed for " + deviceName +
                        L", status=" + std::to_wstring(static_cast<int>(status)), true);
                    try { asyncOp.Cancel(); } catch (...) {}
                    return;
                }

                auto wfdDevice = asyncOp.GetResults();

                wfdDevice.ConnectionStatusChanged(
                    [](WiFiDirectDevice const& sender, auto const&)
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
                        StreamSocketListener const&,
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
                LogMessage(L"Connect operation threw an exception (HRESULT 0x" +
                    [](HRESULT hr) {
                        wchar_t buf[16];
                        swprintf_s(buf, L"%08X", static_cast<unsigned int>(hr));
                        return std::wstring(buf);
                    }(e.code()) +
                    L"): " + std::wstring(e.message().c_str()), true);
            }
            }).detach();
        });
    }

    // Start advertisement
    try
    {
        publisher.Start();
        LogMessage(L"Advertisement started, waiting for connections...");
    }
    catch (const winrt::hresult_error& e)
    {
        LogMessage(L"Error starting advertisement: " + std::wstring(e.message().c_str()), true);
        return;
    }

    // Interactive command loop
    std::wcout << L"\nCommands:" << std::endl;
    std::wcout << L"  send <index> <message> - Send message to connected device" << std::endl;
    std::wcout << L"  list                   - List connected devices" << std::endl;
    std::wcout << L"  close <index>          - Close connection to device" << std::endl;
    std::wcout << L"  addie <text>           - Add a custom Information Element" << std::endl;
    std::wcout << L"  stop                   - Stop advertisement and exit" << std::endl;
    std::wcout << std::endl;

    while (true)
    {
        std::wcout << L"> ";
        std::wstring cmd;
        std::getline(std::wcin, cmd);

        if (cmd == L"stop" || cmd == L"quit" || cmd == L"exit")
        {
            break;
        }
        else if (cmd == L"list")
        {
            std::lock_guard<std::mutex> lock(devicesMutex);
            if (connectedDevices.empty())
            {
                std::wcout << L"No connected devices." << std::endl;
            }
            else
            {
                for (size_t i = 0; i < connectedDevices.size(); i++)
                {
                    std::wcout << L"  [" << i << L"] " << connectedDevices[i]->DisplayName << std::endl;
                }
            }
        }
        else if (cmd.substr(0, 5) == L"send ")
        {
            std::wistringstream iss(cmd.substr(5));
            int idx = 0;
            iss >> idx;
            std::wstring msg;
            std::getline(iss, msg);
            if (!msg.empty() && msg[0] == L' ') msg = msg.substr(1);

            std::lock_guard<std::mutex> lock(devicesMutex);
            if (idx >= 0 && idx < static_cast<int>(connectedDevices.size()) && connectedDevices[idx]->SocketRW)
            {
                connectedDevices[idx]->SocketRW->WriteMessage(winrt::hstring(msg));
            }
            else
            {
                LogMessage(L"Invalid device index or socket not ready", true);
            }
        }
        else if (cmd.substr(0, 6) == L"close ")
        {
            int idx = std::stoi(cmd.substr(6));
            std::lock_guard<std::mutex> lock(devicesMutex);
            if (idx >= 0 && idx < static_cast<int>(connectedDevices.size()))
            {
                try
                {
                    if (connectedDevices[idx]->SocketRW)
                        connectedDevices[idx]->SocketRW->Close();
                    connectedDevices[idx]->WfdDevice.Close();
                    LogMessage(connectedDevices[idx]->DisplayName + L" closed successfully");
                    connectedDevices.erase(connectedDevices.begin() + idx);
                }
                catch (const winrt::hresult_error& e)
                {
                    LogMessage(L"Close threw an exception: " + std::wstring(e.message().c_str()), true);
                }
            }
            else
            {
                LogMessage(L"Invalid device index", true);
            }
        }
        else if (cmd.substr(0, 6) == L"addie ")
        {
            std::wstring ieText = cmd.substr(6);
            if (ieText.empty())
            {
                LogMessage(L"Please specify IE text", true);
                continue;
            }

            try
            {
                WiFiDirectInformationElement ie;

                DataWriter dataWriter;
                dataWriter.UnicodeEncoding(winrt::Windows::Storage::Streams::UnicodeEncoding::Utf8);
                dataWriter.ByteOrder(winrt::Windows::Storage::Streams::ByteOrder::LittleEndian);
                winrt::hstring ieStr(ieText);
                dataWriter.WriteUInt32(dataWriter.MeasureString(ieStr));
                dataWriter.WriteString(ieStr);
                ie.Value(dataWriter.DetachBuffer());

                DataWriter dataWriterOUI;
                dataWriterOUI.WriteBytes(CustomOui);
                ie.Oui(dataWriterOUI.DetachBuffer());

                ie.OuiType(CustomOuiType);

                publisher.Advertisement().InformationElements().Append(ie);
                LogMessage(L"IE added successfully");
            }
            catch (const winrt::hresult_error& e)
            {
                LogMessage(L"Error adding IE: " + std::wstring(e.message().c_str()), true);
            }
        }
        else if (!cmd.empty())
        {
            std::wcout << L"Unknown command. Use: send, list, close, addie, stop" << std::endl;
        }
    }

    // Cleanup
    try
    {
        publisher.Stop();
        publisher.StatusChanged(statusToken);
        if (listener)
        {
            listener.ConnectionRequested(connectionToken);
        }
    }
    catch (...) {}

    {
        std::lock_guard<std::mutex> lock(devicesMutex);
        for (auto& dev : connectedDevices)
        {
            try
            {
                if (dev->SocketRW) dev->SocketRW->Close();
                dev->WfdDevice.Close();
            }
            catch (...) {}
        }
    }

    LogMessage(L"Advertiser stopped.");
}

// =============================================
// Connector Mode
// =============================================
void RunConnectorMode()
{
    std::wcout << L"\n=== Wi-Fi Direct Connector Mode ===" << std::endl;

    std::wcout << L"Device selector type (0=DeviceInterface, 1=AssociationEndpoint) [0]: ";
    std::wstring input;
    std::getline(std::wcin, input);

    WiFiDirectDeviceSelectorType selectorType = WiFiDirectDeviceSelectorType::DeviceInterface;
    if (!input.empty() && input[0] == L'1')
        selectorType = WiFiDirectDeviceSelectorType::AssociationEndpoint;

    std::wcout << L"Group Owner Intent (0-15) [14]: ";
    std::getline(std::wcin, input);
    int16_t goIntent = input.empty() ? 14 : static_cast<int16_t>(std::stoi(input));
    if (goIntent < 0) goIntent = 0;
    if (goIntent > 15) goIntent = 15;

    // Discovered devices
    std::vector<std::shared_ptr<DiscoveredDevice>> discoveredDevices;
    std::mutex discoveryMutex;

    // Connected devices
    std::vector<std::shared_ptr<ConnectedDevice>> connectedDevices;
    std::mutex connectionMutex;

    // Create device watcher
    auto deviceSelector = WiFiDirectDevice::GetDeviceSelector(selectorType);

    auto requestedProperties = winrt::single_threaded_vector<winrt::hstring>();
    requestedProperties.Append(L"System.Devices.WiFiDirect.InformationElements");

    auto deviceWatcher = DeviceInformation::CreateWatcher(deviceSelector, requestedProperties);

    deviceWatcher.Added([&discoveredDevices, &discoveryMutex](DeviceWatcher const&, DeviceInformation const& info)
    {
        std::lock_guard<std::mutex> lock(discoveryMutex);
        discoveredDevices.push_back(std::make_shared<DiscoveredDevice>(info));
        LogMessage(L"Device found: " + std::wstring(info.Name().c_str()) +
                   L" (Total: " + std::to_wstring(discoveredDevices.size()) + L")");
    });

    deviceWatcher.Removed([&discoveredDevices, &discoveryMutex](DeviceWatcher const&, DeviceInformationUpdate const& update)
    {
        std::lock_guard<std::mutex> lock(discoveryMutex);
        for (auto it = discoveredDevices.begin(); it != discoveredDevices.end(); ++it)
        {
            if ((*it)->DeviceInfo.Id() == update.Id())
            {
                LogMessage(L"Device removed: " + (*it)->DisplayName);
                discoveredDevices.erase(it);
                break;
            }
        }
    });

    deviceWatcher.Updated([&discoveredDevices, &discoveryMutex](DeviceWatcher const&, DeviceInformationUpdate const& update)
    {
        std::lock_guard<std::mutex> lock(discoveryMutex);
        for (auto& dev : discoveredDevices)
        {
            if (dev->DeviceInfo.Id() == update.Id())
            {
                dev->DeviceInfo.Update(update);
                break;
            }
        }
    });

    deviceWatcher.EnumerationCompleted([](DeviceWatcher const&, auto const&)
    {
        LogMessage(L"DeviceWatcher enumeration completed");
    });

    deviceWatcher.Stopped([](DeviceWatcher const&, auto const&)
    {
        LogMessage(L"DeviceWatcher stopped");
    });

    // Start watching
    deviceWatcher.Start();
    LogMessage(L"Watching for Wi-Fi Direct devices...");

    // Interactive command loop
    std::wcout << L"\nCommands:" << std::endl;
    std::wcout << L"  devices                - List discovered devices" << std::endl;
    std::wcout << L"  connect <index>        - Connect to a discovered device" << std::endl;
    std::wcout << L"  send <index> <message> - Send message to connected device" << std::endl;
    std::wcout << L"  list                   - List connected devices" << std::endl;
    std::wcout << L"  close <index>          - Close connection to device" << std::endl;
    std::wcout << L"  ie <index>             - Show Information Elements for device" << std::endl;
    std::wcout << L"  stop                   - Stop watcher and exit" << std::endl;
    std::wcout << std::endl;

    while (true)
    {
        std::wcout << L"> ";
        std::wstring cmd;
        std::getline(std::wcin, cmd);

        if (cmd == L"stop" || cmd == L"quit" || cmd == L"exit")
        {
            break;
        }
        else if (cmd == L"devices")
        {
            std::lock_guard<std::mutex> lock(discoveryMutex);
            if (discoveredDevices.empty())
            {
                std::wcout << L"No devices discovered yet." << std::endl;
            }
            else
            {
                for (size_t i = 0; i < discoveredDevices.size(); i++)
                {
                    std::wcout << L"  [" << i << L"] " << discoveredDevices[i]->DisplayName
                               << L" (" << std::wstring(discoveredDevices[i]->DeviceInfo.Id().c_str()) << L")" << std::endl;
                }
            }
        }
        else if (cmd.substr(0, 8) == L"connect ")
        {
            int idx = std::stoi(cmd.substr(8));
            winrt::hstring deviceId;
            std::wstring deviceName;

            {
                std::lock_guard<std::mutex> lock(discoveryMutex);
                if (idx < 0 || idx >= static_cast<int>(discoveredDevices.size()))
                {
                    LogMessage(L"Invalid device index", true);
                    continue;
                }
                deviceId = discoveredDevices[idx]->DeviceInfo.Id();
                deviceName = discoveredDevices[idx]->DisplayName;
            }

            LogMessage(L"Connecting to " + deviceName + L"...");

            try
            {
                WiFiDirectConnectionParameters connectionParams;
                connectionParams.GroupOwnerIntent(goIntent);

                auto wfdDevice = WiFiDirectDevice::FromIdAsync(deviceId, connectionParams).get();

                wfdDevice.ConnectionStatusChanged([](WiFiDirectDevice const& sender, auto const&)
                {
                    LogMessage(L"Connection status changed: " +
                        std::to_wstring(static_cast<int>(sender.ConnectionStatus())));
                });

                auto endpointPairs = wfdDevice.GetConnectionEndpointPairs();
                auto remoteHost = endpointPairs.GetAt(0).RemoteHostName();

                LogMessage(L"L2 connected, connecting to " +
                    std::wstring(remoteHost.DisplayName().c_str()) +
                    L":" + std::wstring(strServerPort.c_str()));

                // Wait for server to start listening
                LogMessage(L"Waiting for server socket...");
                std::this_thread::sleep_for(std::chrono::seconds(5));

                // Connect L4
                StreamSocket clientSocket;
                clientSocket.ConnectAsync(remoteHost, strServerPort).get();

                auto socketRW = std::make_shared<SocketReaderWriter>(clientSocket);

                std::wstring sessionId = L"Session_" + std::to_wstring(rand());

                auto connDevice = std::make_shared<ConnectedDevice>(sessionId, wfdDevice, socketRW);

                {
                    std::lock_guard<std::mutex> lock(connectionMutex);
                    connectedDevices.push_back(connDevice);
                }

                // Start reading messages
                socketRW->StartReading();

                // Send session ID
                socketRW->WriteMessage(winrt::hstring(sessionId));

                LogMessage(L"Connected on L4 layer, session: " + sessionId);
            }
            catch (const winrt::hresult_error& e)
            {
                LogMessage(L"Connection error: " + std::wstring(e.message().c_str()), true);
            }
        }
        else if (cmd == L"list")
        {
            std::lock_guard<std::mutex> lock(connectionMutex);
            if (connectedDevices.empty())
            {
                std::wcout << L"No connected devices." << std::endl;
            }
            else
            {
                for (size_t i = 0; i < connectedDevices.size(); i++)
                {
                    std::wcout << L"  [" << i << L"] " << connectedDevices[i]->DisplayName << std::endl;
                }
            }
        }
        else if (cmd.substr(0, 5) == L"send ")
        {
            std::wistringstream iss(cmd.substr(5));
            int idx = 0;
            iss >> idx;
            std::wstring msg;
            std::getline(iss, msg);
            if (!msg.empty() && msg[0] == L' ') msg = msg.substr(1);

            std::lock_guard<std::mutex> lock(connectionMutex);
            if (idx >= 0 && idx < static_cast<int>(connectedDevices.size()) && connectedDevices[idx]->SocketRW)
            {
                connectedDevices[idx]->SocketRW->WriteMessage(winrt::hstring(msg));
            }
            else
            {
                LogMessage(L"Invalid device index or socket not ready", true);
            }
        }
        else if (cmd.substr(0, 6) == L"close ")
        {
            int idx = std::stoi(cmd.substr(6));
            std::lock_guard<std::mutex> lock(connectionMutex);
            if (idx >= 0 && idx < static_cast<int>(connectedDevices.size()))
            {
                try
                {
                    if (connectedDevices[idx]->SocketRW)
                        connectedDevices[idx]->SocketRW->Close();
                    connectedDevices[idx]->WfdDevice.Close();
                    LogMessage(connectedDevices[idx]->DisplayName + L" closed successfully");
                    connectedDevices.erase(connectedDevices.begin() + idx);
                }
                catch (const winrt::hresult_error& e)
                {
                    LogMessage(L"Close threw an exception: " + std::wstring(e.message().c_str()), true);
                }
            }
            else
            {
                LogMessage(L"Invalid device index", true);
            }
        }
        else if (cmd.substr(0, 3) == L"ie ")
        {
            int idx = std::stoi(cmd.substr(3));
            std::lock_guard<std::mutex> lock(discoveryMutex);
            if (idx < 0 || idx >= static_cast<int>(discoveredDevices.size()))
            {
                LogMessage(L"Invalid device index", true);
                continue;
            }

            try
            {
                auto ieCollection = WiFiDirectInformationElement::CreateFromDeviceInformation(
                    discoveredDevices[idx]->DeviceInfo);

                if (ieCollection.Size() == 0)
                {
                    LogMessage(L"No Information Elements found");
                    continue;
                }

                for (uint32_t i = 0; i < ieCollection.Size(); i++)
                {
                    auto ie = ieCollection.GetAt(i);
                    std::wstring prefix = L"Unknown IE: ";

                    // Check OUI
                    auto ouiBuf = ie.Oui();
                    if (ouiBuf)
                    {
                        auto reader = DataReader::FromBuffer(ouiBuf);
                        std::vector<uint8_t> ouiBytes(reader.UnconsumedBufferLength());
                        reader.ReadBytes(ouiBytes);

                        if (ouiBytes.size() == 3)
                        {
                            if (ouiBytes[0] == 0x00 && ouiBytes[1] == 0x50 && ouiBytes[2] == 0xF2)
                                prefix = L"Microsoft IE: ";
                            else if (ouiBytes[0] == 0x50 && ouiBytes[1] == 0x6F && ouiBytes[2] == 0x9A)
                                prefix = L"WFA IE: ";
                            else if (ouiBytes[0] == 0xAA && ouiBytes[1] == 0xBB && ouiBytes[2] == 0xCC)
                            {
                                prefix = L"Custom IE: ";
                                // Read custom IE value
                                auto valBuf = ie.Value();
                                if (valBuf)
                                {
                                    auto valReader = DataReader::FromBuffer(valBuf);
                                    valReader.UnicodeEncoding(winrt::Windows::Storage::Streams::UnicodeEncoding::Utf8);
                                    valReader.ByteOrder(winrt::Windows::Storage::Streams::ByteOrder::LittleEndian);
                                    uint32_t strLen = valReader.ReadUInt32();
                                    winrt::hstring ieData = valReader.ReadString(strLen);
                                    std::wcout << prefix << L"Data: " << ieData.c_str() << std::endl;
                                    continue;
                                }
                            }
                        }
                    }

                    std::wcout << prefix << L"OUI Type: " << ie.OuiType() << std::endl;
                }
            }
            catch (const winrt::hresult_error& e)
            {
                LogMessage(L"Error reading IE: " + std::wstring(e.message().c_str()), true);
            }
        }
        else if (!cmd.empty())
        {
            std::wcout << L"Unknown command. Use: devices, connect, send, list, close, ie, stop" << std::endl;
        }
    }

    // Cleanup
    try
    {
        deviceWatcher.Stop();
    }
    catch (...) {}

    {
        std::lock_guard<std::mutex> lock(connectionMutex);
        for (auto& dev : connectedDevices)
        {
            try
            {
                if (dev->SocketRW) dev->SocketRW->Close();
                dev->WfdDevice.Close();
            }
            catch (...) {}
        }
    }

    LogMessage(L"Connector stopped.");
}

// =============================================
// Main Entry Point
// =============================================
int wmain(int argc, wchar_t* argv[])
{
    // Initialize WinRT apartment
    winrt::init_apartment();

    if (argc < 2)
    {
        PrintUsage();
        winrt::uninit_apartment();
        return 1;
    }

    std::wstring mode(argv[1]);

    if (mode == L"advertise")
    {
        RunAdvertiserMode();
    }
    else if (mode == L"connect")
    {
        RunConnectorMode();
    }
    else if (mode == L"help" || mode == L"--help" || mode == L"-h")
    {
        PrintUsage();
    }
    else
    {
        std::wcerr << L"Unknown mode: " << mode << std::endl;
        PrintUsage();
        winrt::uninit_apartment();
        return 1;
    }

    winrt::uninit_apartment();
    return 0;
}
