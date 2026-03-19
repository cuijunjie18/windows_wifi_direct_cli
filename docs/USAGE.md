# Wi-Fi Direct CLI Tool - Usage Guide

## Overview

`WiFiDirectCLI.exe` is a command-line tool for Windows that provides Wi-Fi Direct
peer-to-peer connectivity capabilities:

- **Advertiser mode**: Publish a Wi-Fi Direct advertisement, accept incoming connections,
  and exchange messages with connected devices.
- **Connector mode**: Discover nearby Wi-Fi Direct devices, connect to them,
  and exchange messages.

This tool was converted from the Microsoft UWP Wi-Fi Direct C++/CX sample,
replacing the XAML graphical interface with an interactive command-line interface.

## Quick Start

```cmd
WiFiDirectCLI.exe advertise    # Run as advertiser (server)
WiFiDirectCLI.exe connect      # Run as connector (client)
WiFiDirectCLI.exe help         # Show help
```

## Advertiser Mode

### Startup Options

When starting advertiser mode, you will be prompted:

| Prompt | Description | Default |
|--------|-------------|---------|
| Listen State | Discoverability level: 0=Normal, 1=Intensive, 2=None | 0 (Normal) |
| Autonomous GO | Enable autonomous group owner mode | No |
| Connection Listener | Accept incoming connections | Yes |
| GO Intent | Group owner intent (0-15, higher = prefer to be GO) | 14 |

### Interactive Commands

| Command | Description |
|---------|-------------|
| `list` | List all connected devices |
| `send <idx> <msg>` | Send a text message to device at index `<idx>` |
| `close <idx>` | Disconnect device at index `<idx>` |
| `addie <text>` | Add a custom Information Element to the advertisement |
| `stop` | Stop advertising and exit |

### Example Session

```
> WiFiDirectCLI.exe advertise

=== Wi-Fi Direct Advertiser Mode ===
Listen State (0=Normal, 1=Intensive, 2=None) [0]: 0
Enable Autonomous Group Owner mode? (y/n) [n]: n
Enable connection listener? (y/n) [y]: y
Group Owner Intent (0-15) [14]: 14
[INFO]  Advertisement started, waiting for connections...

Commands:
  send <index> <message> - Send message to connected device
  list                   - List connected devices
  close <index>          - Close connection to device
  addie <text>           - Add a custom Information Element
  stop                   - Stop advertisement and exit

[INFO]  Connection request received from DESKTOP-ABC123
[INFO]  Auto-accepting connection...
[INFO]  Devices connected on L2, listening on IP Address: 192.168.49.1 Port: 50001
[INFO]  Connecting to remote side on L4 layer...
[INFO]  Connected with remote side on L4 layer

> list
  [0] Session_12345

> send 0 Hello from advertiser!
[INFO]  Sent message: Hello from advertiser!

> stop
[INFO]  Advertiser stopped.
```

## Connector Mode

### Startup Options

| Prompt | Description | Default |
|--------|-------------|---------|
| Selector Type | 0=Device Interface, 1=Association Endpoint | 0 |
| GO Intent | Group owner intent (0-15) | 14 |

### Interactive Commands

| Command | Description |
|---------|-------------|
| `devices` | List all discovered Wi-Fi Direct devices |
| `connect <idx>` | Connect to discovered device at index `<idx>` |
| `list` | List all active connections |
| `send <idx> <msg>` | Send a text message to connected device |
| `close <idx>` | Disconnect device at index `<idx>` |
| `ie <idx>` | Show Information Elements for discovered device |
| `stop` | Stop device watcher and exit |

### Example Session

```
> WiFiDirectCLI.exe connect

=== Wi-Fi Direct Connector Mode ===
Device selector type (0=DeviceInterface, 1=AssociationEndpoint) [0]: 0
Group Owner Intent (0-15) [14]: 14
[INFO]  Watching for Wi-Fi Direct devices...

[INFO]  Device found: MyLaptop-Direct (Total: 1)
[INFO]  Device found: Phone-WiFiDirect (Total: 2)
[INFO]  DeviceWatcher enumeration completed

> devices
  [0] MyLaptop-Direct (WiFiDirect#xxx...)
  [1] Phone-WiFiDirect (WiFiDirect#yyy...)

> connect 0
[INFO]  Connecting to MyLaptop-Direct...
[INFO]  L2 connected, connecting to 192.168.49.1:50001
[INFO]  Waiting for server socket...
[INFO]  Sent message: Session_54321
[INFO]  Connected on L4 layer, session: Session_54321

> send 0 Hello from connector!
[INFO]  Sent message: Hello from connector!

> ie 1
Custom IE: Data: my custom data

> stop
[INFO]  Connector stopped.
```

## Typical Test Workflow

To test Wi-Fi Direct between two Windows machines:

1. **Machine A** (the advertiser/server):
   ```cmd
   WiFiDirectCLI.exe advertise
   ```

2. **Machine B** (the connector/client):
   ```cmd
   WiFiDirectCLI.exe connect
   ```

3. On Machine B, type `devices` to see Machine A, then `connect 0`
4. Machine A auto-accepts and sets up a TCP socket on port 50001
5. Use `send <idx> <message>` on either machine to exchange messages
6. Use `close <idx>` to disconnect, or `stop` to exit

## System Requirements

- **OS**: Windows 10 (Build 17763+) or Windows 11
- **Hardware**: Wi-Fi adapter with Wi-Fi Direct support
- **Permissions**: Administrator privileges may be required

## Troubleshooting

| Issue | Solution |
|-------|----------|
| No devices discovered | Ensure Wi-Fi is enabled; the other device must be advertising |
| Connection error | Check firewall; ensure port 50001 is not blocked |
| `FromIdAsync` failed | Try running as Administrator |
| Advertisement status error | Wi-Fi adapter may not support Wi-Fi Direct |
| Timeout waiting for socket | Increase wait time or check network connectivity |

## Architecture Mapping

```
Original UWP App                 -->  CLI Tool
-----------------------------------------------------------
XAML pages + event handlers      -->  stdin/stdout interactive commands
C++/CX language extensions       -->  C++/WinRT (standard C++17)
Visual Studio .sln/.vcxproj      -->  cl.exe command-line build (build.bat)
MainPage::NotifyUser()           -->  LogMessage() -> wcout/wcerr
MessageDialog (accept/decline)   -->  Auto-accept (logged to console)
ListView (device lists)          -->  std::vector + index-based selection
Button click handlers            -->  Text command parser in while loop
```
