# SN34F28x ChibiOS port scaffold

Status: **research scaffold; intentionally not buildable yet**.

This directory is the landing zone for a native ChibiOS-Contrib port for the SONiX SN34F28x family, with SN34F288 as the first QMK target.

## Why this exists now

The generic SN34F28x platform is understood well enough to freeze the port boundaries before the remaining vendor artifacts are recovered. The code must not guess register definitions, startup details, USB PHY values, flash policy, or board wiring.

The branch containing this scaffold is based directly on accepted upstream ChibiOS-Contrib baseline `5a9ad82b6ba4f649cdb800e8c2bbc9be2d3ce767`, not on the older private `sn32_develop` branch.

## Port boundary

```text
QMK / keyboard code
        |
ChibiOS generic HAL
        |
os/hal/ports/SN34/SN34F28x
        |-- hal_lld / clocks / FCS
        |-- registry / IRQ topology
        |
os/hal/ports/SN34/LLD
        |-- GPIO/PAL
        |-- SYSTICK/TMR
        |-- DMA
        |-- ADC
        |-- FOTG210v1 USB device LLD
        `-- additional peripherals only when required
        |
SONiX SN34F28x CMSIS/register definitions
```

The HFD8KCZ700 Hall-effect scan/mux/calibration pipeline is board code and must not be placed in the generic MCU port.

## Verified platform facts used to shape the scaffold

- CPU: ARM Cortex-M4F / ARMv7-M with single-precision FPU.
- Maximum core/AHB clock: 192 MHz.
- User flash: 504 KiB of a 512 KiB total flash space; an 8 KiB Boot ROM is documented separately.
- SRAM is segmented: 32 KiB at `0x20000000`, 96 KiB at `0x20008000`, and 32 KiB at `0x20020000`.
- USB-HS controller base: `0x40058000`.
- USB-HS PHY base: `0x4001D000`.
- USB wake IRQ: 76; USB-HS IRQ: 77.
- F28 generic peripheral map is very close to SN34F78x, but F78 is not hardware evidence for the F28 USB/PHY block.
- F28 clock switching uses the SONiX FCS mechanism and must obey flash-frequency and keyed-prescaler constraints.

## Explicit blockers before first build

1. Recover or clean-room reproduce the authoritative SN34F280 CMSIS device header / SVD from `SONiX.SN34F2_DFP.2.0.5` or a newer official pack.
2. Recover/verify startup and vector naming, including the common SN34 pre-`SystemInit()` writes around `0x4001F000`.
3. Define a linker layout that preserves all three SRAM banks; do not flatten the memory map without evidence.
4. Implement the F28 clock/FCS sequence from official behavior, including flash operating-frequency ordering and `SCU_CLKPRE` keying.
5. Populate the registry only from verified instances, IRQs, and capabilities.
6. Implement GPIO/PAL and the system timer before attempting USB.
7. Recover the SONiX USB PHY `PHYPRM0..4` values/order/delays from authoritative or stock-firmware evidence.
8. Implement the FOTG210-class USB device core independently of the SONiX-specific PHY hook.
9. Verify flash geometry/security/ISP behavior before enabling EFL/wear-leveling or self-programming.

## USB design rule

The generic FOTG210 device LLD must not contain guessed SONiX PHY magic. Platform-specific USB clock/reset/PHY preparation belongs behind a small SN34F28x hook. This keeps the FOTG210 core testable against CherryUSB/Linux behavior and localizes the remaining R2 uncertainty.

## Source/reuse policy

- Existing SN32 ChibiOS-Contrib: structural/integration reference, not a register donor.
- SN34F78x public vendor-derived LL/HAL: behavioral evidence for generic SN34 peripherals; do not copy unless licensing is independently established.
- Holy80 AT32 ChibiOS-Contrib: structural reference for modular high-speed-capable LLD organization.
- CherryUSB FOTG210: permissive device-core/register-flow donor; Bouffalo-specific PHY/PDS code is not applicable to SONiX.
- Linux FOTG210 UDC/core: GPL-compatible behavioral/reference donor.
- Official SN34F28x documentation and recovered DFP/stock firmware remain authoritative for SONiX-specific behavior.

## First executable milestone

The first real bring-up target is deliberately smaller than QMK:

1. verified startup/vector entry;
2. safe clock setup;
3. GPIO toggle;
4. stable system timer;
5. SWD/debug remains recoverable.

Only after that should the native USB-HS LLD be enabled.
