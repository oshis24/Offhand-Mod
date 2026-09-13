from pathlib import Path
import os
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/runtime/OffhandValidationHook.cpp").read_text()
HEADER = (ROOT / "src/runtime/OffhandValidationHook.hpp").read_text()
MANIFEST = (ROOT / "manifest.json").read_text()
BUILD_SH = (ROOT / "scripts/build.sh").read_text()
WORKFLOW = (ROOT / ".github/workflows/build.yml").read_text()

required = {
    "native Item constructor signature": "kItemConstructorFlagSignature",
    "constructor default flag RVA": "kItemDefaultFlagsRva=0xF65A3BC",
    "native old instruction": 'kVanillaItemFlagsInstruction[]="09 0A 80 52"',
    "native patched instruction": 'kAllOffhandItemFlagsInstruction[]="09 1A 80 52"',
    "native patch name": 'kAllOffhandPatchName[]="levi_offhand.item_allow_offhand"',
    "Patch API write": "pl::memory::writeBytes(",
    "Patch API revert": "pl::memory::revertPatch(",
    "native policy log": "[NativeItemOffhand]",
}
for name, marker in required.items():
    assert marker in SOURCE, f"missing {name}: {marker}"

forbidden = {
    "manual container pre-validation signature": "kManualSetPathSignature",
    "auto-add validation signature": "kAutoAddPathSignature",
    "tryTransfer validation signature": "kTryTransferSignature",
    "trySwap validation signature": "kTrySwapSignature",
    "getAllowOffHand detour signature": "kAllowOffhandSignature",
    "manual validation detour": "manualSetPathDetour(",
    "auto-add validation detour": "autoAddPathDetour(",
    "transfer validation detour": "tryTransferDetour(",
    "swap validation detour": "trySwapDetour(",
    "allowOffhand detour": "allowOffhandDetour(",
    "Hook API dependency": "pl/memory/Hook.hpp",
}
for name, marker in forbidden.items():
    assert marker not in SOURCE, f"legacy {name} still present: {marker}"
    assert marker not in HEADER, f"legacy {name} still declared: {marker}"

assert '"version": "0.2.60"' in MANIFEST
assert "levi-offhand-v0.2.60.levipack" in BUILD_SH
assert "levi-offhand-v0.2.60.levipack" in WORKFLOW
assert "levi-offhand-arm64-v0.2.60" in WORKFLOW

lib_path = os.environ.get("LEVI_MCPE_LIBRARY")
if lib_path:
    data = Path(lib_path).read_bytes()
    patch_rva = 0xF65A3BC
    old = bytes.fromhex("09 0A 80 52")
    new = bytes.fromhex("09 1A 80 52")
    assert data[patch_rva:patch_rva+4] == old, (
        f"unexpected Item default flags instruction at {patch_rva:#x}: "
        f"{data[patch_rva:patch_rva+4].hex(' ')}"
    )
    assert new != old

    # 48-byte window from the supplied target is unique.  The implementation
    # resolves the beginning of this window and applies the fixed +0x0c patch.
    sig_rva = 0xF65A3B0
    signature = data[sig_rva:sig_rva+48]
    assert data.count(signature) == 1, "Item constructor patch signature is not unique"

print("v0.2.60 native Item mAllowOffHand constructor contract passed")
