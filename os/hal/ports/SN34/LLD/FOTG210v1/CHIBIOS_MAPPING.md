# ChibiOS USB LLD -> FOTG210 device-core mapping

Status: research/design checkpoint. This is not an implementation and does not enable the driver.

The mapping below cross-checks three independent interfaces:

1. ChibiOS USB LLD contract as used by the existing SN32 port;
2. CherryUSB Bouffalo FOTG210-class device implementation (Apache-2.0);
3. Linux Faraday FOTG210 UDC implementation (GPL-2.0+).

The F28 official register map remains authoritative for the eventual code. CherryUSB/Linux are used to establish controller semantics and to catch ambiguous manual interpretation.

## Stable FOTG210 device register model

The Linux Faraday header and the register names used by CherryUSB agree on the classic device block shape:

| Offset | Classic name | Role |
|---:|---|---|
| `0x100` | `DMCR` / device control | chip enable, software reset, global device IRQ, suspend/wakeup policy, HS/FS policy |
| `0x104` | `DAR` | device address; `AFT_CONF` state |
| `0x114` | `PHYTMSR` | USB test/unplug control |
| `0x120` | `DCFESR` / CX config | control FIFO clear/stall/done and FIFO-empty status |
| `0x130` | `DMIGR` | interrupt-group masks |
| `0x134` | `DMISGR0` | control-transfer interrupt masks |
| `0x138` | `DMISGR1` | FIFO endpoint interrupt masks |
| `0x13C` | `DMISGR2` | reset/suspend/resume/DMA/ZLP masks in classic FOTG210 |
| `0x140` | `DIGR` | interrupt-group status |
| `0x144` | `DISGR0` | control-transfer status |
| `0x148` | `DISGR1` | FIFO endpoint status |
| `0x14C` | `DISGR2` | reset/suspend/resume/DMA/ZLP status |
| `0x150` | `RX0BYTE` | received zero-length packet flags |
| `0x154` | `TX0BYTE` | transmitted zero-length packet flags |
| `0x160+` | `INEPMPSR(n)` | IN endpoint MPS, stall, toggle reset |
| `0x180+` | `OUTEPMPSR(n)` | OUT endpoint MPS, stall, toggle reset |
| `0x1A0` | `EPMAP` | endpoint -> FIFO mapping |
| `0x1A8` | `FIFOMAP` | FIFO -> endpoint + direction mapping |
| `0x1AC` | `FIFOCF` | FIFO type/block-size/block-count/enable |
| `0x1B0+` | `FIBCR(n)` | FIFO byte count/reset |
| `0x1C0` | `DMATFNR` | DMA/control-FIFO/FIFO target selection |
| `0x1C8` | `DMACPSR1` | classic DMA length/direction/start/abort |
| `0x1CC` | `DMACPSR2` | classic DMA memory address |
| `0x1D0` | `CXPORT` | control FIFO data port |

SN34F28x also exposes a VDMA extension. Prefer the F28 definition of that extension over assuming the older Linux single-DMA register model.

### Interrupt-mask polarity

The FOTG210 device interrupt mask registers use **1 = masked/disabled, 0 = enabled** for the sources exercised by both donors. Code should use explicit `mask_*()` / `unmask_*()` helpers instead of generic-looking `enable` bit operations, because the polarity is easy to invert.

## Endpoint model: hard architectural constraint

FOTG210 endpoint setup is not only `endpoint number + MPS`.

For non-control endpoints the driver must coordinate:

1. endpoint direction;
2. endpoint transfer type;
3. endpoint MPS (`INEPMPSR`/`OUTEPMPSR`);
4. endpoint -> FIFO mapping (`EPMAP`);
5. FIFO -> endpoint + direction mapping (`FIFOMAP`);
6. FIFO block size/count/type/enable (`FIFOCF`).

Linux explicitly performs both mapping directions. CherryUSB independently does the same. Treat partial mapping as invalid even if a simple endpoint appears to work on one transfer.

For the first QMK HID milestone, prefer a deterministic static allocator for EP1..EP4 rather than a general dynamic FIFO allocator. Generalization can follow after enumeration is stable.

## ChibiOS LLD mapping

