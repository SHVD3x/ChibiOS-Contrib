# SN34F28x research scaffold.
#
# This file intentionally wires only the platform shell. Driver LLDs are added
# after their register/IRQ contracts are verified. See PORTING.md.

PLATFORMSRC_CONTRIB += $(CHIBIOS)/os/hal/ports/common/ARMCMx/nvic.c \
                       $(CHIBIOS_CONTRIB)/os/hal/ports/SN34/SN34F28x/hal_lld.c

PLATFORMINC_CONTRIB += $(CHIBIOS)/os/hal/ports/common/ARMCMx \
                       $(CHIBIOS_CONTRIB)/os/hal/ports/SN34/SN34F28x

# Planned LLD composition, intentionally disabled until each contract is
# implemented and verified:
# include $(CHIBIOS_CONTRIB)/os/hal/ports/SN34/LLD/GPIOv1/driver.mk
# include $(CHIBIOS_CONTRIB)/os/hal/ports/SN34/LLD/DMAv1/driver.mk
# include $(CHIBIOS_CONTRIB)/os/hal/ports/SN34/LLD/SYSTICKv1/driver.mk
# include $(CHIBIOS_CONTRIB)/os/hal/ports/SN34/LLD/TMRv1/driver.mk
# include $(CHIBIOS_CONTRIB)/os/hal/ports/SN34/LLD/ADCv1/driver.mk
# include $(CHIBIOS_CONTRIB)/os/hal/ports/SN34/LLD/FOTG210v1/driver.mk

ALLCSRC += $(PLATFORMSRC_CONTRIB)
ALLINC  += $(PLATFORMINC_CONTRIB)
