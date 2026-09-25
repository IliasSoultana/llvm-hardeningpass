# llvm-hardening-pass

[![CI](https://github.com/IliasSoultana/llvm-hardeningpass/actions/workflows/ci.yml/badge.svg)](https://github.com/IliasSoultana/llvm-hardeningpass/actions/workflows/ci.yml)

An LLVM module pass that audits functions for compiler security hardening attributes. The IR-level counterpart to [hardening-check](https://github.com/IliasSoultana/hardening-check).

## What it checks

| Attribute | Compiler flag | What it protects against |
|---|---|---|
| **SSP** (`ssp` / `sspstrong` / `sspreq`) | `-fstack-protector-strong` | Stack buffer overflows |
| **SafeStack** | `-fsanitize=safe-stack` | Stack-based control-flow hijacking |
| **ShadowCallStack** | `-fsanitize=shadow-call-stack` | Return address overwrite |

`hardening-check` reads a final ELF binary. This pass runs **before** linking, at the IR stage, so you catch gaps earlier in the build pipeline, before the binary even exists.

## Requirements

- LLVM 17+ (`llvm-config`, `opt` in `$PATH`)
- CMake 3.20+
- A C++17 compiler

Install on Ubuntu/Debian:
```bash
sudo apt install llvm-17-dev llvm-17-tools
```

Install on macOS via Homebrew:
```bash
brew install llvm
export PATH="$(brew --prefix llvm)/bin:$PATH"
```

## Build

```bash
mkdir build && cd build
cmake .. -DLLVM_DIR="$(llvm-config --cmakedir)"
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

The plugin is built at `build/HardeningAuditPass.so` (Linux) or `build/HardeningAuditPass.dylib` (macOS).

## Run

```bash
# Against the bundled test IR
opt -load-pass-plugin ./build/HardeningAuditPass.so \
    -passes="hardening-audit" \
    -disable-output \
    test/example.ll
```

Expected output:

```
=== hardening-audit: test/example.ll ===
function                                  SSP    SafeStack   ShadowCS   calls
------------------------------------------------------------------------
process_input                             YES    -           -          1
safe_handler                              NO     yes         -          1
legacy_parser                             NO     -           -          2
fatal_error                               NO     -           -          1

Functions audited : 4
Missing SSP       : 3
WARNING: 3 function(s) compiled without stack-smashing protection.
```

### Against your own code

```bash
# Emit LLVM IR from a C file
clang -O2 -fstack-protector-strong -emit-llvm -S -o my_program.ll my_program.c

# Run the audit
opt -load-pass-plugin ./build/HardeningAuditPass.so \
    -passes="hardening-audit" \
    -disable-output \
    my_program.ll
```

## How it works

The pass is built on LLVM's **new pass manager** (introduced in LLVM 13, now the default). It registers as a `ModulePass` so it sees all functions at once.

For each function definition (skipping declarations) it reads the function's attribute set and counts `CallInst` / `InvokeInst` nodes across all basic blocks. The attribute checks map directly to the ELF-level properties that `hardening-check` reads from the binary:

| ELF check (hardening-check) | IR check (this pass) |
|---|---|
| Stack canary (`__stack_chk_fail` in `.dynsym`) | `ssp` / `sspstrong` / `sspreq` attribute |
| NX stack (no `PF_X` on `PT_GNU_STACK`) | not checked at IR level, linker concern |
| PIE (`ET_DYN` e_type) | not checked at IR level, linker concern |

## Why IR and not the binary

`hardening-check` answers "was the canary machinery linked in". This answers
"which functions actually got it", and those are different questions.

`-fstack-protector` instruments only functions with character arrays.
`-fstack-protector-strong` widens that considerably, but still not to
everything. In both cases the linked binary contains `__stack_chk_fail` and an
ELF-level scanner reports a canary, while individual functions holding
exploitable stack buffers may carry no `ssp` attribute at all. That gap is
invisible after linking and obvious here.

## Limitations

- **Attributes, not codegen.** The pass reads what the frontend recorded on
  each function. It does not verify that the backend emitted a guard, nor
  where the canary sits relative to locals.
- **Module scope only.** It sees one translation unit's IR. A program is
  hardened only if every unit is, including vendored static libraries that
  never pass through this pass.
- **PIE and NX are out of reach.** Both are decided at link time, so no
  IR-level pass can see them; the ELF scanners cover that half.
- **SafeStack and ShadowCallStack are reported, not judged.** Their absence is
  normal; almost nothing enables them. Only missing SSP is counted as a
  warning.
- **LLVM 17+.** The pass uses the new pass manager plugin interface and is
  built against the host's LLVM; version drift between the build and the `opt`
  you run it under will fail to load.

## Related

- [hardening-check](https://github.com/IliasSoultana/hardening-check), the same question on the finished ELF
- [elfharden](https://github.com/IliasSoultana/elfharden) · [elfharden-rs](https://github.com/IliasSoultana/elfharden-rs), Go and Rust implementations of the scanner
- [diversity-poc](https://github.com/IliasSoultana/diversity-poc), compiler-level layout diversification