### `usb_lld_init()`

ChibiOS responsibility:

- initialize `USBDriver` software state (`usbObjectInit` pattern);
- initialize private FOTG210 software bookkeeping;
- do **not** touch clocks, PHY parameters or attach state.

Recommended private state:

```text
fifo_owner[4]
endpoint direction/type/MPS cache
transfer active flags
transfer requested/actual lengths
optional VDMA bookkeeping
```

Do not encode HFD8KCZ700 board state here.

### `usb_lld_start(usbp)`

Recommended sequence:

```text
1. SN34F28x platform hook: enable controller/PHY clocks and perform verified reset/PHY sequence.
2. Keep device logically unplugged while configuring the controller.
3. Mask global device interrupt.
4. Enable FOTG210 device core / request software reset.
5. Do not force Full Speed; allow HS negotiation.
6. Wait for FOTG210 software reset completion.
7. Clear device address/AFT_CONF state.
8. Reset/unmap all non-control FIFOs.
9. Clear stale interrupt status before unmasking sources.
10. Configure reset/suspend/resume, control and required transfer-completion masks.
11. Enable VDMA only if the first implementation actually uses it.
12. Enable SN34 IRQ 77 at the NVIC.
13. Release logical unplug only after the controller state is coherent.
14. Enable FOTG210 global device IRQ last.
```

Important donor facts:

- CherryUSB asserts `UNPLUG` during controller setup and releases it near the end.
- CherryUSB masks global IRQ before setup, clears pending source groups, then enables global device IRQ last.
- CherryUSB clears `FORCE_FS` for HS operation. For SN34, verify equivalent F28 field semantics before writing it.
- SONiX clock/reset/`PHYPRM0..4` setup is outside the generic FOTG210 core.

### `usb_lld_stop(usbp)`

Recommended sequence:

```text
1. mask FOTG210 global/device interrupts;
2. assert logical unplug;
3. abort/quiesce any active VDMA;
4. reset/disable non-control FIFOs;
5. disable NVIC IRQ 77;
6. invoke SN34F28x platform shutdown/gating hook only after the core is quiet.
```

Do not copy Bouffalo PDS power-down code.

### bus reset / `usb_lld_reset(usbp)`

On `USBRST`:

- acknowledge reset status using F28-defined W1C/read-modify semantics;
- reset all FIFO/control-FIFO state;
- clear active transfer bookkeeping;
- reset software USB address to zero and clear DAR address bits;
- clear `AFT_CONF` until configured;
- install/reinitialize EP0 ChibiOS endpoint config;
- keep non-control endpoints disabled until ChibiOS configures them;
- refresh speed/timing-dependent controller state if F28 requires it after reset;
- signal ChibiOS `_usb_reset(usbp)` from ISR context.

CherryUSB additionally reapplies its SOF timing mask after reset. Treat the exact F28 timing value as a controller-specific item to verify rather than hard-code from Bouffalo.

### `usb_lld_set_address(usbp)`

Map to `DAR[6:0]`.

Implementation rule: **replace** address bits, do not OR a new address into an old value.

Keep `AFT_CONF` separate from address programming. Linux models them as separate operations; CherryUSB also preserves the bit when updating address.

The ChibiOS core determines the correct control-transfer phase for calling `usb_lld_set_address()`; the LLD should not invent an additional delayed-address state machine unless testing proves it is required.

### `usb_lld_init_endpoint(usbp, ep)`

#### EP0

EP0 uses the dedicated control FIFO (`CXF`), not one of the four general data FIFOs.

Required state:

- 64-byte control MPS for the initial USB2 enumeration path unless descriptors/core explicitly require otherwise;
- control SETUP/IN/OUT event handling;
- dedicated 8-byte setup buffer in ChibiOS endpoint config;
- `CX_STL`, `CX_CLR`, `CX_DONE` semantics via the control configuration register.

#### EP1..EP4

For each active direction:

```text
1. validate MPS against speed/type limits;
2. reserve a FIFO;
3. program IN or OUT MPS register;
4. program endpoint -> FIFO map;
5. program FIFO -> endpoint + direction map;
6. program transfer type/block size/count;
7. enable FIFO;
8. leave transfer interrupt masked until a transfer is armed, if using Linux-style demand masking.
```

