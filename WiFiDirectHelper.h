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

// WiFiDirectHelper.h - Helper definitions for the CLI tool

#pragma once

#include <cstdint>
#include <string>

// Custom OUI values for Information Elements
namespace WiFiDirectConst
{
    static const uint8_t CustomOui[] = { 0xAA, 0xBB, 0xCC };
    static const uint8_t CustomOuiType = 0xDD;
    static const uint8_t WfaOui[] = { 0x50, 0x6F, 0x9A };
    static const uint8_t MsftOui[] = { 0x00, 0x50, 0xF2 };
    static const wchar_t* ServerPort = L"50001";
}
