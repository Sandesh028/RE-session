# RE-session
# Ghidra / Reverse Engineering Workshop — Student Lab Guide

> A single-file reference for the `vuln_selftrigger` exercise: setup, files, commands, step-by-step analysis (GDB + pwndbg), mapping dynamic evidence to static analysis (Ghidra), computing offsets, and mitigation demo. Save this as `RE_Workshop.md` and distribute to students.

---

# 1 — Overview (what you'll learn)

* Build and run a tiny vulnerable program that demonstrates a **stack-based buffer overflow**.
* Generate a deterministic payload that overwrites saved frame data and causes a crash.
* Use **GDB / pwndbg** to inspect registers, stack, and memory; locate the payload marker.
* Use **Ghidra** to inspect the binary statically (decompiler, functions, stack layout).
* Correlate dynamic evidence (addresses, marker) with static layout to compute overwrite offsets.
* Demonstrate mitigations and discuss secure coding practices.

---

# 2 — Files provided / produced

* `vuln_selftrigger.c` — single C file that can self-trigger the overflow (`--self`) or accept external input.
* `vuln_selftrigger` — compiled binary (if built).
* `/tmp/payload.bin` — payload written at runtime for GDB testing.
* `ghidra_workshop_single/out/` — (if using script) contains: `vuln`, `benign`, `payload.bin`, `gdb_crash.txt`, `run_output.txt`, and sources.
* `gdb_crash.txt` — batch-captured GDB output for offline study (optional).

---

# 3 — Quick setup (one-liners)

On Linux (recommended):

```bash
# compile (from lab folder)
gcc -g -O0 -fno-stack-protector -no-pie vuln_selftrigger.c -o vuln_selftrigger

# make sure core dumps allowed (optional)
ulimit -c unlimited
```

Run & trigger self-payload:

```bash
./vuln_selftrigger --self
# OR supply external payload:
./vuln_selftrigger "$(python3 -c "print('A'*80 + 'CRASH')")"
```

---

# 4 — Single-step script (what it does)

If you prefer a single script that creates sources, builds, creates payload and runs gdb in batch, use `prepare_and_run.sh` (example in class). It writes outputs into `./ghidra_workshop_single/out/` for you to import into Ghidra.

---

# 5 — Lab stages — step-by-step instructor / student workflow

## Stage A — Reproduce the crash

1. Ensure binary built as above.
2. Run:

   ```bash
   ./vuln_selftrigger --self
   ```
3. Expected:

   * A line indicating the program called `vuln()` with a long payload.
   * Partial printed buffer (e.g., `You said: AAAAA...`) followed by `Segmentation fault (core dumped)`.

Explain: crash = memory corruption; we overwrite saved frame data on stack.

---

## Stage B — Quick static checks

```bash
file vuln_selftrigger
strings vuln_selftrigger | grep -n "Congratulations\|CRASH\|vuln"
hexdump -C /tmp/payload.bin | head -n 8
```

Explain outputs:

* `file` shows architecture (ELF 64-bit LSB etc.).
* `strings` reveals `secret()` textual artifacts.
* `hexdump` shows the `A` pattern + `CRASH` marker.

---

## Stage C — Interactive GDB / pwndbg investigation (recommended)

Start gdb:

```bash
gdb -q --args ./vuln_selftrigger --self
```

Inside gdb, run these commands step-by-step (explain each):

1. Run program:

   ```
   (gdb) run
   ```

   Observe SIGSEGV/crash.

2. Backtrace + full info:

   ```
   (gdb) bt full
   (gdb) info registers
   ```

   Explain: `bt full` shows call stack + local variables; `info registers` lists CPU registers.

3. Inspect instruction at crash:

   ```
   (gdb) x/i $rip
   (gdb) disassemble $rip-32, $rip+32
   ```

4. Inspect stack & find marker:

   ```
   (gdb) x/256bx $rsp            # dump 256 bytes at RSP (byte view)
   (gdb) find $rsp-0x400, $rsp+0x800, "CRASH"
   (gdb) x/s <address_from_find>   # print as string
   ```

5. If you re-run with a breakpoint at `vuln` before `strcpy`, capture addresses:

   ```
   (gdb) break vuln
   (gdb) run --self
   (gdb) p &buf
   (gdb) p/x $rbp
   (gdb) x/40bx (char *)&buf
   ```

Use outputs to compute offsets (see Stage E).

---

## Stage D — Batch GDB (create a reproducible log)

To produce a file `gdb_crash.txt` to hand out:

```bash
gdb -q --batch \
  -ex "set pagination off" \
  -ex "run" \
  -ex "bt full" \
  -ex "info registers" \
  -ex "x/200bx \$rsp" \
  -ex "find \$rsp,\$rsp+0x400, \"CRASH\"" \
  --args ./vuln_selftrigger --self \
  &> gdb_crash.txt || true

sed -n '1,240p' gdb_crash.txt
```

---

## Stage E — Correlate dynamic (GDB) to static (Ghidra) & compute offsets

1. **From GDB** (while stopped before overflow or from batch):

   * `p &buf` → address of buffer start (e.g., `0x7ffc...e2a0`)
   * `find ... "CRASH"` → address where marker appears (e.g., `0x7ffc...e2f0`)
   * Compute `offset = addr_CRASH - addr_buf` (decimal) — bytes from buffer start to marker.

2. **Saved return address location**:

   * Typically at saved RBP + 8 (x86_64 with frame pointer).
   * Get `$rbp` (from `p $rbp` before crash).
   * `addr_saved_return_loc = $rbp + 8`
   * Compute `bytes_to_return = addr_saved_return_loc - addr_buf`

