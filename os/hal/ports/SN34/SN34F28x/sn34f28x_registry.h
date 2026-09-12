/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    sn34f28x_registry.h
 * @brief   Verified SN34F28x capability/IRQ facts for port bring-up.
 *
 * This is deliberately not a full ChibiOS peripheral registry yet. Add
 * CHIBIOS-style capability macros only after the corresponding LLD contract
 * has been audited against the authoritative F28 register definitions.
 */

#ifndef SN34F28X_REGISTRY_H
#define SN34F28X_REGISTRY_H

/* Verified CPU/platform facts. */
#define SN34F28X_CORE_CLOCK_MAX_HZ     192000000UL
#define SN34F28X_FLASH_USER_SIZE       0x0007E000UL

/* Verified SRAM banks; keep the linker model segmented until proven otherwise. */
#define SN34F28X_SRAM1_BASE            0x20000000UL
#define SN34F28X_SRAM1_SIZE            0x00008000UL
#define SN34F28X_SRAM2_BASE            0x20008000UL
#define SN34F28X_SRAM2_SIZE            0x00018000UL
#define SN34F28X_SRAM3_BASE            0x20020000UL
#define SN34F28X_SRAM3_SIZE            0x00008000UL

/* Verified native USB-HS blocks. */
#define SN34F28X_USB_HS_BASE           0x40058000UL
#define SN34F28X_USB_HS_PHY_BASE       0x4001D000UL
#define SN34F28X_USB_HS_WAKEUP_IRQN    76U
#define SN34F28X_USB_HS_IRQN           77U

#endif /* SN34F28X_REGISTRY_H */
