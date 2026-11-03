#pragma once
// Ported from MFG-Unlocker source/native/adapter_discovery.h, MIT licensed, see NOTICE.md.
// Only the NVAPI ownership checks the CUDA route uses; adapter enumeration is not needed.
#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <iterator>

namespace adapter_discovery
{
inline bool OwnedCode(HMODULE module, const void* code) noexcept
{
    MEMORY_BASIC_INFORMATION memory{};
    if (!code || VirtualQuery(code, &memory, sizeof(memory)) != sizeof(memory)) return false;
    const DWORD protection = memory.Protect & 0xff;
    return memory.AllocationBase == module && memory.Type == MEM_IMAGE && memory.State == MEM_COMMIT
        && !(memory.Protect & (PAGE_GUARD | PAGE_NOACCESS))
        && (protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE);
}

struct ModuleReference
{
    HMODULE module{};
    ModuleReference() noexcept = default;
    ModuleReference(const ModuleReference&) = delete;
    ModuleReference& operator=(const ModuleReference&) = delete;
    ~ModuleReference() { if (module) FreeLibrary(module); }
    bool Acquire(const void* address) noexcept
    {
        return !module && address && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(address), &module);
    }
    void Reset() noexcept
    {
        if (module) FreeLibrary(module);
        module = nullptr;
    }
};

constexpr size_t kPathCapacity = 1024;
inline bool CanonicalPath(const wchar_t* path, std::array<wchar_t, kPathCapacity>& canonical) noexcept
{
    const size_t length = path ? wcsnlen_s(path, kPathCapacity) : 0;
    if (length < 3 || length == kPathCapacity || path[1] != L':' || (path[2] != L'\\' && path[2] != L'/'))
        return false;
    // Loaded Windows driver paths need no relative, device, or UNC syntax.
    for (size_t start = 3, end = start; start < length; start = end + 1)
    {
        end = start;
        while (end < length && path[end] != L'\\' && path[end] != L'/') ++end;
        if (end == start || (end - start == 1 && path[start] == L'.')
            || (end - start == 2 && path[start] == L'.' && path[start + 1] == L'.')) return false;
    }
    const DWORD result = GetFullPathNameW(path, static_cast<DWORD>(canonical.size()), canonical.data(), nullptr);
    return result && result < canonical.size();
}

inline bool NvapiOwnerPath(const wchar_t* systemDirectory, const wchar_t* candidate,
    bool systemStubOnly = false) noexcept
{
    std::array<wchar_t, kPathCapacity> system{}, path{};
    if (!CanonicalPath(systemDirectory, system) || !CanonicalPath(candidate, path)) return false;
    const size_t baseLength = wcslen(system.data()), pathLength = wcslen(path.data());
    if (baseLength + 1 >= pathLength || path[baseLength] != L'\\'
        || CompareStringOrdinal(system.data(), static_cast<int>(baseLength), path.data(),
            static_cast<int>(baseLength), TRUE) != CSTR_EQUAL) return false;
    const wchar_t* relative = path.data() + baseLength + 1;
    if (_wcsicmp(relative, L"nvapi64.dll") == 0) return true;
    if (systemStubOnly) return false;
    constexpr wchar_t repository[] = L"DriverStore\\FileRepository\\";
    constexpr size_t prefixLength = std::size(repository) - 1;
    if (wcslen(relative) <= prefixLength || _wcsnicmp(relative, repository, prefixLength) != 0) return false;
    const wchar_t* leaf = wcsrchr(relative + prefixLength, L'\\');
    return leaf && leaf > relative + prefixLength && _wcsicmp(leaf + 1, L"nvapi64_impl.dll") == 0;
}

using QueryInterface = void*(__cdecl*)(uint32_t);
inline bool OwnerFactoryMatches(HMODULE owner, uint32_t id, const void* target) noexcept
{
    if (!OwnedCode(owner, target)) return false;
    const auto query = reinterpret_cast<QueryInterface>(GetProcAddress(owner, "nvapi_QueryInterface"));
    return OwnedCode(owner, reinterpret_cast<void*>(query)) && query(id) == target;
}
} // namespace adapter_discovery