The first implementation should cap `USB_MAX_ENDPOINTS` to the verified hardware/controller subset (EP0..EP4 is the common donor baseline) until the F28 manual/header proves more are usable.

### `usb_lld_disable_endpoints(usbp)`

For EP1+:

- mask FIFO interrupts;
- abort active transfer/VDMA;
- reset FIFO;
- clear `EPMAP` ownership;
- clear `FIFOMAP` ownership/direction;
- disable FIFO in `FIFOCF`;
- clear software ownership/state.

Leave EP0 alive.

### `usb_lld_get_status_in/out()`

Return:

- `EP_STATUS_DISABLED` if endpoint has no active mapping/FIFO allocation;
- `EP_STATUS_STALLED` if direction-specific MPS register has the STALL bit;
- otherwise `EP_STATUS_ACTIVE`.

Do not infer endpoint enabled solely from a nonzero MPS.

### `usb_lld_read_setup()`

The control SETUP packet is exactly 8 bytes.

Two donor-supported approaches exist:

- read the control FIFO data port after selecting CXF (Linux);
- use F28 VDMA from CXF into the 8-byte setup buffer (CherryUSB-style VDMA extension).

For first bring-up, choose the simpler path supported cleanly by F28 documentation. PIO for SETUP is attractive because it removes VDMA from the minimum enumeration dependency chain.

After acquiring the packet, invoke the ChibiOS setup callback path and preserve ChibiOS' dedicated `setup_buf` contract.

### `usb_lld_start_in()` / transmit

ChibiOS supplies `txbuf`, `txsize`, `txcnt`, `txlast` and endpoint MPS through endpoint state/config.

First implementation policy:

- arm at most one packet or one bounded VDMA transaction at a time;
- for EP0, use CXF and assert `CX_DONE` at the correct end of the control stage;
- for EP1+, target the mapped FIFO;
- on completion, update ChibiOS transfer counters and either arm the next packet or invoke `_usb_isr_invoke_in_cb()`;
- handle zero-length IN with `TX0BYTE`/equivalent controller mechanism rather than pretending it is a normal nonzero DMA;
- preserve the QMK-level `in-flight + latest-pending` policy above this driver; do not add a historical software FIFO inside the USB LLD.

### `usb_lld_start_out()` / receive

First implementation policy:

- arm the mapped FIFO/CXF for the currently requested ChibiOS buffer;
- on completion, derive actual length from VDMA remaining count or FIFO byte count, whichever F28 mode is used;
- update `rxcnt/rxsize/rxpkts` consistently with ChibiOS semantics;
- complete on requested packet count or short packet;
- handle OUT ZLP via `RX0BYTE`/equivalent event;
- invoke `_usb_isr_invoke_out_cb()` only after state counters are coherent.

### `usb_lld_stall_in/out()`

EP0:

- use `DCFESR.CX_STL`.

EP1+:

- set `STL_EP` in direction-specific `INEPMPSR(n)` or `OUTEPMPSR(n)`.

Linux additionally waits for IN FIFO empty before asserting IN stall. Preserve this as a verification item for F28 rather than blindly omitting it.

### `usb_lld_clear_in/out()`

For EP1+:

1. clear `STL_EP`;
2. perform the controller's data-toggle reset sequence.

Linux explicitly sets **and then clears** `RESET_TSEQ`; it notes the hardware does not self-clear that bit. This is a high-value implementation detail to retain unless F28 documentation proves different behavior.

EP0 clear/stall semantics are separate and must use the CX control state machine.

## ISR translation

### Top-level

IRQ 77 should:

1. read global/device group status;
2. service only enabled/unmasked groups;
3. acknowledge each source according to F28 status semantics;
4. translate events into ChibiOS callbacks/state transitions;
5. never busy-loop indefinitely inside the ISR waiting for ordinary endpoint traffic.

IRQ 76 remains a separate SONiX wake interrupt and must not be conflated with the normal FOTG210 device IRQ until its exact F28 behavior is verified.

### Minimum event map

