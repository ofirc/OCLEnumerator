/*
 OpenCL ICDs enumeration using Universal (CM_*) or the Windows Registry APIs.

 Launch modes:
 1. OCLEnumerator.exe --display

    Starts from Display Adapters and then uses CM_Get_Child/Sibling to find any
    software component children.

 2. OCLEnumerator.exe --registry

    Enumerates device keys by looking under:
    HKLM\SYSTEM\CurrentControlSet\Control\Class\{5c4c3332-344d-483c-8739-259e934c9cc8}\*\OpenCLDriverName[Wow]

 Starting from Windows 10 RS3+, graphics drivers components have dedicated
 component INF files and are referenced via the INF AddComponent directive.
 Every such component, e.g. OpenCL instance (ICD), is assigned a unique device
 instance ID (e.g. 0000, 0001, etc) under the reg key above and use it to store
 the path to its ICD.

 Possible keys:
 Key:   HKLM\SYSTEM\CurrentControlSet\Control\Class\{5c4c3332-344d-483c-8739-259e934c9cc8}\0000\OpenCLDriverName (REG_MULTI_SZ)
 Value: IntelOpenCL64.dll

 Author: Ofir Cohen <ofir.o.cohen@intel.com>

 License: this code is in the public domain.

 References:
 https://docs.microsoft.com/en-us/windows-hardware/drivers/install/adding-software-components-with-an-inf-file

*/
#include "modes.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef enum
{
    Display,
    SoftwareComponents,
    CmRegistrySoftware,
    Registry
} Mode;

#ifdef _WIN64
const wchar_t OPENCL_REG_SUB_KEY[] = L"OpenCLDriverName";
#else
const wchar_t OPENCL_REG_SUB_KEY[] = L"OpenCLDriverNameWow";
#endif

static void usage(char *prog_name)
{
    printf("Usage: %s [--display | --software-components | --cm-registry-software | --registry]\n", prog_name);
    printf("If no mode is given, --display is the selected mode.\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    Mode mode = Display;
    bool res = false;

    if (argc > 2)
        usage(argv[0]);

    if (argc == 2)
    {
        if (strcmp(argv[1], "--display") == 0)
            mode = Display;
        else if (strcmp(argv[1], "--software-components") == 0)
            mode = SoftwareComponents;
        else if (strcmp(argv[1], "--cm-registry-software") == 0)
            mode = CmRegistrySoftware;
        else if (strcmp(argv[1], "--registry") == 0)
            mode = Registry;
        else
        {
            fprintf(stderr, "error: invalid usage mode %s.\n", argv[1]);
            usage(argv[0]);
        }
    }

    switch (mode)
    {
    case Display:
        res = EnumDisplay();
        break;
    case Registry:
        res = EnumRegistry();
        break;
    default:
        fprintf(stderr, "error: mode %s is not supported.", argv[1]);
        exit(EXIT_FAILURE);
    }

    printf("%s OpenCL ICD key\n", res ? "Found" : "Could not find");

    return res ? EXIT_SUCCESS : EXIT_FAILURE;
}
