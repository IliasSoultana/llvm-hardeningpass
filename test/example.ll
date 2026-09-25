; test/example.ll
;
; Minimal LLVM IR with a mix of hardened and unhardened functions.
; Run with:
;   opt -load-pass-plugin ../build/HardeningAuditPass.so \
;       -passes="hardening-audit" -disable-output example.ll

; ── hardened function ────────────────────────────────────────────────────────
; Compiled with -fstack-protector-strong: has 'sspstrong' attribute.
define i32 @process_input(ptr %buf, i32 %len) #0 {
entry:
  %result = alloca i32, align 4
  store i32 0, ptr %result, align 4
  %cmp = icmp sgt i32 %len, 0
  br i1 %cmp, label %do_work, label %done

do_work:
  %call = call i32 @validate(ptr %buf, i32 %len)
  store i32 %call, ptr %result, align 4
  br label %done

done:
  %ret = load i32, ptr %result, align 4
  ret i32 %ret
}

; ── another hardened function (safestack) ───────────────────────────────────
define void @safe_handler(i64 %code) #1 {
entry:
  %call = call i32 @log_event(i64 %code)
  ret void
}

; ── unhardened function, no SSP ────────────────────────────────────────────
; Compiled without -fstack-protector: no hardening attributes at all.
; This is what the audit pass should flag.
define i32 @legacy_parser(ptr %data) {
entry:
  %tmp = alloca [64 x i8], align 1
  %call1 = call i32 @strlen(ptr %data)
  %call2 = call ptr @memcpy(ptr %tmp, ptr %data, i64 64)
  ret i32 %call1
}

; ── noreturn function ────────────────────────────────────────────────────────
define void @fatal_error(i32 %code) #2 {
entry:
  call void @exit(i32 %code)
  unreachable
}

; ── declarations (skipped by the pass, not definitions) ────────────────────
declare i32 @validate(ptr, i32)
declare i32 @log_event(i64)
declare i32 @strlen(ptr)
declare ptr @memcpy(ptr, ptr, i64)
declare void @exit(i32)

; ── attribute sets ───────────────────────────────────────────────────────────
attributes #0 = { sspstrong "frame-pointer"="all" }
attributes #1 = { safestack }
attributes #2 = { noreturn }
