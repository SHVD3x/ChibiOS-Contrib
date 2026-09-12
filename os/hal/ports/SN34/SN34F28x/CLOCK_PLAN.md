# SN34F28x clock / FCS bring-up contract

Status: design frozen from official F28 behavior + vendor F28 sample + sibling F78 behavioral donor. No executable clock code is enabled yet because authoritative F28 register definitions are still missing from-tree.

## First target clock profile

The recovered SONiX SN34F280 V2.1.3 vendor sample configures the high-performance profile as:

```text
PLL input: IHRC = 12 MHz
PLL NS:    64
PLL FS:    /4
PLL out:   12 MHz * 64 / 4 = 192 MHz
SYSCLK:    PLL
AHB/HCLK:  /1 = 192 MHz
APB0:      /4 = 48 MHz
APB1:      /2 = 96 MHz
```

This exactly matches the official F28 maximum bus limits:

```text
AHB  <= 192 MHz
APB0 <=  48 MHz
APB1 <=  96 MHz
```

Use this profile for initial bring-up unless the board crystal/stock firmware gives a specific reason to start elsewhere. The use of IHRC as PLL source avoids making an external crystal a prerequisite for first SWD/GPIO bring-up.

## Key safety invariant

Before increasing HCLK:

> the Flash operating-frequency configuration must already support the **new** HCLK.

When decreasing HCLK, Flash timing/capability may be reduced only **after** the successful frequency-change sequence.

The F28 vendor sample explicitly programs Flash capability to 192 MHz before retuning an active PLL. The F78 LL independently implements the same ordering generically.

## FCS is mandatory

Official F28 documentation requires clock/PLL changes to pass through the Frequency Change Sequence (FCS):

```text
configure requested PLL / SYSCLK / prescalers
-> set FCS interrupt path
-> set FCS
-> CPU executes WFI / enters idle
-> hardware gates system clock and performs frequency-change sequence
-> SCU FCS interrupt wakes CPU
-> clear FCS status
-> verify PLL/status registers
```

Do not replace this with a simple register write + delay loop.

## Recovered sibling FCS algorithm

SN34F78x LL uses a conservative sequence that matches the F28 manual and is a strong behavioral donor:

```text
backup NVIC enable state
mask peripheral IRQs
backup SysTick CTRL
mask SysTick interrupt
backup SCU interrupt enable state
mask SCU interrupts
clear stale INT_FCS
unmask SCU FCS interrupt
NVIC enable SCU IRQ
enable EFCSTB/FCS wake path
set PWRMODE.FCS
WFI
on wake: disable EFCSTB
restore NVIC/SysTick/SCU interrupt enables
```

For SN34F28x, reproduce the behavior independently against F28 symbols. Do not copy vendor LL source text without a licensing basis.

### Why the interrupt masking matters

FCS relies on WFI/idle and a specific SCU wake event. An unrelated enabled peripheral/SysTick interrupt must not prematurely wake the CPU and make the clock transition appear complete.

The ChibiOS implementation therefore needs a small critical FCS primitive rather than using ordinary sleep while the full interrupt system is live.

## PLL programming sequence

Initial 192 MHz setup:

```text
1. Ensure IHRC enabled and ready.
2. Ensure Flash operating-frequency config >= 192 MHz before any HCLK increase.
3. Set PLL source = IHRC.
4. Set NS = 64.
5. Set FS = /4.
6. Enable PLL.
7. Clear/disable FCS_PLLRSTOFF so FCS resets/relocks PLL when changing active PLL parameters.
8. Execute FCS primitive.
9. Wait/check PLLSTABLE with a bounded timeout.
10. Verify PLL status fields reflect source/NS/FS.
```

The F78 donor explicitly sets `FCS_PLLRSTOFF = 0` when reconfiguring PLL so the PLL is reset for lock during FCS; this behavior agrees with the F28 manual note.

## SYSCLK and bus prescaler sequence

Once the PLL is stable:

```text
1. Set FCS_PLLRSTOFF = 1 to keep stable PLL active during bus/source FCS.
2. Request SYSCLK = PLL.
3. Request AHBPRE  = /1.
4. Request APB0PRE = /4.
5. Request APB1PRE = /2.
6. Execute FCS primitive.
7. Verify SYSCLK status == PLL.
8. Verify effective HCLK/APB0/APB1 limits.
9. Set SystemCoreClock = 192 MHz from readback, not merely requested values.
10. Reconfigure ChibiOS system tick/time base for the resulting clocks.
```

