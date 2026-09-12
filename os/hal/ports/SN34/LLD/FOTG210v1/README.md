# FOTG210v1 USB device LLD contract

Status: design/research only; no driver is enabled by `SN34F28x/platform.mk` yet.

SN34F28x exposes a USB-HS device controller whose device register layout strongly matches the Faraday FOTG210 family. The eventual ChibiOS LLD should isolate generic FOTG210 device-core behavior from SONiX-specific clock/reset/PHY initialization.

## Layer boundary

```text
ChibiOS USBDriver / usb_lld_*
        |
FOTG210v1 device core
        |-- device global control/interrupts
        |-- EP0 control flow
        |-- endpoint <-> FIFO mapping
        |-- max packet size
        |-- IN/OUT FIFO/VDMA operations
        |-- stall/toggle/reset
        |
SN34F28x platform hook
        |-- USBHSCLKEN
        |-- USBPHYCLKEN
        |-- USBPHY12MCLKEN
        |-- USBHSRST
        `-- PHYPRM0..4 sequence
```

The FOTG210 core must never contain Bouffalo-specific PDS/PHY setup copied from CherryUSB.

## ChibiOS operation mapping to recover/implement

| ChibiOS-side operation | FOTG210 responsibility | Current evidence/status |
|---|---|---|
| `usb_lld_init()` | software driver state only; no speculative PHY programming | architecture fixed |
| `usb_lld_start()` | call SN34 USB prepare hook, controller device-mode init, clear stale status, arm IRQs | core known; exact SONiX PHY hook blocked |
| `usb_lld_stop()` | mask IRQs, quiesce endpoints/FIFOs, stop controller; platform may gate clocks | to audit |
| bus reset handler | clear/initialize device state, endpoint/FIFO state and address | FOTG210 donor behavior available |
| set address | update device address at the correct control-transfer phase | register flow to map |
| EP0 SETUP | decode setup event/FIFO, expose 8-byte setup packet to ChibiOS | FOTG210 donor behavior available |
| EP0 IN/OUT | control transfer data/status stages | FOTG210 donor behavior available |
| endpoint init | allocate/map endpoint to FIFO, set direction/type/MPS | FOTG210 donor behavior available |
| prepare/start transmit | program IN FIFO/VDMA and completion event | FOTG210 donor behavior available |
| prepare/start receive | arm OUT FIFO/VDMA and completion event | FOTG210 donor behavior available |
| stall endpoint | set endpoint stall and preserve ChibiOS endpoint state | register flow to map |
| clear stall | clear stall and reset data toggle where required | register flow to map |
| ISR dispatch | translate reset/suspend/resume/EP/FIFO/error events into ChibiOS callbacks | core known; exact bit map to audit |
| wake IRQ 76 | SONiX-specific wake path, separate from primary USB IRQ 77 | verified IRQ topology; behavior unresolved |

## Bring-up rules

1. Clear/inspect stale pending USB status before enabling controller interrupts. Real HE firmware work on another HS MCU family has shown startup suspend events can wedge USB state machines if inherited blindly.
2. Do not force or infer High Speed from a writable mode bit. Verify negotiated speed from controller status after reset/enumeration.
3. Start with EP0 + one interrupt IN endpoint. Do not implement 8 kHz scheduling, vendor HID, large endpoints or Hall telemetry in the first enumeration milestone.
4. Keep endpoint FIFO allocation explicit and observable; FOTG210 uses endpoint/FIFO mapping rather than a simple fixed endpoint RAM model.
5. Add VDMA only after PIO/control transfers are understood enough to separate controller bugs from DMA bugs.
6. Preserve a trace path for reset, setup, address, configuration and endpoint-complete events from the first hardware build.

## Donor roles

- **Official SN34F28x manual/header/SVD** — authoritative controller and PHY addresses, bit fields, IRQs.
- **CherryUSB FOTG210 (Apache-2.0)** — preferred permissive donor for endpoint/FIFO/VDMA/device-flow implementation.
- **Linux FOTG210 UDC/core (GPL)** — independent device-controller behavioral cross-check and GPL-compatible reference.
- **U-Boot FOTG210 definitions** — secondary register naming/offset cross-check.
- **AT32/Holy80 USB LLD** — ChibiOS-facing architectural reference only; its OTG controller implementation is a different hardware block.

## Hard blocker

Do not enable this driver on hardware until the exact SN34F28x `PHYPRM0..4` values, write order and required delays are recovered from an authoritative SDK or stock firmware trace/static analysis.
