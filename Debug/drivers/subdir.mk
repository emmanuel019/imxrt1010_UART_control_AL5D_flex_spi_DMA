################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../drivers/fsl_cache.c \
../drivers/fsl_clock.c \
../drivers/fsl_common.c \
../drivers/fsl_dmamux.c \
../drivers/fsl_edma.c \
../drivers/fsl_flexspi.c \
../drivers/fsl_flexspi_edma.c \
../drivers/fsl_gpio.c \
../drivers/fsl_lpuart.c 

OBJS += \
./drivers/fsl_cache.o \
./drivers/fsl_clock.o \
./drivers/fsl_common.o \
./drivers/fsl_dmamux.o \
./drivers/fsl_edma.o \
./drivers/fsl_flexspi.o \
./drivers/fsl_flexspi_edma.o \
./drivers/fsl_gpio.o \
./drivers/fsl_lpuart.o 

C_DEPS += \
./drivers/fsl_cache.d \
./drivers/fsl_clock.d \
./drivers/fsl_common.d \
./drivers/fsl_dmamux.d \
./drivers/fsl_edma.d \
./drivers/fsl_flexspi.d \
./drivers/fsl_flexspi_edma.d \
./drivers/fsl_gpio.d \
./drivers/fsl_lpuart.d 


# Each subdirectory must supply rules for building sources it contributes
drivers/%.o: ../drivers/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -std=gnu99 -D__REDLIB__ -DCPU_MIMXRT1011DAE5A -DCPU_MIMXRT1011DAE5A_cm7 -DENABLE_RAM_VECTOR_TABLE -DSDK_DEBUGCONSOLE=1 -DXIP_EXTERNAL_FLASH=1 -DXIP_BOOT_HEADER_ENABLE=1 -DSERIAL_PORT_TYPE_UART=1 -DCR_INTEGER_PRINTF -DPRINTF_FLOAT_ENABLE=0 -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\board" -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\source" -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\drivers" -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\device" -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\utilities" -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\component\uart" -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\component\serial_manager" -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\component\lists" -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\xip" -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\CMSIS" -O0 -fno-common -g3 -c -ffunction-sections -fdata-sections -ffreestanding -fno-builtin -fmerge-constants -fmacro-prefix-map="../$(@D)/"=. -mcpu=cortex-m7 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


