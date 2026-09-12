# SN34F28x startup / CMSIS recovery checkpoint

Status: recovered vendor-derived evidence; no executable startup has been added to the port yet.

Source repository: `lxnick/SN34F-Application`, project generated from SONiX Platform SDK `SN34F280/V2.1.3` and Keil pack `SONiX.SN34F2_DFP.2.0.5`.

Recovered RTE files:

```text
printf_uart/build/RTE/Device/SN34F289F/startup_SN34F280.s
printf_uart/build/RTE/Device/SN34F289F/system_SN34F280.c
```

The startup file identifies itself as `startup_SN34F280.s` V2.0.1, dated January 2024. The uVision project records this as the `Device / Startup` component from `SONiX.SN34F2_DFP 2.0.5`.

## Reset preamble

Before `SystemInit()` and C runtime entry, the recovered vendor startup performs:

```text
R0 = 0x4001F004
R1 = *R0
R1 &= 0xFFFEFFFF        // clear bit 16
*R0 = R1

R0 = 0x4001F000
R1 = 0x00010000
*R0 = R1

SystemInit()
__main
```

The official F28 public memory map does not give this reserved block a normal peripheral identity. The same pattern has been observed in sibling SN34 vendor startup, so treat it as common SN34 boot/shadow/system machinery rather than Hall or USB logic.

Bring-up rule: preserve this sequence in the first native startup until its exact purpose is proven. Do not optimize it away merely because the block is undocumented in the public peripheral map.

## Boot/user marker at end of user flash

The vendor startup includes a section at:

```text
0x0007DFFC: 0xAAAA5555
```

with the comment:

```text
ISP_MODE_FLAG = 0xAAAA5555 for USER MODE
```

The `lxnick` application intentionally comments this section out to avoid padding a partial application image to the full 504 KiB user region. Therefore:

- the marker is real vendor startup behavior;
- its exact bootloader/security semantics still require controlled hardware/bootloader validation;
- a full replacement image and a partial-update/application image may have different requirements;
- do not blindly emit or omit it in the eventual ChibiOS linker/startup until the flashing/recovery model is chosen.

## Recovered interrupt topology

The vendor DFP startup gives the following named external IRQs. Unlisted entries in the table are reserved in that startup version.

| IRQ | Handler |
|---:|---|
| 0 | `WWDT_IRQHandler` |
| 1 | `LVD_IRQHandler` |
| 2 | `WDT_IRQHandler` |
| 3 | `SCU_IRQHandler` |
| 4 | `FLASH_IRQHandler` |
| 11 | `DMA0_CH0_IRQHandler` |
| 12 | `DMA0_CH1_IRQHandler` |
| 13 | `DMA0_CH2_IRQHandler` |
| 14 | `DMA0_CH3_IRQHandler` |
| 15 | `DMA0_CH4_IRQHandler` |
| 16 | `DMA0_CH5_IRQHandler` |
| 17 | `DMA0_CH6_IRQHandler` |
| 18 | `ADC0_IRQHandler` |
| 19 | `CAN0_TX_IRQHandler` |
| 20 | `CAN0_RX_IRQHandler` |
| 21 | `CAN0_TT_IRQHandler` |
| 22 | `CAN0_EW_IRQHandler` |
| 24 | `CT16B0_IRQHandler` |
| 25 | `CT16B8_IRQHandler` |
| 27 | `CT16B3_IRQHandler` |
| 28 | `CT16B2_IRQHandler` |
| 29 | `CT16B5_IRQHandler` |
| 31 | `I2C0_IRQHandler` |
| 33 | `I2C1_IRQHandler` |
| 35 | `SPI0_IRQHandler` |
| 36 | `SPI1_IRQHandler` |
| 37 | `UART0_IRQHandler` |
| 38 | `UART1_IRQHandler` |
| 39 | `UART2_IRQHandler` |
| 43 | `CT16B4_IRQHandler` |
| 45 | `CT16B1_IRQHandler` |
| 47 | `DMA0_CH7_IRQHandler` |
| 49 | `SDIO_IRQHandler` |
| 51 | `SPI2_IRQHandler` |
| 52 | `UART3_IRQHandler` |
| 53 | `UART4_IRQHandler` |
| 54 | `CT16B6_IRQHandler` |
| 55 | `CT16B7_IRQHandler` |
| 56 | `DMA1_CH0_IRQHandler` |
| 57 | `DMA1_CH1_IRQHandler` |
| 58 | `DMA1_CH2_IRQHandler` |
| 59 | `DMA1_CH3_IRQHandler` |
| 60 | `DMA1_CH4_IRQHandler` |
| 61 | `ETH_IRQHandler` |
| 63 | `CAN1_TX_IRQHandler` |
| 64 | `CAN1_RX_IRQHandler` |
| 65 | `CAN1_TT_IRQHandler` |
| 66 | `CAN1_EW_IRQHandler` |
| 68 | `DMA1_CH5_IRQHandler` |
| 69 | `DMA1_CH6_IRQHandler` |
| 70 | `DMA1_CH7_IRQHandler` |
| 71 | `UART5_IRQHandler` |
| 72 | `I2C2_IRQHandler` |
| 76 | `USB_HS_WKUP_IRQHandler` |
| 77 | `USB_HS_IRQHandler` |
| 81 | `FPU_IRQHandler` |
| 90 | `LCM_IRQHandler` |
| 91 | `ETH_WOL_IRQHandler` |
| 92 | `GPIO3_IRQHandler` |
| 93 | `GPIO2_IRQHandler` |
| 94 | `GPIO1_IRQHandler` |
| 95 | `GPIO0_IRQHandler` |

