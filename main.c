//
// This application enumerates all OpenCLDriverName[Wow] registry values under the
// "Software Components" reg key:
// HKLM\SYSTEM\CurrentControlSet\Control\Class\{5c4c3332-344d-483c-8739-259e934c9cc8}\
//
// Starting from Windows 10 RS3+, graphics drivers components have dedicated
// component INF files and are referenced via the INF AddComponent directive.
// Every such component, e.g. OpenCL instance (ICD), is assigned a unique device
// instance ID (e.g. 0000, 0001, etc) under the reg key above and use it to store
// the path to its ICD.
//
// Possible keys:
// Key:   HKLM\SYSTEM\CurrentControlSet\Control\Class\{5c4c3332-344d-483c-8739-259e934c9cc8}\0000\OpenCLDriverName (REG_MULTI_SZ)
// Value: IntelOpenCL64.dll
//

// Author: Ofir Cohen <ofir.o.cohen@intel.com>
//
// License: this code is in the public domain.
//
// References:
// https://docs.microsoft.com/en-us/windows-hardware/drivers/install/adding-software-components-with-an-inf-file
//
#include <windows.h>

#if 1
    #include <stdio.h>
    #define OCL_ENUM_TRACE(...) \
    do \
    { \
        fprintf(stderr, "OCL ENUM trace at %s:%d: ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
    } while (0)

    #define OCL_ENUM_ASSERT(x) \
    do \
    { \
        if (!(x)) \
        { \
            fprintf(stderr, "OCL ENUM assert at %s:%d: %s failed", __FILE__, __LINE__, #x); \
        } \
    } while (0)
#else
    #define OCL_ENUM_TRACE(...)
    #define OCL_ENUM_ASSERT(x)
#endif

#ifdef _WIN64
static const char OPENCL_REG_SUB_KEY[] = "OpenCLDriverName";
#else
static const char OPENCL_REG_SUB_KEY[] = "OpenCLDriverNameWow";
#endif

// Enumerates each of the vendors under the SW components key
// and searches for OpenCLDriverName[Wow] entry.
BOOL CALLBACK khrEnumSoftwareComponents(void)
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
        return FALSE;
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

        result = RegQueryValueExA(
            devInstKey,
            OPENCL_REG_SUB_KEY,
            NULL,
            &dwLibraryNameType,
            (LPBYTE)cszOclPath,
            &dwOclPathSize);
        if (ERROR_SUCCESS != result)
        {
            OCL_ENUM_TRACE("Failed to open sub key %s, continuing\n", OPENCL_REG_SUB_KEY);
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

    return TRUE;
}

int main()
{
    BOOL res;
    res = khrEnumSoftwareComponents();
    return res ? 0 : 1;
}
