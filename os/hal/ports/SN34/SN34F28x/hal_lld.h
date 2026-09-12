/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file    hal_lld.h
 * @brief   SN34F28x HAL low level driver scaffold.
 */

#ifndef HAL_LLD_H
#define HAL_LLD_H

#include "sn34f28x_registry.h"

#define PLATFORM_NAME "SONiX SN34F28x"

/*
 * Clock-point reporting remains unavailable until the F28 clock tree is
 * implemented from authoritative register definitions.
 */
#define hal_lld_get_clock_point(clkpt) 0U

#if !defined(PLATFORM_MCUCONF)
#error "SN34F28x: PLATFORM_MCUCONF is not defined"
#endif

#ifdef __cplusplus
extern "C" {
#endif
void hal_lld_init(void);
void sn34f28x_clock_init(void);
#ifdef __cplusplus
}
#endif

#endif /* HAL_LLD_H */
