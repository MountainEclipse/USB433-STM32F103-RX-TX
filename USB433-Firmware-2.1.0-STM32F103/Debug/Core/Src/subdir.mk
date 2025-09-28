################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Core/Src/commands.cpp \
../Core/Src/core_main.cpp \
../Core/Src/main.cpp \
../Core/Src/more_math.cpp \
../Core/Src/receiver.cpp \
../Core/Src/transmitter.cpp 

OBJS += \
./Core/Src/commands.o \
./Core/Src/core_main.o \
./Core/Src/main.o \
./Core/Src/more_math.o \
./Core/Src/receiver.o \
./Core/Src/transmitter.o 

CPP_DEPS += \
./Core/Src/commands.d \
./Core/Src/core_main.d \
./Core/Src/main.d \
./Core/Src/more_math.d \
./Core/Src/receiver.d \
./Core/Src/transmitter.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.cpp Core/Src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m3 -std=gnu++14 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I../Core/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -I"Z:/Documents/STM32/433MHz-Dongle/Core/Inc/sys" -I"Z:/Documents/STM32/433MHz-Dongle/Core/Src/sys" -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/commands.cyclo ./Core/Src/commands.d ./Core/Src/commands.o ./Core/Src/commands.su ./Core/Src/core_main.cyclo ./Core/Src/core_main.d ./Core/Src/core_main.o ./Core/Src/core_main.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/more_math.cyclo ./Core/Src/more_math.d ./Core/Src/more_math.o ./Core/Src/more_math.su ./Core/Src/receiver.cyclo ./Core/Src/receiver.d ./Core/Src/receiver.o ./Core/Src/receiver.su ./Core/Src/transmitter.cyclo ./Core/Src/transmitter.d ./Core/Src/transmitter.o ./Core/Src/transmitter.su

.PHONY: clean-Core-2f-Src

