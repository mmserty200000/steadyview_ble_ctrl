#ifndef STEADYVIEW_SPI_H
#define STEADYVIEW_SPI_H
#include "stm32wbxx_hal.h"
#define SPI_CS_Pin GPIO_PIN_4
#define SPI_CS_GPIO_Port GPIOA
extern SPI_HandleTypeDef hspi1;
void MX_SPI1_Init(void);
void SPI1_RTOS_Init(void);
#endif
