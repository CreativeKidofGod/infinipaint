/*
 * InfiniPaint
 * Copyright (C) 2025-2026 Yousef Khadadeh
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "CanvasProtection.hpp"
#include <system_error>

#ifdef _WIN32
    #include <windows.h>
    #include <aclapi.h>
    #include <shellapi.h>
    #pragma comment(lib, "advapi32.lib")
    #pragma comment(lib, "shell32.lib")
#endif

namespace CanvasProtection {

namespace {

const std::filesystem::path CANVAS_EXTENSION = ".infpnt";

#ifdef _WIN32
// The rules use the well-known "Everyone" group, so they apply to File Explorer and every other program
struct EveryoneSid {
    BYTE buffer[SECURITY_MAX_SID_SIZE];
    PSID sid = nullptr;
    EveryoneSid() {
        DWORD size = sizeof(buffer);
        if(CreateWellKnownSid(WinWorldSid, nullptr, buffer, &size))
            sid = buffer;
    }
};

PSID everyone() {
    static EveryoneSid e;
    return e.sid;
}

bool is_our_deny_ace(void* ace, DWORD mask) {
    ACE_HEADER* header = static_cast<ACE_HEADER*>(ace);
    if(header->AceType != ACCESS_DENIED_ACE_TYPE || (header->AceFlags & INHERITED_ACE))
        return false;
    ACCESS_DENIED_ACE* denied = static_cast<ACCESS_DENIED_ACE*>(ace);
    return denied->Mask == mask && EqualSid(reinterpret_cast<PSID>(&denied->SidStart), everyone());
}

void add_deny(const std::filesystem::path& p, DWORD mask) {
    if(!everyone())
        return;
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    if(GetNamedSecurityInfoW(p.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &dacl, nullptr, &sd) != ERROR_SUCCESS)
        return;
    bool alreadyThere = false;
    ACL_SIZE_INFORMATION info;
    if(dacl && GetAclInformation(dacl, &info, sizeof(info), AclSizeInformation)) {
        for(DWORD i = 0; i < info.AceCount && !alreadyThere; i++) {
            void* ace = nullptr;
            if(GetAce(dacl, i, &ace) && is_our_deny_ace(ace, mask))
                alreadyThere = true;
        }
    }
    if(!alreadyThere) {
        EXPLICIT_ACCESSW ea{};
        ea.grfAccessPermissions = mask;
        ea.grfAccessMode = DENY_ACCESS;
        ea.grfInheritance = NO_INHERITANCE; // Only this file/folder, never the things inside it
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea.Trustee.ptstrName = static_cast<LPWSTR>(everyone());
        PACL newDacl = nullptr;
        if(SetEntriesInAclW(1, &ea, dacl, &newDacl) == ERROR_SUCCESS) {
            SetNamedSecurityInfoW(const_cast<LPWSTR>(p.c_str()), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, newDacl, nullptr);
            LocalFree(newDacl);
        }
    }
    LocalFree(sd);
}

void remove_deny(const std::filesystem::path& p, DWORD mask) {
    if(!everyone())
        return;
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    if(GetNamedSecurityInfoW(p.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &dacl, nullptr, &sd) != ERROR_SUCCESS)
        return;
    bool removedAny = false;
    ACL_SIZE_INFORMATION info;
    if(dacl && GetAclInformation(dacl, &info, sizeof(info), AclSizeInformation)) {
        for(DWORD i = info.AceCount; i > 0; i--) { // Backwards, since deleting shifts the later entries
            void* ace = nullptr;
            if(GetAce(dacl, i - 1, &ace) && is_our_deny_ace(ace, mask) && DeleteAce(dacl, i - 1))
                removedAny = true;
        }
    }
    if(removedAny)
        SetNamedSecurityInfoW(const_cast<LPWSTR>(p.c_str()), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, dacl, nullptr);
    LocalFree(sd);
}
#endif

bool is_canvas_file(const std::filesystem::path& p) {
    std::error_code ec;
    return p.extension() == CANVAS_EXTENSION && std::filesystem::is_regular_file(p, ec);
}

}

void protect_folder(const std::filesystem::path& folder) {
#ifdef _WIN32
    add_deny(folder, FILE_DELETE_CHILD);
#endif
}

void unprotect_folder(const std::filesystem::path& folder) {
#ifdef _WIN32
    remove_deny(folder, FILE_DELETE_CHILD);
#endif
}

void protect_file(const std::filesystem::path& file) {
#ifdef _WIN32
    if(is_canvas_file(file))
        add_deny(file, DELETE);
#endif
}

void unprotect_file(const std::filesystem::path& file) {
#ifdef _WIN32
    std::error_code ec;
    if(std::filesystem::exists(file, ec))
        remove_deny(file, DELETE);
#endif
}

void protect_all(const std::filesystem::path& folder) {
    std::error_code ec;
    if(folder.empty() || !std::filesystem::is_directory(folder, ec))
        return;
    protect_folder(folder);
    for(auto& entry : std::filesystem::directory_iterator(folder, ec)) {
        if(is_canvas_file(entry.path()))
            protect_file(entry.path());
    }
}

void unprotect_all(const std::filesystem::path& folder) {
    std::error_code ec;
    if(folder.empty() || !std::filesystem::is_directory(folder, ec))
        return;
    for(auto& entry : std::filesystem::directory_iterator(folder, ec)) {
        if(is_canvas_file(entry.path()))
            unprotect_file(entry.path());
    }
    unprotect_folder(folder);
}

bool is_in_folder(const std::filesystem::path& file, const std::filesystem::path& folder) {
    if(file.empty() || folder.empty())
        return false;
    std::error_code ec;
    bool same = std::filesystem::equivalent(file.parent_path(), folder, ec);
    return !ec && same;
}

bool move_to_recycle_bin(const std::filesystem::path& file, std::string& errorOut) {
    std::error_code ec;
    if(!std::filesystem::exists(file, ec)) {
        errorOut = "The file doesn't exist";
        return false;
    }
    unprotect_file(file);
#ifdef _WIN32
    std::wstring from = std::filesystem::absolute(file, ec).wstring();
    from.push_back(L'\0'); // The list of files must end with two zeros
    SHFILEOPSTRUCTW op{};
    op.wFunc = FO_DELETE;
    op.pFrom = from.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    int result = SHFileOperationW(&op);
    if(result != 0 || op.fAnyOperationsAborted) {
        protect_file(file); // Still there, so protect it again
        errorOut = "Windows couldn't move it to the Recycle Bin (error " + std::to_string(result) + ")";
        return false;
    }
    return true;
#else
    if(!std::filesystem::remove(file, ec)) {
        errorOut = ec.message();
        return false;
    }
    return true;
#endif
}

}
