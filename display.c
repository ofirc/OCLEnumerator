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
DEFINE_GUID(OCL_GUID_DEVCLASS_SOFTWARECOMPONENT, 0x5c4c3332, 0x344d, 0x483c, 0x87, 0x39, 0x25, 0x9e, 0x93, 0x4c, 0x9c, 0xc8);

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

    if (CR_SUCCESS != ret)
    {
        OCL_ENUM_TRACE(TEXT("Failed with ret 0x%x\n"), ret);
        goto out;
    }
    else
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
            OCL_ENUM_TRACE(TEXT("Failed to open sub key 0x%x\n"), result);
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
            OCL_ENUM_TRACE(TEXT("Failed to open sub key 0x%x\n"), result);
            goto out;
        }

        if (REG_SZ != dwLibraryNameType)
        {
            OCL_ENUM_TRACE(TEXT("Unexpected registry entry 0x%x! continuing\n"), dwLibraryNameType);
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
            OCL_ENUM_TRACE(TEXT("WARNING: failed to close hkey 0x%x\n"), result);
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
        wprintf_s(L"    WARNING: failed to probe the status of the device 0x%x\n", ret);
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
        wprintf_s(L"    WARNING: device is pending reboot (0x%x), skipping...\n", ulStatus);
        return PendingReboot;
    }

    return Valid;
}

// Tries to look for the OpenCL key under the display devices and
// if not found, falls back to software component devices.
bool EnumDisplay(void)
{
    CONFIGRET ret;
    int iret;
    bool foundOpenCLKey = false;
    DEVINST devinst = 0;
    DEVINST devchild = 0;
    wchar_t *deviceIdList = NULL;
    ULONG szBuffer = 0;

    OLECHAR display_adapter_guid_str[MAX_GUID_STRING_LEN];
    ULONG ulFlags = CM_GETIDLIST_FILTER_CLASS |
                    CM_GETIDLIST_FILTER_PRESENT;

    iret = StringFromGUID2(
        &GUID_DEVCLASS_DISPLAY,
        display_adapter_guid_str,
        MAX_GUID_STRING_LEN);

    if (MAX_GUID_STRING_LEN != iret)
    {
        OCL_ENUM_TRACE(TEXT("StringFromGUID2 failed with %d\n"), iret);
        goto out;
    }

    // Paranoia: we might have a new device added to the list between the call
    // to CM_Get_Device_ID_List_Size() and the call to CM_Get_Device_ID_List().
    do
    {
        ret = CM_Get_Device_ID_List_Size(
            &szBuffer,
            display_adapter_guid_str,
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
            display_adapter_guid_str,
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
        if (foundOpenCLKey |= ReadOpenCLKey(devinst))
        {
            continue;
        }

        wprintf_s(L"    Could not find the key, proceeding to children software components...\n");

        ret = CM_Get_Child(
            &devchild,
            devinst,
            0);

        if (CR_SUCCESS != ret)
        {
            wprintf_s(L"    CM_Get_Child returned 0x%x, skipping children...\n", ret);
        }
        else
        {
            do
            {
                wchar_t deviceInstanceID[MAX_DEVICE_ID_LEN] = { 0 };
                GUID guid;
                ULONG szGuid = sizeof(guid);

                wprintf_s(L"    devchild: %d\n", devchild);
                ret = CM_Get_Device_ID(
                    devchild,
                    deviceInstanceID,
                    sizeof(deviceInstanceID),
                    0);
                if (CR_SUCCESS != ret)
                {
                    wprintf_s(L"    CM_Get_Device_ID returned 0x%x, skipping device...\n", ret);
                    continue;
                }
                else
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
                    !IsEqualGUID(&OCL_GUID_DEVCLASS_SOFTWARECOMPONENT, &guid))
                {
                    continue;
                }

                if (ProbeDevice(devchild) != Valid)
                {
                    continue;
                }

                if (foundOpenCLKey |= ReadOpenCLKey(devchild))
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
