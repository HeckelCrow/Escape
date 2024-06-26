/*
Module : enumser.h
Purpose: Defines the interface for a class to enumerate the serial ports installed on a PC
         using a number of different approaches

Copyright (c) 1998 - 2023 by PJ Naughter (Web: www.naughter.com, Email: pjna@naughter.com)

All rights reserved.

Copyright / Usage Details:

You are allowed to include the source code in any product (commercial, shareware, freeware or otherwise) 
when your product is released in binary form. You are allowed to modify the source code in any way you want 
except you cannot modify the copyright details at the top of each module. If you want to distribute source 
code with your application, then you are only allowed to distribute versions released by the author. This is 
to maintain a single distribution point for the source code. 

*/


///////////////////////// Macros / Defines ////////////////////////////////////

#pragma once

#if _MSVC_LANG < 201703
#error CEnumerateSerial requires a minimum C++ language standard of /std:c++17
#endif //#if _MSVC_LANG < 201703

#ifndef __ENUMSER_H__
#define __ENUMSER_H__

#ifndef CENUMERATESERIAL_EXT_CLASS
#define CENUMERATESERIAL_EXT_CLASS
#endif //#ifndef CENUMERATESERIAL_EXT_CLASS


///////////////////////// Includes ////////////////////////////////////////////

#ifndef __ATLBASE_H__
#pragma message("To avoid this message, please put atlbase.h in your pre compiled header (normally stdafx.h)")
#include <atlbase.h>
#endif //#ifndef __ATLBASE_H__

#ifndef _VECTOR_
#pragma message("To avoid this message, please put vector in your pre compiled header (normally stdafx.h)")
#include <vector>
#endif //#ifndef _VECTOR_

#ifndef _STRING_
#pragma message("To avoid this message, please put string in your pre compiled header (normally stdafx.h)")
#include <string>
#endif //#ifndef _STRING_

#ifndef _UTILITY_
#pragma message("To avoid this message, please put utility in your pre compiled header (normally stdafx.h)")
#include <utility>
#endif //#ifndef _UTILITY_

#ifndef __ATLSTR_H__
#pragma message("To avoid this message, please put atlstr.h in your pre compiled header (normally stdafx.h)")
#include <atlstr.h>
#endif //#ifndef __ATLSTR_H__

#if !defined(NO_CENUMERATESERIAL_USING_SETUPAPI1) || !defined(NO_CENUMERATESERIAL_USING_SETUPAPI2)
#include <winioctl.h>

#ifndef _INC_SETUPAPI
#pragma message("To avoid this message, please put setupapi.h in your pre compiled header (normally stdafx.h)")
#include <setupapi.h>
#endif //#ifndef _INC_SETUPAPI

#endif //#if !defined(NO_CENUMERATESERIAL_USING_SETUPAPI1) || !defined(NO_CENUMERATESERIAL_USING_SETUPAPI2)


///////////////////////// Classes /////////////////////////////////////////////

class CENUMERATESERIAL_EXT_CLASS CEnumerateSerial
{
public:
//Typedefs
  using CPortsArray = std::vector<UINT>;
#ifdef _UNICODE
  using String = std::wstring;
#else
  using String = std::string;
#endif
  using CNamesArray = std::vector<String>;
  using CPortAndNamesArray = std::vector<std::pair<UINT, String>>;

//Methods
#ifndef NO_CENUMERATESERIAL_USING_CREATEFILE
  static _Return_type_success_(return != false) bool UsingCreateFile(_Inout_ CPortsArray& ports);
#endif //#ifndef NO_CENUMERATESERIAL_USING_CREATEFILE

#ifndef NO_CENUMERATESERIAL_USING_QUERYDOSDEVICE
  static _Return_type_success_(return != false) bool UsingQueryDosDevice(_Inout_ CPortsArray& ports);
#endif //#ifndef NO_CENUMERATESERIAL_USING_QUERYDOSDEVICE

#ifndef NO_CENUMERATESERIAL_USING_GETDEFAULTCOMMCONFIG
  static _Return_type_success_(return != false) bool UsingGetDefaultCommConfig(_Inout_ CPortsArray& ports);
#endif //#ifndef NO_CENUMERATESERIAL_USING_GETDEFAULTCOMMCONFIG

#ifndef NO_CENUMERATESERIAL_USING_SETUPAPI1
  static _Return_type_success_(return != false) bool UsingSetupAPI1(_Inout_ CPortAndNamesArray& ports);
#endif //#ifndef NO_CENUMERATESERIAL_USING_SETUPAPI1

#ifndef NO_CENUMERATESERIAL_USING_SETUPAPI2
  static _Return_type_success_(return != false) bool UsingSetupAPI2(_Inout_ CPortAndNamesArray& ports);
#endif //#ifndef NO_CENUMERATESERIAL_USING_SETUPAPI2

#ifndef NO_CENUMERATESERIAL_USING_ENUMPORTS
  static _Return_type_success_(return != false) bool UsingEnumPorts(_Inout_ CPortAndNamesArray& ports);
#endif //#ifndef NO_CENUMERATESERIAL_USING_ENUMPORTS

#ifndef NO_CENUMERATESERIAL_USING_WMI
  static HRESULT UsingWMI(_Inout_ CPortAndNamesArray& ports);
#endif //#ifndef NO_CENUMERATESERIAL_USING_WMI

#ifndef NO_CENUMERATESERIAL_USING_COMDB
  static _Return_type_success_(return != false) bool UsingComDB(_Inout_ CPortsArray& ports);
#endif //#ifndef NO_CENUMERATESERIAL_USING_COMDB

#ifndef NO_CENUMERATESERIAL_USING_REGISTRY
  static _Return_type_success_(return != false) bool UsingRegistry(_Inout_ CNamesArray& ports);
#endif //#ifndef NO_CENUMERATESERIAL_USING_REGISTRY

#ifndef NO_CENUMERATESERIAL_USING_GETCOMMPORTS
  static _Return_type_success_(return != false) bool UsingGetCommPorts(_Inout_ CPortsArray& ports);
#endif //#ifndef NO_CENUMERATESERIAL_USING_REGISTRY

protected:
//Methods
#if !defined(NO_CENUMERATESERIAL_USING_SETUPAPI1) || !defined(NO_CENUMERATESERIAL_USING_SETUPAPI2)
  static _Return_type_success_(return != false) bool RegQueryValueString(_In_ ATL::CRegKey& key, _In_ LPCTSTR lpValueName, _Inout_ String& sValue);
  static _Return_type_success_(return != false) bool QueryRegistryPortName(_In_ ATL::CRegKey& deviceKey, _Out_ int& nPort);
  static _Return_type_success_(return != false) bool QueryUsingSetupAPI(_In_ const GUID& guid, _In_ DWORD dwFlags, _Inout_ CPortAndNamesArray& ports);
  static _Return_type_success_(return != false) bool QueryDeviceDescription(_In_ HDEVINFO hDevInfoSet, _In_ SP_DEVINFO_DATA& devInfo, _Inout_ String& sFriendlyName);
#endif //#if !defined(NO_CENUMERATESERIAL_USING_SETUPAPI1) || !defined(NO_CENUMERATESERIAL_USING_SETUPAPI2)
  static _Return_type_success_(return != false) bool IsNumeric(_In_z_ LPCSTR pszString, _In_ bool bIgnoreColon) noexcept;
  static _Return_type_success_(return != false) bool IsNumeric(_In_z_ LPCWSTR pszString, _In_ bool bIgnoreColon) noexcept;
};

#endif //#ifndef __ENUMSER_H__
