# OCLEnumerator
Yet another OpenCL Loader

This application enumerates all OpenCLDriverName[Wow] registry values under the
"Software Components" reg key:
HKLM\SYSTEM\CurrentControlSet\Control\Class\{5c4c3332-344d-483c-8739-259e934c9cc8}\

Starting from Windows 10 RS3+, graphics drivers components have dedicated
component INF files and are referenced via the INF AddComponent directive.
Every such component, e.g. OpenCL instance (ICD), is assigned a unique device
instance ID (e.g. 0000, 0001, etc) under the reg key above and use it to store
the path to its ICD.

Possible keys:
Key:   HKLM\SYSTEM\CurrentControlSet\Control\Class\{5c4c3332-344d-483c-8739-259e934c9cc8}\0000\OpenCLDriverName (REG_MULTI_SZ)
Value: IntelOpenCL64.dll

License: this code is in the public domain.

References:
https://docs.microsoft.com/en-us/windows-hardware/drivers/install/adding-software-components-with-an-inf-file