3. **Interpretation**:

   * If `payload` length >= `bytes_to_return`, the saved return address will be overwritten.
   * The marker location tells you exactly how far the payload reached.

4. **On Ghidra side**:

   * Import binary into a project, run analysis.
   * Open `vuln()` in decompiler and listing.
   * Identify `char buf[64]` and how the stack frame is laid out (stack offsets).
   * Ghidra's assembly will show access patterns like `sub rsp, 0x?` and `mov QWORD PTR [rbp-0x?], ...`.
   * Match `&buf` (GDB) to the offsets shown in disassembly to confirm the computed distances.

---

# 6 — Registers explained (concise, for students)

(As will appear in `info registers` / pwndbg)

* `RIP` — instruction pointer (where CPU executes). Crash location is `RIP`.
* `RSP` — stack pointer (top of the stack).
* `RBP` — frame/base pointer (points into stack frame). Saved `RBP` overwritten? indicates overflow hit frame data.
* `RDI, RSI, RDX, RCX, R8, R9` — argument registers (on x86_64 SysV: RDI = arg1, RSI = arg2, RDX = arg3, ...).
* `RAX` — accumulator / return value register.
* `RBX, R12-R15` — callee-saved registers.
* `EFLAGS` — CPU flags (status).
* Segment regs (`cs`, `ss`) — generally constant on Linux userland.

**Key diagnostic note:** `RBP == 0x414141...` (ASCII 'A') = evidence saved frame pointer was overwritten by payload.

---

# 7 — GDB / pwndbg useful commands (cheat sheet)

```gdb
# run & breakpoints
(gdb) break vuln
(gdb) run --self

# backtrace + info
(gdb) bt full
(gdb) info registers
(gdb) x/i $rip
(gdb) disassemble $rip-32, $rip+32

# memory & stack inspection
(gdb) x/256bx $rsp                 # bytes from rsp
(gdb) x/40gx $rbp                  # 8-byte words at rbp
(gdb) find $rsp-0x400, $rsp+0x800, "CRASH"
(gdb) x/s 0x7ffc....               # print string at address

# addresses & arithmetic
(gdb) p/x &buf
(gdb) p/x $rbp
(gdb) p/x ((char*)addr_CRASH - (char*)&buf)   # compute offset
```

---

# 8 — Compute offsets example (template)

1. `addr_buf = (from p &buf)`
2. `addr_CRASH = (from find "CRASH")`
3. `offset_marker = addr_CRASH - addr_buf` (decimal)
4. `addr_rbploc = $rbp + 8` (saved return address location)
5. `bytes_to_return = addr_rbploc - addr_buf`
6. If payload length (`len(payload)`) ≥ `bytes_to_return`, you overwrite the saved return address.

---

# 9 — Mitigation demo (rebuild & compare)

Rebuild with protections and observe different behavior:

1. Stack protector:

```bash
gcc -g -O0 -fstack-protector-strong vuln_selftrigger.c -o vuln_canary
./vuln_canary --self
# Expect: program abort with stack-smash detected (or protected behavior)
```

2. PIE / ASLR:

```bash
gcc -g -O0 -fpie -pie vuln_selftrigger.c -o vuln_pie
# Addresses randomized — less reproducible exploitation
```

3. Compile-time safe coding:

* Replace `strcpy(buf, data)` with `snprintf(buf, sizeof(buf), "%s", data)` or `strlcpy` (if available) and show crash prevented.

---

# 10 — Discussion questions (for students)

* Why is `strcpy` unsafe? What minimal change prevents overflow here?
* How do stack canaries and ASLR increase attacker effort?
* If you wanted to redirect control to `secret()`, which address must you overwrite and how would you find it in Ghidra?
* Why do we compile with `-fno-stack-protector -no-pie` for teaching? Why *not* in production?

---

# 11 — Ethics & safety

* This lab is strictly for **learning** on controlled, isolated systems you own or where you have explicit permission.
* Do **not** apply these techniques against external systems or services.
* Discuss responsible disclosure practices if vulnerabilities are found in real-world software.

---

# 12 — Troubleshooting (common issues)

* **GDB on macOS**: macOS requires code signing for `gdb`; prefer Linux for the interactive parts.
* **No `CRASH` found**: confirm payload was written to `/tmp/payload.bin` and used by the program.
* **Symbols missing in Ghidra**: ensure binary compiled with `-g` (debug symbols).
* **Different stack layouts**: compiler/optimization and ABI details can change offsets — rely on dynamic address measurement (`p &buf`, `p $rbp`).

---

# 13 — Homework / extensions

* Rebuild `vuln_selftrigger` with different combinations of mitigations; document behavior.
* Use Ghidra to annotate assembly and produce a diagram of the stack layout showing `buf`, saved `rbp`, return address and marker locations (with numeric addresses).
* (Advanced) Try crafting a payload that overwrites return address with the address of `secret()` — do this **only** in the lab and discuss conceptual steps (no public exploit code).

---

# 14 — Quick reference snippets (copy/paste)

### Build

```bash
gcc -g -O0 -fno-stack-protector -no-pie vuln_selftrigger.c -o vuln_selftrigger
```

### Run & produce gdb batch log

```bash
gdb -q --batch \
  -ex "set pagination off" \
  -ex "run" \
  -ex "bt full" \
  -ex "info registers" \
  -ex "x/200bx \$rsp" \
  -ex "find \$rsp,\$rsp+0x400, \"CRASH\"" \
  --args ./vuln_selftrigger --self \
  &> gdb_crash.txt || true

less gdb_crash.txt
```

### Ghidra import

* Open `ghidraRun` → New Project → Import `vuln_selftrigger` → Analyze → Inspect `vuln()` and `secret()`.

---


