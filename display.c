#include "shared.h"
#include <windows.h>
#include <cfgmgr32.h>
#include <assert.h>
#include <stdbool.h>
#include <initguid.h>
#include <Devpkey.h>
#include <devguid.h>

// This GUID was only added to devguid.h on Windows SDK v10.0.16232 which
// corresponds to Windows 10 Redstone 3 (Windows 10 Fall Creators Update).
#ifndef GUID_DEVCLASS_SOFTWARECOMPONENT
DEFINE_GUID(GUID_DEVCLASS_SOFTWARECOMPONENT, 0x5c4c3332, 0x344d, 0x483c, 0x87, 0x39, 0x25, 0x9e, 0x93, 0x4c, 0x9c, 0xc8);
#endif

#pragma comment(lib, "Cfgmgr32.lib")

typedef enum
{
    ProbeFailure,
    PendingReboot,
    Valid
} DeviceProbeResult;

#ifdef _WIN64
const wchar_t OPENCL_REG_SUB_KEY[] = L"OpenCLDriverName";
#else
const wchar_t OPENCL_REG_SUB_KEY[] = L"OpenCLDriverNameWow";
#endif

static bool ReadOpenCLKey(DEVINST dnDevNode)
{
    HKEY hkey = 0;
    CONFIGRET ret;
    bool bRet = false;
    DWORD dwLibraryNameType = 0;
    wchar_t *wcszOclPath = NULL;
    DWORD dwOclPathSize = 0;
    LSTATUS result;

    ret = CM_Open_DevNode_Key(
        dnDevNode,
        KEY_QUERY_VALUE,
        0,
        RegDisposition_OpenExisting,
        &hkey,
        CM_REGISTRY_SOFTWARE);

    if (CR_SUCCESS == ret)
    {
        result = RegQueryValueEx(
            hkey,
            OPENCL_REG_SUB_KEY,
            NULL,
            &dwLibraryNameType,
            NULL,
            &dwOclPathSize);

        if (ERROR_SUCCESS != result)
        {
            OCL_ENUM_TRACE(TEXT("Failed to open sub key\n"));
            goto out;
        }

        wcszOclPath = malloc(dwOclPathSize);
        if (NULL == wcszOclPath)
        {
            OCL_ENUM_TRACE(TEXT("Failed to allocate %u bytes for registry value\n"), dwOclPathSize);
            goto out;
        }

        result = RegQueryValueEx(
            hkey,
            OPENCL_REG_SUB_KEY,
            NULL,
            &dwLibraryNameType,
            (LPBYTE)wcszOclPath,
            &dwOclPathSize);
        if (ERROR_SUCCESS != result)
        {
            OCL_ENUM_TRACE(TEXT("Failed to open sub key\n"));
            goto out;
        }

        if (REG_MULTI_SZ != dwLibraryNameType)
        {
            OCL_ENUM_TRACE(TEXT("Unexpected registry entry! continuing\n"));
            goto out;
        }

        wprintf_s(L"    Path: %ls\n", wcszOclPath);

        bRet = true;
    }

out:
    free(wcszOclPath);

    if (hkey)
    {
        result = RegCloseKey(hkey);
        if (ERROR_SUCCESS != result)
        {
            OCL_ENUM_TRACE(TEXT("WARNING: failed to close hkey\n"));
        }
    }

    return bRet;
}

static DeviceProbeResult ProbeDevice(DEVINST devnode)
{
    CONFIGRET ret;
    ULONG ulStatus;
    ULONG ulProblem;

    ret = CM_Get_DevNode_Status(
        &ulStatus,
        &ulProblem,
        devnode,
        0);

    // TODO: consider extracting warning messages out of this function
    if (CR_SUCCESS != ret)
    {
        wprintf_s(L"    WARNING: failed to probe the status of the device");
        return ProbeFailure;
    }

    //
    // Careful here, we need to check 2 scenarios:
    // 1. DN_NEED_RESTART
    //    status flag indicates that a reboot is needed when an _already started_
    //    device cannot be stopped. This covers devices that are still started with their
    //    old KMD (because they couldn't be stopped/restarted) while the UMD is updated
    //    and possibly out of sync.
    //
    // 2.  Status & DN_HAS_PROBLEM  && Problem == CM_PROB_NEED_RESTART
    //     indicates that a reboot is needed when a _stopped device_ cannot be (re)started.
    //
    if (((ulStatus & DN_HAS_PROBLEM) && ulProblem == CM_PROB_NEED_RESTART) ||
          ulStatus & DN_NEED_RESTART)
    {
        wprintf_s(L"    WARNING: device is pending reboot, skipping...");
        return PendingReboot;
    }

    return Valid;
}

