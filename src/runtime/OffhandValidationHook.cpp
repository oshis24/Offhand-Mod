#include "runtime/OffhandValidationHook.hpp"

#include <android/log.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>

#include <pl/memory/Patch.hpp>
#include <pl/memory/Signature.hpp>

namespace levioffhand::runtime {
namespace {

constexpr char kMinecraftLibrary[]="libminecraftpe.so";
constexpr char kLogTag[]="Levi Offhand";

/* Minecraft Bedrock Android 1.26.45.1
 * Build ID: 868e275cb295e9a275bb29d2258edc2f7dc48761
 *
 * Item::Item default flag initialization:
 *   RVA F65A3BC  mov w9,#0x50
 *   ...
 *   RVA F65A3E0  strh w8,[x21,#0x2a]
 *
 * x21 == this + 0xe8 at the store, therefore +0x2a == Item +0x112.
 * Item::getAllowOffHand reads bit 7 (0x80) from that 16-bit field.
 * Changing 0x50 -> 0xD0 preserves every existing default bit and adds 0x80.
 */
constexpr std::uintptr_t kItemDefaultFlagsRva=0xF65A3BC;
constexpr std::uintptr_t kPatchOffsetFromSignature=0x0C;

constexpr char kItemConstructorFlagSignature[]=
    "08 DA 94 94 "
    "00 E4 00 6F "
    "F5 03 13 AA "
    "09 0A 80 52 "
    "A0 8E 8E 3C "
    "A8 56 40 79 "
    "A0 C2 00 91 "
    "A0 A2 81 3C "
    "08 19 17 12 "
    "A0 06 80 3D "
    "08 01 09 2A "
    "BF 2E 00 B9";

constexpr char kVanillaItemFlagsInstruction[]="09 0A 80 52";
constexpr char kAllOffhandItemFlagsInstruction[]="09 1A 80 52";
constexpr char kAllOffhandPatchName[]="levi_offhand.item_allow_offhand";

[[nodiscard]] bool belongsToMinecraft(std::uintptr_t address) noexcept {
    if(address==0) return false;
    Dl_info info{};
    if(dladdr(reinterpret_cast<void*>(address),&info)==0 || !info.dli_fname) {
        return false;
    }
    return std::strstr(info.dli_fname,kMinecraftLibrary)!=nullptr;
}

[[nodiscard]] bool bytesEqual(
    std::uintptr_t address,
    const std::array<std::uint8_t,4>& expected
) noexcept {
    const auto actual=pl::memory::readBytes(address,expected.size());
    return actual.size()==expected.size()
        && std::memcmp(actual.data(),expected.data(),expected.size())==0;
}

constexpr std::array<std::uint8_t,4> kVanillaBytes{0x09,0x0A,0x80,0x52};
constexpr std::array<std::uint8_t,4> kPatchedBytes{0x09,0x1A,0x80,0x52};

} // namespace

OffhandValidationHook& OffhandValidationHook::instance() noexcept {
    static OffhandValidationHook value;
    return value;
}

bool OffhandValidationHook::applyNativeItemPatch() noexcept {
    if(mItemFlagsInstruction==0) return false;
    if(mPatchApplied.load(std::memory_order_acquire)) return true;

    if(bytesEqual(mItemFlagsInstruction,kPatchedBytes)) {
        mPatchApplied.store(true,std::memory_order_release);
        return true;
    }
    if(!bytesEqual(mItemFlagsInstruction,kVanillaBytes)) {
        __android_log_print(
            ANDROID_LOG_ERROR,kLogTag,
            "[NativeItemOffhand] refusing patch: unexpected bytes at Item flags instruction"
        );
        return false;
    }

    const bool ok=pl::memory::writeBytes(
        mItemFlagsInstruction,
        kAllOffhandItemFlagsInstruction,
        kAllOffhandPatchName
    );
    mPatchApplied.store(ok,std::memory_order_release);
    if(ok) {
        __android_log_print(
            ANDROID_LOG_INFO,kLogTag,
            "[NativeItemOffhand] Item::Item default flags 0x50 -> 0xD0; mAllowOffHand bit is native"
        );
    }
    return ok;
}

void OffhandValidationHook::revertNativeItemPatch() noexcept {
    if(!mPatchApplied.exchange(false,std::memory_order_acq_rel)) return;
    if(!pl::memory::revertPatch(kAllOffhandPatchName)) {
        __android_log_print(
            ANDROID_LOG_WARN,kLogTag,
            "[NativeItemOffhand] patch revert reported failure"
        );
    }
}

bool OffhandValidationHook::install(pl::mod::ModContext& context) noexcept {
    if(installed()) return true;

    mItemFlagsInstruction=0;
    const auto signatureBase=pl::memory::resolveSignature(
        kItemConstructorFlagSignature,
        kMinecraftLibrary
    );
    if(!belongsToMinecraft(signatureBase)) {
        context.logger().error(
            "Levi Offhand v0.2.60: Item constructor flag signature resolution failed"
        );
        return false;
    }

    mItemFlagsInstruction=signatureBase+kPatchOffsetFromSignature;
    if(!bytesEqual(mItemFlagsInstruction,kVanillaBytes)
        && !bytesEqual(mItemFlagsInstruction,kPatchedBytes)) {
        context.logger().error(
            "Levi Offhand v0.2.60: Item flag instruction fingerprint mismatch"
        );
        mItemFlagsInstruction=0;
        return false;
    }

    mFeatureEnabled.store(true,std::memory_order_release);
    if(!applyNativeItemPatch()) {
        mItemFlagsInstruction=0;
        return false;
    }

    context.logger().info(
        "[NativeItemOffhand] single native Item policy patch active at 0x{:x} (target RVA 0x{:x})",
        mItemFlagsInstruction,
        kItemDefaultFlagsRva
    );
    context.logger().info(
        "[NativeItemOffhand] legacy manual/autoAdd/transfer/swap/getAllowOffHand hooks removed"
    );
    return true;
}

void OffhandValidationHook::uninstall(pl::mod::ModContext& context) noexcept {
    revertNativeItemPatch();
    mItemFlagsInstruction=0;
    context.logger().info(
        "[NativeItemOffhand] constructor patch removed; already-constructed Item singletons keep their current flags until process restart"
    );
}

void OffhandValidationHook::setFeatureEnabled(bool enabled) noexcept {
    mFeatureEnabled.store(enabled,std::memory_order_release);
    if(enabled) {
        if(!applyNativeItemPatch()) {
            __android_log_print(
                ANDROID_LOG_ERROR,kLogTag,
                "[NativeItemOffhand] failed to re-enable constructor patch"
            );
        }
    } else {
        revertNativeItemPatch();
        __android_log_print(
            ANDROID_LOG_INFO,kLogTag,
            "[NativeItemOffhand] constructor patch disabled; restart required for complete rollback of existing Item singletons"
        );
    }
}

bool OffhandValidationHook::featureEnabled() const noexcept {
    return mFeatureEnabled.load(std::memory_order_acquire);
}

bool OffhandValidationHook::installed() const noexcept {
    return mItemFlagsInstruction!=0
        && mPatchApplied.load(std::memory_order_acquire);
}

} // namespace levioffhand::runtime
