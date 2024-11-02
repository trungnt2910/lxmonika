#include "monika.h"

#include <bcrypt.h>

#include "device.h"
#include "module.h"

#include "AutoResource.h"
#include "Logger.h"

BOOLEAN MapCngLoaded = FALSE;
PDRIVER_OBJECT MapCngDriverObject = NULL;
PDRIVER_UNLOAD MapCngDriverUnload = NULL;
PDEVICE_SET MapCngDeviceSet = NULL;

NTSTATUS
MapCngInitialize(
    _Inout_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    HANDLE hdlCng = NULL;
    MA_RETURN_IF_FAIL(MdlpFindModuleByName("cng.sys", &hdlCng, NULL));

    PDRIVER_INITIALIZE pCngDriverEntry = NULL;
    MA_RETURN_IF_FAIL(MdlpGetEntryPoint(hdlCng, (PVOID*)&pCngDriverEntry));

    Logger::LogTrace("Resolved cng.sys entry point at: ", pCngDriverEntry, ".");

    MA_RETURN_IF_FAIL(pCngDriverEntry(DriverObject, RegistryPath));

    // Free the driver if any of the subsequent operations fail.
    BOOLEAN bMapUnfinished = TRUE;
    AUTO_RESOURCE(bMapUnfinished, [](auto b) { (void)b; MapCngCleanup(); });

    MA_RETURN_IF_FAIL(DevpRegisterDeviceSet(DriverObject, &MapCngDeviceSet));

    // Commit.
    bMapUnfinished = FALSE;

    MapCngLoaded = TRUE;
    MapCngDriverObject = DriverObject;
    MapCngDriverUnload = DriverObject->DriverUnload;

    // Doing dummy stuff to make sure the linker imports cng.sys
    UCHAR chBuffer[sizeof(PVOID)];
    PVOID pFuncToImport = (PVOID)BCryptGenRandom;
    memcpy(chBuffer, &pFuncToImport, sizeof(PVOID));
    BCryptGenRandom(NULL, chBuffer, sizeof(chBuffer), BCRYPT_USE_SYSTEM_PREFERRED_RNG);

    Logger::LogTrace("Successfully loaded cng.sys.");

    return STATUS_SUCCESS;
}

VOID
MapCngCleanup()
{
    if (MapCngLoaded)
    {
        if (MapCngDeviceSet != NULL)
        {
            DevpUnregisterDeviceSet(MapCngDeviceSet);
        }

        if (MapCngDriverUnload != NULL)
        {
            MapCngDriverUnload(MapCngDriverObject);
        }

        MapCngDeviceSet = NULL;
        MapCngDriverUnload = NULL;
        MapCngDriverObject = NULL;
        MapCngLoaded = FALSE;
    }
}