This table is suitable for seeding the ChibiOS ISR registry, but only names/indices required by implemented LLDs should be exposed initially.

## Recovered `SystemInit()` behavior

The DFP `system_SN34F280.c` `SystemInit()` is intentionally small:

- enables CP10/CP11 full access when FPU is present/used;
- optionally relocates VTOR when user vector-table relocation is configured;
- does **not** configure the application PLL/clock tree itself.

Therefore the native ChibiOS `hal_lld` still needs an explicit F28 clock/FCS implementation. We should not expect CMSIS `SystemInit()` to provide the runtime clock policy for us.

## Recovered `SystemCoreClockUpdate()` model

Constants in the recovered file:

```text
IHRC = 12,000,000 Hz
ILRC = 32,000 Hz
ELS  = 32,768 Hz
EHS  = build-time configured, example 16 MHz
```

Clock-source interpretation used by the vendor CMSIS file:

```text
PLLSTS.SYSCLKSTS = 0 -> IHRC
PLLSTS.SYSCLKSTS = 2 -> EHS
PLLSTS.SYSCLKSTS = 4 -> PLL
```

For PLL:

```text
PLLSTS.PLLCLKSTS == 0 -> IHRC input
otherwise             -> EHS input
PLL frequency numerator uses PLLSTS.NSSTS
```

`FSSTS` divisor mapping:

```text
0 -> /32
1 -> /16
2 -> /8
3 -> /4
```

Then `CLKPRE.AHBPRE` divides the selected system clock by:

```text
0 -> /1
1 -> /2
2 -> /4
3 -> /8
4 -> /16
5 -> /32
6 -> /64
7 -> /128
```

This is strong vendor evidence for interpreting the official F28 SCU clock fields. The actual clock-transition sequence still comes from the official F28 FCS rules plus the F78 behavioral donor, not from this passive clock-reporting function.

## Compiler/startup implications for ChibiOS

The recovered project confirms:

- Cortex-M4 target;
- hardware FPU is expected;
- standard Cortex-M exception table including MemManage/BusFault/UsageFault;
- FPU interrupt exists at external IRQ 81;
- vector table extends through IRQ95;
- CMSIS startup stack/heap defaults are irrelevant to ChibiOS linker design and should not be copied as policy.

The ChibiOS startup should be written in native ChibiOS style while preserving the verified reset preamble and IRQ numbering.

## Remaining startup/CMSIS blockers

Before replacing the fail-closed `hal_lld.c` and adding executable startup/linker support:

1. obtain `SN34F280.h` or clean-room equivalent register definitions;
2. recover/verify the DFP SVD if possible;
3. decide linker representation of the three physical SRAM regions;
4. verify how Boot ROM/shadow remap interacts with the `0x4001F000` preamble and debugger `Dbg_User.ini`;
5. verify full-image vs partial-image behavior of the `0x7DFFC` USER MODE marker;
6. implement F28 FCS/flash-frequency-safe clock setup independently of CMSIS `SystemInit()`.
