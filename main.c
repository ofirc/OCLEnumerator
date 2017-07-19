/*
 OpenCL ICDs enumeration using the Universal API (CM_*).

 Launch with no arguments:
 OCLEnumerator.exe

 It will enumerate all Display Adapters nodes, search for OpenCLDriverName[Wow]
 registry value under their HKR and if not found - it will use CM_Get_Child/Sibling
 to find software component children which might contain the key.

 Background:
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
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

extern bool EnumDisplay(void);

int main(int argc, char** argv)
{
    bool res = false;

    res = EnumDisplay();

    printf("%s OpenCL ICD key\n", res ? "Found" : "Could not find");

    return res ? EXIT_SUCCESS : EXIT_FAILURE;
}
