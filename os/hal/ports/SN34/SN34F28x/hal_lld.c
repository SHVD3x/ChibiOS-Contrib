/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    hal_lld.c
 * @brief   SN34F28x HAL low level driver scaffold.
 *
 * This translation unit intentionally refuses to build until the authoritative
 * SN34F28x CMSIS/register boundary and clock/FCS sequence are in-tree. An
 * empty hal_lld_init() would be unsafe because it could make an unverified
 * clock/startup configuration look usable on hardware.
 */

#error "SN34F28x port scaffold only: recover/verify CMSIS + startup + clock/FCS before enabling hal_lld.c"

#include "hal.h"

void hal_lld_init(void) {
}

void sn34f28x_clock_init(void) {
}