// Tries to look for the OpenCL key under the display devices and
// if not found, falls back to software component devices.
bool EnumDisplay(void)
{
    CONFIGRET ret;
    bool foundOpenCLKey = false;
    DEVINST devinst = 0;
    DEVINST devchild = 0;
    wchar_t *deviceIdList = NULL;
    ULONG szBuffer = 0;

    // TODO: can we use StringFromGUID2(GUID_DEVCLASS_DISPLAY) instead of
    //       hardcoding the value here?
    static const wchar_t* DISPLAY_ADAPTER_GUID =
        L"{4d36e968-e325-11ce-bfc1-08002be10318}";
    ULONG ulFlags = CM_GETIDLIST_FILTER_CLASS |
                    CM_GETIDLIST_FILTER_PRESENT;

    // Paranoia: we might have a new device added to the list between the call
    // to CM_Get_Device_ID_List_Size() and the call to CM_Get_Device_ID_List().
    do
    {
        ret = CM_Get_Device_ID_List_Size(
            &szBuffer,
            DISPLAY_ADAPTER_GUID,
            ulFlags);

        if (CR_SUCCESS != ret)
        {
            OCL_ENUM_TRACE(TEXT("CM_Get_Device_ID_List_size failed with 0x%x\n"), ret);
            break;
        }

        // "pulLen [out] Receives a value representing the required buffer
        //  size, in characters."
        //  So we need to allocate the right size in bytes but we still need
        //  to keep szBuffer as it was returned from CM_Get_Device_ID_List_Size so
        //  the call to CM_Get_Device_ID_List will receive the correct size.
        deviceIdList = malloc(szBuffer * sizeof(wchar_t));
        if (NULL == deviceIdList)
        {
            OCL_ENUM_TRACE(TEXT("Failed to allocate %u bytes for device ID strings\n"), szBuffer);
            break;
        }

        ret = CM_Get_Device_ID_List(
            DISPLAY_ADAPTER_GUID,
            deviceIdList,
            szBuffer,
            ulFlags);

        if (CR_SUCCESS != ret)
        {
            OCL_ENUM_TRACE(TEXT("CM_Get_Device_ID_List failed with 0x%x\n"), ret);
            free(deviceIdList);
            deviceIdList = NULL;
        }
    } while (CR_BUFFER_SMALL == ret);

    if (NULL == deviceIdList)
    {
        goto out;
    }

    for (PWSTR deviceId = deviceIdList; *deviceId; deviceId += wcslen(deviceId) + 1)
    {
        DEVPROPTYPE devpropType;

        wprintf_s(L"Device ID: %ls\n", deviceId);

        ret = CM_Locate_DevNode(&devinst, deviceId, 0);
        if (CR_SUCCESS == ret)
        {
            wprintf_s(L"    devinst: %d\n", devinst);
        }
        else
        {
            OCL_ENUM_TRACE(TEXT("CM_Locate_DevNode failed with 0x%x\n"), ret);
            continue;
        }

        if (ProbeDevice(devinst) != Valid)
        {
            continue;
        }

        wprintf_s(L"    Trying to look for the key in the display adapter HKR...\n");
        if (foundOpenCLKey = ReadOpenCLKey(devinst))
        {
            continue;
        }

        wprintf_s(L"    Could not find the key, proceeding to children software components...\n");

        if (CM_Get_Child(&devchild, devinst, 0) == CR_SUCCESS)
        {
            do
            {
                wchar_t deviceInstanceID[MAX_DEVICE_ID_LEN] = { 0 };
                GUID guid;
                ULONG szGuid = sizeof(guid);

                wprintf_s(L"    devchild: %d\n", devchild);
                if (CM_Get_Device_ID(devchild, deviceInstanceID, sizeof(deviceInstanceID), 0) == CR_SUCCESS)
                {
                    wprintf_s(L"    deviceInstanceID: %ls\n", deviceInstanceID);
                }

                ret = CM_Get_DevNode_Property(
                    devchild,
                    &DEVPKEY_Device_ClassGuid,
                    &devpropType,
                    (PBYTE)&guid,
                    &szGuid,
                    0);

                OCL_ENUM_ASSERT(devpropType == DEVPROP_TYPE_GUID);

                if (CR_SUCCESS != ret ||
                    !IsEqualGUID(&GUID_DEVCLASS_SOFTWARECOMPONENT, &guid))
                {
                    continue;
                }

                if (ProbeDevice(devchild) != Valid)
                {
                    continue;
                }

                if (foundOpenCLKey = ReadOpenCLKey(devchild))
                {
                    break;
                }
            } while (CM_Get_Sibling(&devchild, devchild, 0) == CR_SUCCESS);
        }
    }

out:
    free(deviceIdList);
    return foundOpenCLKey;
}
