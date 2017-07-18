#include "shared.h"
#include <windows.h>
#include <stdbool.h>

extern const wchar_t OPENCL_REG_SUB_KEY[];

// Enumerates each of the vendors under the SW components key
// and searches for OpenCLDriverName[Wow] entry.
bool EnumRegistry(void)
{
    static const char* SW_COMPONENTS_KEY =
        "SYSTEM\\CurrentControlSet\\Control\\Class\\{5c4c3332-344d-483c-8739-259e934c9cc8}\\";

    HKEY swcoKey;
    LSTATUS result;
    DWORD dwIndex;

    OCL_ENUM_TRACE("Opening key HKLM\\%s...\n", SW_COMPONENTS_KEY);
    result = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        SW_COMPONENTS_KEY,
        0,
        KEY_READ,
        &swcoKey);

    if (ERROR_SUCCESS != result)
    {
        OCL_ENUM_TRACE("Failed to open software components key %s\n", SW_COMPONENTS_KEY);
        return false;
    }

    for (dwIndex = 0;; ++dwIndex)
    {
        errno_t ret;
        char cszDeviceInstanceAbs[1024] = { 0 };
        char cszDeviceInstance[1024] = { 0 };
        DWORD dwDeviceInstanceAbsSize = sizeof(cszDeviceInstanceAbs);
        DWORD dwDeviceInstanceSize = sizeof(cszDeviceInstance);
        DWORD dwLibraryNameType = 0;
        char cszOclPath[MAX_PATH] = { '\0' };
        DWORD dwOclPathSize = sizeof(cszOclPath);
        HKEY devInstKey = NULL;

        result = RegEnumKeyExA(
            swcoKey,
            dwIndex,
            cszDeviceInstance,
            &dwDeviceInstanceSize,
            NULL,
            NULL,
            NULL,
            NULL);

        if (ERROR_NO_MORE_ITEMS == result)
        {
            OCL_ENUM_TRACE("Done with the sw components enumeration.\n");
            break;
        }
        else if (ERROR_SUCCESS != result)
        {
            OCL_ENUM_ASSERT(!"Unexpected failure.");
            break;
        }

        ret = strcat_s(cszDeviceInstanceAbs, dwDeviceInstanceAbsSize, SW_COMPONENTS_KEY);
        OCL_ENUM_ASSERT(ret == 0);
        ret = strcat_s(cszDeviceInstanceAbs, dwDeviceInstanceAbsSize, "\\");
        OCL_ENUM_ASSERT(ret == 0);
        ret = strcat_s(cszDeviceInstanceAbs, dwDeviceInstanceAbsSize, cszDeviceInstance);
        OCL_ENUM_ASSERT(ret == 0);

        OCL_ENUM_TRACE("Opening key HKLM\\%s...\n", cszDeviceInstanceAbs);
        result = RegOpenKeyExA(
            HKEY_LOCAL_MACHINE,
            cszDeviceInstanceAbs,
            0,
            KEY_READ,
            &devInstKey);

        if (ERROR_SUCCESS != result)
        {
            OCL_ENUM_TRACE("Failed to open device key %s, continuing\n", cszDeviceInstanceAbs);
            continue;
        }

        result = RegQueryValueExW(
            devInstKey,
            OPENCL_REG_SUB_KEY,
            NULL,
            &dwLibraryNameType,
            (LPBYTE)cszOclPath,
            &dwOclPathSize);
        if (ERROR_SUCCESS != result)
        {
            OCL_ENUM_TRACE("Failed to open sub key, continuing\n");
            goto NextIter;
        }

        OCL_ENUM_TRACE("Key HKLM\\%s, Value %s\n", cszDeviceInstanceAbs, cszOclPath);

    NextIter:
        if (devInstKey)
        {
            result = RegCloseKey(devInstKey);
            if (ERROR_SUCCESS != result)
            {
                OCL_ENUM_TRACE("Failed to close device key %s, ignoring\n", cszDeviceInstanceAbs);
            }

            devInstKey = NULL;
        }
    }


    result = RegCloseKey(swcoKey);
    if (ERROR_SUCCESS != result)
    {
        OCL_ENUM_TRACE("Failed to close software components key %s, ignoring\n", SW_COMPONENTS_KEY);
    }

    return true;
}
