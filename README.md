# llvm-hardening-pass

An LLVM module pass that audits functions for compiler security hardening attributes — the IR-level counterpart to [hardening-check](https://github.com/IliasSoultana/hardening-check).

## What it checks

| Attribute | Compiler flag | What it protects against |
|---|---|---|
| **SSP** (`ssp` / `sspstrong` / `sspreq`) | `-fstack-protector-strong` | Stack buffer overflows |
| **SafeStack** | `-fsanitize=safe-stack` | Stack-based control-flow hijacking |
| **ShadowCallStack** | `-fsanitize=shadow-call-stack` | Return address overwrite |

`hardening-check` reads a final ELF binary. This pass runs **before** linking, at the IR stage, so you catch gaps earlier in the build pipeline — before the binary even exists.

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
| NX stack (no `PF_X` on `PT_GNU_STACK`) | not checked at IR level — linker concern |
| PIE (`ET_DYN` e_type) | not checked at IR level — linker concern |
