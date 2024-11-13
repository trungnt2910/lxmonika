#include "registry.h"

#include <memory>
#include <vector>

#include <Windows.h>

#include "Exception.h"

template <typename... Args>
std::shared_ptr<std::remove_pointer_t<HKEY>>
RegGetSharedRegistryHandle(LSTATUS(APIENTRY *func)(Args..., PHKEY), Args&&... args)
{
    HKEY hKeyResult = NULL;
    Win32Exception::ThrowIfNonZero(func(std::forward<decltype(args)>(args)..., &hKeyResult));
    return std::shared_ptr<std::remove_pointer_t<HKEY>>(hKeyResult, RegCloseKey);
}

bool
RegIsTestSigningEnabled()
{
    std::wstring value = std::any_cast<std::wstring&>(
        RegQueryRegistryKey(L"SYSTEM\\CurrentControlSet\\Control")
            .at(L"SystemStartOptions")
    );

    std::wstringstream valueStream(std::move(value));

    while (true)
    {
        value.clear();
        valueStream >> value;

        if (value.empty())
        {
            // Stream has ended.
            break;
        }

        if (value == L"TESTSIGNING")
        {
            return true;
        }
    }

    return false;
}

RegistryValues
RegQueryRegistryKey(
    const std::wstring& path
)
{
    auto hKey = RegGetSharedRegistryHandle<HKEY, LPCWSTR>(
        RegOpenKeyW,
        HKEY_LOCAL_MACHINE,
        path.c_str()
    );

    RegistryValues result;

    DWORD dwIndex = 0;
    DWORD dwType;
    std::wstring valueName((size_t)MAXSHORT + 1, L'\0');
    DWORD cchValueName = MAXSHORT;
    std::vector<BYTE> data;
    DWORD cbData = 0;

    while (true)
    {
        cchValueName = MAXSHORT;
        DWORD cbOriginalData = (DWORD)data.size();
        cbData = cbOriginalData;

        LSTATUS status = RegEnumValueW(
            hKey.get(),
            dwIndex,
            valueName.data(),
            &cchValueName,
            NULL,
            &dwType,
            (PBYTE)data.data(),
            &cbData
        );

        data.resize(cbData);

        switch (status)
        {
            case ERROR_NO_MORE_ITEMS:
                return result;
            case ERROR_MORE_DATA:
            case ERROR_SUCCESS:
                // If we pass 0 as cbData, we will still get ERROR_SUCCESS despite
                // the data not being read at all.
                if (cbOriginalData < cbData)
                {
                    // The original buffer had fewer bytes than required. Re-read.
                    continue;
                }
                else
                {
                    Win32Exception::ThrowIfNonZero(status);
                    break;
                }
            default:
                throw Win32Exception(status);
        }

        valueName[cchValueName] = L'\0';

        switch (dwType)
        {
            case REG_BINARY:
                result.emplace(
                    valueName.data(),
                    std::vector<BYTE>(data.data(), data.data() + cbData)
                );
            break;
            case REG_DWORD:
                result.emplace(valueName.data(), *(DWORD*)(data.data()));
            break;
            case REG_QWORD:
                result.emplace(valueName.data(), *(ULONG64*)(data.data()));
            break;
            case REG_EXPAND_SZ:
                result.emplace(valueName.data(), ExpandString((PWCHAR)data.data()));
            break;
            case REG_SZ:
                result.emplace(valueName.data(), std::wstring((PWCHAR)data.data()));
            break;
            case REG_MULTI_SZ:
                result.emplace(valueName.data(), UtilStringListToVector((PWCHAR)data.data()));
            break;
            default:
                throw Win32Exception(ERROR_NOT_SUPPORTED);
        }

        ++dwIndex;
    }
}

void
RegSetRegistryKey(
    const std::wstring& path,
    const RegistryValues& contents
)
{
    auto hKey = RegGetSharedRegistryHandle<HKEY, LPCWSTR>(
        RegCreateKeyW,
        HKEY_LOCAL_MACHINE,
        path.c_str()
    );

    DWORD dwType;
    std::vector<BYTE> data;

    for (const auto& [name, value] : contents)
    {
        if (value.type() == typeid(std::vector<BYTE>))
        {
            dwType = REG_BINARY;
            const std::vector<BYTE>& bytes = std::any_cast<const std::vector<BYTE>&>(value);
            data = bytes;
        }
        else if (value.type() == typeid(DWORD))
        {
            dwType = REG_DWORD;
            data.resize(sizeof(DWORD));
            DWORD dwValue = std::any_cast<DWORD>(value);
            memcpy(data.data(), &dwValue, sizeof(DWORD));
        }
        else if (value.type() == typeid(ULONG64))
        {
            dwType = REG_QWORD;
            data.resize(sizeof(ULONG64));
            ULONG64 ulValue = std::any_cast<ULONG64>(value);
            memcpy(data.data(), &ulValue, sizeof(ULONG64));
        }
        else if (value.type() == typeid(ExpandString))
        {
            dwType = REG_EXPAND_SZ;
            const ExpandString& str = std::any_cast<const ExpandString&>(value);
            data.resize((str.size() + 1) * sizeof(WCHAR));
            memcpy(data.data(), str.data(), data.size());
        }
        else if (value.type() == typeid(std::wstring))
        {
            dwType = REG_SZ;
            const std::wstring& str = std::any_cast<const std::wstring&>(value);
            data.resize((str.size() + 1) * sizeof(WCHAR));
            memcpy(data.data(), str.data(), data.size());
        }
        else if (value.type() == typeid(std::vector<std::wstring>))
        {
            dwType = REG_MULTI_SZ;
            std::wstring str = UtilVectorToStringList(
                std::any_cast<const std::vector<std::wstring>&>(value)
            );
            data.clear();
            data.resize((str.size() + 1) * sizeof(WCHAR));
            memcpy(data.data(), str.data(), data.size());
        }
        else
        {
            throw Win32Exception(ERROR_NOT_SUPPORTED);
        }

        Win32Exception::ThrowIfNonZero(RegSetKeyValueW(
            hKey.get(),
            NULL,
            name.c_str(),
            dwType,
            data.data(),
            (DWORD)data.size()
        ));
    }
}

void
RegClearRegistryKey(
    const std::wstring& path
)
{
    auto hKey = RegGetSharedRegistryHandle<HKEY, LPCWSTR>(
        RegOpenKeyW,
        HKEY_LOCAL_MACHINE,
        path.c_str()
    );

    DWORD dwIndex = 0;
    std::wstring valueName((size_t)MAXSHORT + 1, L'\0');
    DWORD cchValueName = MAXSHORT;

    std::vector<std::wstring> toDelete;

    while (true)
    {
        cchValueName = MAXSHORT;

        LSTATUS status = RegEnumValueW(
            hKey.get(),
            dwIndex,
            valueName.data(),
            &cchValueName,
            NULL,
            NULL,
            NULL,
            NULL
        );

        switch (status)
        {
        case ERROR_NO_MORE_ITEMS:
            goto startDelete;
        case ERROR_SUCCESS:
            break;
        default:
            throw Win32Exception(status);
        }

        valueName[cchValueName] = L'\0';
        toDelete.emplace_back(valueName.c_str());

        ++dwIndex;
    }

startDelete:
    for (const std::wstring& name : toDelete)
    {
        Win32Exception::ThrowIfNonZero(RegDeleteValueW(
            hKey.get(),
            name.c_str()
        ));
    }
}
