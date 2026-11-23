# Optional command line build (arm-none-eabi-gcc in PATH, run setup_drivers first).
# STM32CubeIDE does not need this file.
TARGET  = CWKeyer_F746
BUILD   = build
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

MCU     = -mcpu=cortex-m7 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
DEFS    = -DUSE_HAL_DRIVER -DSTM32F746xx
INCS    = -ICore/Inc -IApp/Inc -IGui/Inc -ILib/Inc -IPlatform/Inc \
          -IDrivers/STM32F7xx_HAL_Driver/Inc -IDrivers/STM32F7xx_HAL_Driver/Inc/Legacy \
          -IDrivers/CMSIS/Device/ST/STM32F7xx/Include -IDrivers/CMSIS/Include \
          -IDrivers/BSP/STM32746G-Discovery -IDrivers/BSP/Components/wm8994 -IDrivers/BSP/Components/Common \
          -IMiddlewares/ST/STM32_USB_Device_Library/Core/Inc -IMiddlewares/ST/STM32_USB_Device_Library/Class/MSC/Inc
CFLAGS  = $(MCU) $(DEFS) $(INCS) -std=gnu11 -O2 -g -Wall -ffunction-sections -fdata-sections
LDFLAGS = $(MCU) -TSTM32F746NGHX_FLASH.ld --specs=nano.specs -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map \
          -Wl,--start-group -lc -lm -Wl,--end-group

SRCS = $(wildcard App/Src/*.c Gui/Src/*.c Gui/Src/screens/*.c Lib/Src/*.c Platform/Src/*.c Core/Src/*.c \
        Drivers/STM32F7xx_HAL_Driver/Src/*.c Drivers/BSP/STM32746G-Discovery/*.c \
        Drivers/BSP/Components/wm8994/*.c Drivers/BSP/Components/ft5336/*.c \
        Middlewares/ST/STM32_USB_Device_Library/Core/Src/*.c \
        Middlewares/ST/STM32_USB_Device_Library/Class/MSC/Src/*.c)
ASMS = $(wildcard Core/Startup/*.s)
OBJS = $(addprefix $(BUILD)/,$(SRCS:.c=.o)) $(addprefix $(BUILD)/,$(ASMS:.s=.o))

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(MCU) -x assembler-with-cpp -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

test:
	sh Tools/test/run_tests.sh

clean:
	rm -rf $(BUILD)

.PHONY: all clean test