## `SCU_CLKPRE` write protection

Official English F28 v1.7 register documentation states that AHB/APB/CLKOUT prescaler writes require:

```text
CLKPRE.WRPKEY = 0x5AFA
```

The sibling F78 LL independently writes `0x5AFA` before changing HCLK/APB prescalers.

One Chinese F28 description contains `0x5A5A` in a prose step, while another section and the current English register table use `0x5AFA`. Treat `0x5A5A` as a documentation typo unless silicon evidence proves otherwise.

Implementation rule: a prescaler helper should emit the key and the new prescaler value in the register transaction required by the F28 register definition. Do not rely on a prior key write remaining effective.

## Status/readback model recovered from F28 CMSIS

`SystemCoreClockUpdate()` from DFP 2.0.5 interprets:

```text
PLLSTS.SYSCLKSTS:
  0 -> IHRC
  2 -> EHS
  4 -> PLL

PLLSTS.PLLCLKSTS:
  0 -> IHRC input
  other -> EHS input

PLLSTS.FSSTS:
  0 -> /32
  1 -> /16
  2 -> /8
  3 -> /4

CLKPRE.AHBPRE:
  0..7 -> /1,/2,/4,/8,/16,/32,/64,/128
```

Use hardware status fields for post-transition validation rather than trusting control-register writes alone.

## Timeout/error policy

Every wait must be bounded:

- oscillator ready;
- PLL stable;
- SYSCLK status transition;
- FCS completion/wakeup.

The first hardware port should fail into an observable low-risk state rather than loop forever at an unknown frequency. During debug builds, preserve a compact error code in retained/global state and stop with SWD usable.

Do not automatically fall back to another high-performance clock profile until basic bring-up is stable; that can hide a real SCU/Flash configuration error.

## F78 donor defect to avoid

The inspected F78 `LL_RCC_ClockConfig()` contains an apparent bug in its APB1 branch: it calls the APB0 prescaler setter for `APB1CLKDivider`.

Therefore:

- use the F78 code as behavioral evidence, not source-of-truth implementation;
- ensure F28 APB0 and APB1 are written independently;
- add a bring-up assertion that effective APB0 = 48 MHz and APB1 = 96 MHz for the 192 MHz profile.

## Proposed ChibiOS implementation split

```text
sn34f28x_clock_init()
  |- sn34f28x_flash_prepare_frequency(target_hclk)
  |- sn34f28x_osc_enable_wait(IHRC)
  |- sn34f28x_pll_configure(IHRC, NS=64, FS=/4)
  |- sn34f28x_fcs()
  |- sn34f28x_pll_verify()
  |- sn34f28x_bus_config(PLL, /1, /4, /2)
  |- sn34f28x_fcs()
  |- sn34f28x_clock_readback_verify()
  `- SystemCoreClock update / timebase reinit
```

Keep the FCS primitive small and testable. Keep Flash frequency programming behind its own function because its implementation/security semantics are still being audited.

## First-clock hardware acceptance gate

Before enabling USB or ADC, prove:

```text
[ ] reset enters native startup reliably
[ ] pre-SystemInit shadow/boot writes preserved
[ ] SWD remains connectable before/after clock switch
[ ] PLL reports IHRC / NS=64 / FS=/4 / stable
[ ] SYSCLK reports PLL
[ ] HCLK = 192 MHz
[ ] APB0 = 48 MHz
[ ] APB1 = 96 MHz
[ ] CLKOUT or timer-based measurement agrees with expected frequency
[ ] GPIO toggle/timer timing is stable over repeated resets
[ ] no Flash read faults during transition
```

Do not proceed to native USB-HS until this gate is repeatable.

## Remaining blockers before code

1. authoritative/clean-room F28 SCU register definitions in-tree;
2. F28 Flash operating-frequency programming implementation and safe policy;
3. exact ChibiOS startup/early interrupt state assumptions around the FCS primitive;
4. hardware verification of the first 192 MHz profile.