| FOTG210 event | ChibiOS action |
|---|---|
| USB reset | reset FIFOs/address/endpoint software state, `_usb_reset(usbp)` |
| suspend | `_usb_suspend(usbp)` |
| resume/wakeup | `_usb_wakeup(usbp)` |
| control SETUP | fetch 8 bytes, `_usb_isr_invoke_setup_cb(usbp, 0)` |
| EP0 IN complete | update tx state, `_usb_isr_invoke_in_cb(usbp, 0)` when transaction stage completes |
| EP0 OUT complete | update rx state, `_usb_isr_invoke_out_cb(usbp, 0)` when appropriate |
| FIFO IN/VDMA complete | advance corresponding IN endpoint and callback on completion |
| FIFO OUT/VDMA complete | advance corresponding OUT endpoint and callback on short/full completion |
| TX0BYTE | finish zero-length IN for mapped endpoint |
| RX0BYTE | finish zero-length OUT for mapped endpoint |
| DMA/VDMA error | abort transfer, reset affected FIFO, record diagnostic state; exact ChibiOS error path must be designed |
| control abort/fail | clear/flush CXF and return EP0 to a known state; callback/error behavior to verify |

### Stale pending events before enable

Before enabling IRQ 77/global device interrupts:

- snapshot status registers for diagnostics;
- clear all safely-clearable stale reset/suspend/resume/FIFO/VDMA statuses;
- then unmask selected sources;
- then enable the NVIC/global interrupt.

This is both conventional controller hygiene and specifically valuable for keyboard firmware: another HS keyboard stack has observed startup suspend state wedging enumeration when pending events were allowed to leak into the active state machine.

## HS speed detection

Do not use a writable `HS_EN`/`FORCE_FS`-style control bit as proof that enumeration is High Speed.

The runtime speed must come from the F28 controller's negotiated-speed status (the CherryUSB donor uses the OTG status speed field). Expose this in bring-up diagnostics and assert expected HS after reset/enumeration.

## VDMA strategy

F28 has a VDMA-capable extension and CherryUSB demonstrates per-CXF/per-FIFO VDMA operations. However the minimum bring-up should separate these questions:

1. can EP0 enumerate correctly using the simplest documented access path?
2. can one interrupt IN endpoint transfer reliably?
3. only then, does VDMA improve the non-control path and reduce CPU cost enough to justify enabling it?

This avoids debugging PHY + controller + endpoint mapping + VDMA simultaneously.

For the final 8 kHz target, VDMA is likely valuable, but it is not a prerequisite for the first descriptor exchange.

## First hardware trace points

Record at least:

```text
platform USB clocks enabled
platform USB reset released
PHY configuration complete
FOTG software reset begin/end
logical attach (UNPLUG cleared/equivalent)
USBRST ISR
negotiated speed
SETUP packet bytes
SET_ADDRESS value
SET_CONFIGURATION value/AFT_CONF transition
EP1 FIFO mapping
IN arm timestamp
IN completion timestamp
suspend/resume
unexpected DMA/VDMA/control errors
```

Timestamps should eventually use a free-running high-resolution timer, not USB SOF, so SOF timing cannot hide USB scheduling latency.

## Source provenance

- ChibiOS contract/reference: `SHVD3x/ChibiOS-Contrib` accepted baseline, existing `SN32/LLD/SN32F2xx/USB` implementation. Structural/API reference only for SN34.
- Permissive controller donor: `cherry-embedded/CherryUSB`, `port/bouffalolab/usb_dc_bl.c`, Apache-2.0.
- Independent GPL controller donor: Linux `drivers/usb/fotg210/fotg210-udc.c` and `fotg210-udc.h`.
- Hardware authority: SN34F28x official manual and recovered official DFP/header/SVD when available.

## Remaining blockers before code

- authoritative F28 register symbols/bitfields in-tree;
- exact SONiX PHY `PHYPRM0..4` values/order/delays;
- exact F28 VDMA extension semantics and IRQ bits reconciled against the official header/manual;
- exact controller speed-status field on F28;
- confirm F28 endpoint/FIFO count and MPS constraints from official material;
- decide initial PIO-vs-VDMA EP0 implementation after those definitions are recovered.
