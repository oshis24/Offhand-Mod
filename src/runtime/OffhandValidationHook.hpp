#pragma once

#include <atomic>
#include <cstdint>

#include <pl/Mod.hpp>

namespace levioffhand::runtime {

/*
 * Legacy class name retained to avoid churn in LeviOffhandMod.
 * v0.2.60 no longer hooks ContainerValidation/getAllowOffHand.
 * It patches Item::Item's default native flag word so bit 0x80
 * (mAllowOffHand) is set for every Item constructed afterward.
 */
class OffhandValidationHook final {
public:
    static OffhandValidationHook& instance() noexcept;
    ~OffhandValidationHook() = default;

    bool install(pl::mod::ModContext& context) noexcept;
    void uninstall(pl::mod::ModContext& context) noexcept;
    void setFeatureEnabled(bool enabled) noexcept;

    [[nodiscard]] bool featureEnabled() const noexcept;
    [[nodiscard]] bool installed() const noexcept;

private:
    OffhandValidationHook() = default;

    bool applyNativeItemPatch() noexcept;
    void revertNativeItemPatch() noexcept;

    std::uintptr_t mItemFlagsInstruction{0};
    std::atomic_bool mFeatureEnabled{true};
    std::atomic_bool mPatchApplied{false};
};

} // namespace levioffhand::runtime
