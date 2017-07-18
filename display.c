#include "shared.h"
#include "modes.h"
#include <windows.h>
#include <cfgmgr32.h>

#pragma comment(lib, "Cfgmgr32.lib")

extern const wchar_t OPENCL_REG_SUB_KEY[];

static const wchar_t HKR_PREFIX[] = L"SYSTEM\\CurrentControlSet\\Control\\Class\\";

static bool ReadOpenCLKey(DEVINST dnDevNode)
{
    HKEY hkey = 0;
    DWORD ret;
    bool bRet = false;
    DWORD dwLibraryNameType = 0;
    wchar_t wcszOclPath[MAX_PATH] = { '\0' };
    DWORD dwOclPathSize = sizeof(wcszOclPath);
    LSTATUS result;

    ret = CM_Open_DevNode_Key(
        dnDevNode,
        KEY_QUERY_VALUE,
        0,
        RegDisposition_OpenExisting,
        &hkey,
        CM_REGISTRY_SOFTWARE);

    if (ret == CR_SUCCESS)
    {
        result = RegQueryValueEx(
            hkey,
            OPENCL_REG_SUB_KEY,
            NULL,
            &dwLibraryNameType,
            (LPBYTE)wcszOclPath,
            &dwOclPathSize);
        if (ERROR_SUCCESS != result)
        {
            OCL_ENUM_TRACE("Failed to open sub key\n");
            goto out;
        }

        if (REG_MULTI_SZ != dwLibraryNameType)
        {
            OCL_ENUM_TRACE("Unexpected registry entry! continuing\n");
            goto out;
        }

        wprintf_s(L"Path: %ls\n", wcszOclPath);

        ret = true;

    }

out:
    if (hkey)
    {
        RegCloseKey(hkey);
    }

    return ret;
}


// Tries to look for the OpenCL key under the display devices and
// if not found, fall back to software component devices.
bool EnumDisplay(void)
{
    int ret;
    bool foundOpenCLKey = false;
    wchar_t buffer[4096] = { 0 };
    DEVPROPKEY devpropkey[128] = { 0 };
    wchar_t *fullHkr = NULL;

    ULONG szBuffer = sizeof(buffer);
    ULONG szDevpropkeycount = sizeof(devpropkey) / sizeof(devpropkey[0]);
    DEVINST devinst = 0;
    DEVINST devchild = 0;
    static const wchar_t* DISPLAY_ADAPTER_GUID =
        L"{4d36e968-e325-11ce-bfc1-08002be10318}";

    // Enumerate display adapters
    //
    ret = CM_Get_Device_ID_List(
        DISPLAY_ADAPTER_GUID,
        buffer,
        sizeof(buffer),
        CM_GETIDLIST_FILTER_CLASS);

    if (ret != CR_SUCCESS)
    {
        OCL_ENUM_TRACE("CM_Get_Device_ID_List failed with 0x%x\n", ret);
        return false;
    }

    wprintf_s(L"%ls\n", buffer);

    if (CM_Locate_DevNode(&devinst, buffer, 0) == CR_SUCCESS)
    {
        wprintf_s(L"devinst: %d\n", devinst);
    }

    if (CM_Get_Child(&devchild, devinst, 0) == CR_SUCCESS)
    {
        do
        {
            wchar_t deviceInstanceID[MAX_PATH] = { 0 };

            wprintf_s(L"devchild: %d\n", devchild);
            if (CM_Get_Device_ID(devchild, deviceInstanceID, MAX_PATH, 0) == CR_SUCCESS)
            {
                wprintf_s(L"deviceInstanceID: %ls\n", deviceInstanceID);
            }

            if (!foundOpenCLKey)
            {
                foundOpenCLKey = ReadOpenCLKey(devchild);
            }
        } while (CM_Get_Sibling(&devchild, devchild, 0) == CR_SUCCESS);
    }

    return foundOpenCLKey;
}
