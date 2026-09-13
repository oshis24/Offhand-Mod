# Native Item Offhand Flag Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace five scoped container/getAllowOffHand detours with one native Item-constructor flag patch so every Item is born with `mAllowOffHand=true` on Minecraft Bedrock 1.26.45.1.

**Architecture:** Keep the public `OffhandValidationHook` lifecycle API so the rest of the mod does not churn, but replace its implementation with one four-byte patch inside `Item::Item`. The patch changes the constructor's default bitfield constant from `0x50` to `0xD0`, which adds bit `0x80` (`mAllowOffHand`) while preserving the existing bits. Visual code remains unchanged for this diagnostic release.

**Tech Stack:** C++20, Levi preloader Patch/Signature APIs, Python source/binary contracts, GitHub Actions Android NDK 28.2.13676358.

**Spec:** Community recommendation accepted in chat: stop bypassing `ContainerValidation`; set native `Item::mAllowOffHand` at construction time.

## Global Constraints

- Target Minecraft: 1.26.45.1, Build ID `868e275cb295e9a275bb29d2258edc2f7dc48761`.
- Constructor patch site: RVA `0xF65A3BC`, original instruction `09 0A 80 52` (`mov w9,#0x50`).
- Patched instruction: `09 1A 80 52` (`mov w9,#0xD0`).
- Remove manual-set, auto-add, transfer, swap, and `getAllowOffHand` detours from runtime storage implementation.
- Do not alter Bow/Trident render fixes in this release.
- Update package version to 0.2.60.

---

### Task 1: Native constructor policy contract

**Files:**
- Create: `tests/v0260_native_item_flag_contract.py`
- Modify: `tests/run_native_attachment_fix_tests.sh`

**Interfaces:**
- Consumes: `src/runtime/OffhandValidationHook.cpp`, target `libminecraftpe.so` when `LEVI_MCPE_LIBRARY` is supplied.
- Produces: a source/binary contract that rejects legacy validation hooks and validates the constructor patch bytes.

- [ ] **Step 1: Write the failing contract** asserting constructor signature/patch markers and forbidding old validation-hook markers.
- [ ] **Step 2: Run it against v0.2.59 and confirm failure because the constructor patch is absent.**
- [ ] **Step 3: Keep this contract in the normal native test runner.**

### Task 2: Replace scoped validation architecture

**Files:**
- Modify: `src/runtime/OffhandValidationHook.hpp`
- Modify: `src/runtime/OffhandValidationHook.cpp`

**Interfaces:**
- Consumes: existing `install`, `uninstall`, `setFeatureEnabled`, `featureEnabled`, `installed` API.
- Produces: same lifecycle API backed by one named memory patch.

- [ ] **Step 1: Resolve a unique signature around RVA `0xF65A3B0`.**
- [ ] **Step 2: Verify the resolved patch site contains exactly `09 0A 80 52`.**
- [ ] **Step 3: Apply `09 1A 80 52` via the Patch API under one stable patch name.**
- [ ] **Step 4: Revert that patch on disable/unload and make toggle reapply/revert the instruction for future Item constructions.**
- [ ] **Step 5: Log the resolved patch site and explicitly state that existing Item singletons retain whatever flag they were constructed with until process restart.**

### Task 3: Release metadata and regression verification

**Files:**
- Modify: `manifest.json`
- Modify: `scripts/build.sh`
- Modify: `.github/workflows/build.yml`
- Modify: `README.md`

**Interfaces:**
- Produces: v0.2.60 package/artifact names and runtime test instructions.

- [ ] **Step 1: Set version/package names to `0.2.60`.**
- [ ] **Step 2: Document diagnostic cases: stone transfer, chest transfer, swap, crafting output, pickup/reconnect, Bow/Trident visual regression.**
- [ ] **Step 3: Run the full native test suite with the supplied 1.26.45.1 binary.**
- [ ] **Step 4: Run host syntax checks for all project C++ sources using the existing stubs.**
- [ ] **Step 5: Package full-repo and changed-only ZIPs.**
