// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <boost/optional.hpp>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/crypto/key_manager.h"
#include "core/file_sys/vfs.h"

namespace Loader {
enum class ResultStatus : u16;
}

namespace FileSys {

union NCASectionHeader;

enum class NCAContentType : u8 {
    Program = 0,
    Meta = 1,
    Control = 2,
    Manual = 3,
    Data = 4,
    Data_Unknown5 = 5, ///< Seems to be used on some system archives
};

enum class NCASectionCryptoType : u8 {
    NONE = 1,
    XTS = 2,
    CTR = 3,
    BKTR = 4,
};

struct NCASectionTableEntry {
    u32_le media_offset;
    u32_le media_end_offset;
    INSERT_PADDING_BYTES(0x8);
};
static_assert(sizeof(NCASectionTableEntry) == 0x10, "NCASectionTableEntry has incorrect size.");

struct NCAHeader {
    std::array<u8, 0x100> rsa_signature_1;
    std::array<u8, 0x100> rsa_signature_2;
    u32_le magic;
    u8 is_system;
    NCAContentType content_type;
    u8 crypto_type;
    u8 key_index;
    u64_le size;
    u64_le title_id;
    INSERT_PADDING_BYTES(0x4);
    u32_le sdk_version;
    u8 crypto_type_2;
    INSERT_PADDING_BYTES(15);
    std::array<u8, 0x10> rights_id;
    std::array<NCASectionTableEntry, 0x4> section_tables;
    std::array<std::array<u8, 0x20>, 0x4> hash_tables;
    std::array<u8, 0x40> key_area;
    INSERT_PADDING_BYTES(0xC0);
};
static_assert(sizeof(NCAHeader) == 0x400, "NCAHeader has incorrect size.");

inline bool IsDirectoryExeFS(const std::shared_ptr<VfsDirectory>& pfs) {
    // According to switchbrew, an exefs must only contain these two files:
    return pfs->GetFile("main") != nullptr && pfs->GetFile("main.npdm") != nullptr;
}

bool IsValidNCA(const NCAHeader& header);

// An implementation of VfsDirectory that represents a Nintendo Content Archive (NCA) conatiner.
// After construction, use GetStatus to determine if the file is valid and ready to be used.
class NCA : public ReadOnlyVfsDirectory {
public:
    explicit NCA(VirtualFile file, VirtualFile bktr_base_romfs = nullptr,
                 u64 bktr_base_ivfc_offset = 0);
    Loader::ResultStatus GetStatus() const;

    std::vector<std::shared_ptr<VfsFile>> GetFiles() const override;
    std::vector<std::shared_ptr<VfsDirectory>> GetSubdirectories() const override;
    std::string GetName() const override;
    std::shared_ptr<VfsDirectory> GetParentDirectory() const override;

    NCAContentType GetType() const;
    u64 GetTitleId() const;
    bool IsUpdate() const;

    VirtualFile GetRomFS() const;
    VirtualDir GetExeFS() const;

    VirtualFile GetBaseFile() const;

    // Returns the base ivfc offset used in BKTR patching.
    u64 GetBaseIVFCOffset() const;

protected:
    bool ReplaceFileWithSubdirectory(VirtualFile file, VirtualDir dir) override;

private:
    u8 GetCryptoRevision() const;
    boost::optional<Core::Crypto::Key128> GetKeyAreaKey(NCASectionCryptoType type) const;
    boost::optional<Core::Crypto::Key128> GetTitlekey();
    VirtualFile Decrypt(NCASectionHeader header, VirtualFile in, u64 starting_offset);

    std::vector<VirtualDir> dirs;
    std::vector<VirtualFile> files;

    VirtualFile romfs = nullptr;
    VirtualDir exefs = nullptr;
    VirtualFile file;
    VirtualFile bktr_base_romfs;
    u64 ivfc_offset;

    NCAHeader header{};
    bool has_rights_id{};

    Loader::ResultStatus status{};

    bool encrypted;
    bool is_update;

    Core::Crypto::KeyManager keys;
};

} // namespace FileSys
