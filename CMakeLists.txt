cmake_minimum_required(VERSION 3.20)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake" CACHE FILEPATH "Toolchain")
project(steadyview_ble_ctrl LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(NANOPB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/Middlewares/Third_Party/nanopb")
add_executable(${PROJECT_NAME}.elf
  Protocol/steadyview.pb.c
  Core/Src/control_auth.c
  Core/Src/auth_platform.c
  Vendor/Middlewares/Third_Party/tinycrypt/lib/source/sha256.c
  Vendor/Middlewares/Third_Party/tinycrypt/lib/source/hmac.c
  Vendor/Middlewares/Third_Party/tinycrypt/lib/source/utils.c
  Core/Src/settings_store.c
  Core/Src/settings_flash.c
  Core/Src/steadyview.c
  Core/Src/control_protocol.c
  Core/Src/control_app.c
  STM32_WPAN/App/control_service.c
  ${NANOPB_DIR}/pb_common.c
  ${NANOPB_DIR}/pb_encode.c
  ${NANOPB_DIR}/pb_decode.c
  Vendor/Utilities/lpm/tiny_lpm/stm32_lpm.c
  Core/Src/system_stm32wbxx.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_cortex.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_dma.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_dma_ex.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_exti.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_flash.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_flash_ex.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_gpio.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_hsem.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_ipcc.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_pwr.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_pwr_ex.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_rcc.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_rcc_ex.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_rng.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_rtc.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_rtc_ex.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_tim.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_tim_ex.c
  Vendor/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c
  Vendor/Middlewares/Third_Party/FreeRTOS/Source/croutine.c
  Vendor/Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
  Vendor/Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c
  Vendor/Middlewares/Third_Party/FreeRTOS/Source/list.c
  Vendor/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c
  Vendor/Middlewares/Third_Party/FreeRTOS/Source/queue.c
  Vendor/Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
  Vendor/Middlewares/Third_Party/FreeRTOS/Source/tasks.c
  Vendor/Middlewares/Third_Party/FreeRTOS/Source/timers.c
  Vendor/Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gap_aci.c
  Vendor/Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gatt_aci.c
  Vendor/Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hal_aci.c
  Vendor/Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hci_le.c
  Vendor/Middlewares/ST/STM32_WPAN/ble/core/auto/ble_l2cap_aci.c
  Vendor/Middlewares/ST/STM32_WPAN/utilities/dbg_trace.c
  Vendor/Middlewares/ST/STM32_WPAN/ble/svc/Src/dis.c
  Vendor/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/hci_tl.c
  Vendor/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/hci_tl_if.c
  Vendor/Middlewares/ST/STM32_WPAN/ble/core/template/osal.c
  Vendor/Middlewares/ST/STM32_WPAN/utilities/otp.c
  Vendor/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/shci/shci.c
  Vendor/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/shci_tl.c
  Vendor/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/shci_tl_if.c
  Vendor/Middlewares/ST/STM32_WPAN/utilities/stm_list.c
  Vendor/Middlewares/ST/STM32_WPAN/utilities/stm_queue.c
  Vendor/Middlewares/ST/STM32_WPAN/ble/svc/Src/svc_ctl.c
  Vendor/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/tl_mbox.c
  Core/Src/app_debug.c
  Core/Src/app_entry.c
  Core/Src/app_freertos.c
  Core/Src/freertos_port.c
  Core/Src/hw_timerserver.c
  Core/Src/main.c
  Core/Src/stm32_lpm_if.c
  Core/Src/stm32wbxx_hal_msp.c
  Core/Src/stm32wbxx_hal_timebase_tim.c
  Core/Src/stm32wbxx_it.c
  STM32_WPAN/App/app_ble.c
  STM32_WPAN/App/dis_app.c
  STM32_WPAN/Target/hw_ipcc.c
  Vendor/Drivers/CMSIS/Device/ST/STM32WBxx/Source/Templates/gcc/startup_stm32wb55xx_cm4.s
  Core/Src/spi.c
  Vendor/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_spi.c)
target_include_directories(${PROJECT_NAME}.elf PRIVATE
  Protocol
  Vendor/Middlewares/Third_Party/tinycrypt/lib/include
  ${NANOPB_DIR}
  Core/Inc
  STM32_WPAN/App
  Vendor/Drivers/STM32WBxx_HAL_Driver/Inc
  Vendor/Drivers/STM32WBxx_HAL_Driver/Inc/Legacy
  Vendor/Utilities/lpm/tiny_lpm
  Vendor/Middlewares/Third_Party/FreeRTOS/Source/include
  Vendor/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
  Vendor/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
  Vendor/Middlewares/ST/STM32_WPAN
  Vendor/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread
  Vendor/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl
  Vendor/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/shci
  Vendor/Middlewares/ST/STM32_WPAN/utilities
  Vendor/Middlewares/ST/STM32_WPAN/ble/core
  Vendor/Middlewares/ST/STM32_WPAN/ble/core/auto
  Vendor/Middlewares/ST/STM32_WPAN/ble/core/template
  Vendor/Middlewares/ST/STM32_WPAN/ble/svc/Inc
  Vendor/Middlewares/ST/STM32_WPAN/ble/svc/Src
  Vendor/Drivers/CMSIS/Device/ST/STM32WBxx/Include
  Vendor/Middlewares/ST/STM32_WPAN/ble
  Vendor/Drivers/CMSIS/Include
  Vendor/Drivers/CMSIS/RTOS2/Include)
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE USE_HAL_DRIVER STM32WB55xx)
target_compile_options(${PROJECT_NAME}.elf PRIVATE -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -Os -g3 -ffunction-sections -fdata-sections -Wall -Wextra)
target_link_options(${PROJECT_NAME}.elf PRIVATE -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard --specs=nano.specs --specs=nosys.specs -T${CMAKE_CURRENT_SOURCE_DIR}/STM32WB55CGUX_FLASH.ld -Wl,--gc-sections,-Map=${PROJECT_NAME}.map -Wl,--print-memory-usage)
set_property(TARGET ${PROJECT_NAME}.elf APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/STM32WB55CGUX_FLASH.ld)
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
 COMMAND arm-none-eabi-objcopy -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
 COMMAND arm-none-eabi-objcopy -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
 COMMAND arm-none-eabi-size $<TARGET_FILE:${PROJECT_NAME}.elf>)
