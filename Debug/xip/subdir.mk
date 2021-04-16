################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../xip/evkmimxrt1010_flexspi_nor_config.c \
../xip/fsl_flexspi_nor_boot.c 

OBJS += \
./xip/evkmimxrt1010_flexspi_nor_config.o \
./xip/fsl_flexspi_nor_boot.o 

C_DEPS += \
./xip/evkmimxrt1010_flexspi_nor_config.d \
./xip/fsl_flexspi_nor_boot.d 


# Each subdirectory must supply rules for building sources it contributes
xip/%.o: ../xip/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -std=gnu99 -D__REDLIB__ -DCPU_MIMXRT1011DAE5A -DCPU_MIMXRT1011DAE5A_cm7 -DENABLE_RAM_VECTOR_TABLE -DSDK_DEBUGCONSOLE=1 -DXIP_EXTERNAL_FLASH=1 -DXIP_BOOT_HEADER_ENABLE=1 -DSERIAL_PORT_TYPE_UART=1 -DCR_INTEGER_PRINTF -DPRINTF_FLOAT_ENABLE=0 -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\board" -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\source" -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\drivers" -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\device" -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\utilities" -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\component\uart" -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\component\serial_manager" -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\component\lists" -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\xip" -I"C:\Users\garret\Documents\MCUXpressoIDE_11.3.0_5222\workspace\evkmimxrt1010_flexspi_nor_edma_transfer__basic\CMSIS" -O0 -fno-common -g3 -c -ffunction-sections -fdata-sections -ffreestanding -fno-builtin -fmerge-constants -fmacro-prefix-map="../$(@D)/"=. -mcpu=cortex-m7 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


