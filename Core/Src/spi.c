#include "spi.h"
#include "steadyview_bus.h"
#include "main.h"
#include "cmsis_os2.h"

SPI_HandleTypeDef hspi1;
static osMutexId_t spi_mutex;

void SVBus_DelayUs(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000U);
    while ((uint32_t)(DWT->CYCCNT - start) < cycles) {}
}
void SVBus_DelayMs(uint32_t ms)
{
    osDelay((ms * osKernelGetTickFreq() + 999U) / 1000U);
}
static void pins_af(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_1 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Alternate = GPIO_AF5_SPI1;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);
}
void MX_SPI1_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_7, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_4;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
    pins_af();
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_1LINE;
    hspi1.Init.DataSize = SPI_DATASIZE_5BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256; // 250 kHz at PCLK2=64 MHz
    hspi1.Init.FirstBit = SPI_FIRSTBIT_LSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 7;
    hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
    hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
void SPI1_RTOS_Init(void)
{
    const osMutexAttr_t attr = {.name = "SPI1", .attr_bits = osMutexPrioInherit};
    spi_mutex = osMutexNew(&attr);
    if (!spi_mutex) Error_Handler();
}
static bool lock(void)
{
    return spi_mutex && !__get_IPSR() && osKernelGetState() == osKernelRunning &&
           osMutexAcquire(spi_mutex, osWaitForever) == osOK;
}
static void configure(uint32_t size, bool transmit)
{
    __HAL_SPI_DISABLE(&hspi1);
    MODIFY_REG(SPI1->CR2, SPI_CR2_DS | SPI_CR2_FRXTH,
               size | (size <= SPI_DATASIZE_8BIT ? SPI_CR2_FRXTH : 0));
    hspi1.Init.DataSize = size;
    if (transmit) SPI_1LINE_TX(&hspi1);
    else SPI_1LINE_RX(&hspi1);
}
static void finish(bool ok)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    __HAL_SPI_DISABLE(&hspi1);
    if (!ok) {
        // Also clear FIFO and any stuck BSY after a receive/timeout (ES0394).
        __HAL_RCC_SPI1_FORCE_RESET();
        __HAL_RCC_SPI1_RELEASE_RESET();
        hspi1.Init.DataSize = SPI_DATASIZE_5BIT;
        hspi1.State = HAL_SPI_STATE_RESET;
        if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
    }
    configure(SPI_DATASIZE_5BIT, true);
    SVBus_DelayUs(100);
    osMutexRelease(spi_mutex);
}
bool SVBus_Write(uint8_t address, uint32_t value)
{
    if (address > 15 || value > 0xFFFFFU || !lock()) return false;
    uint8_t frames[5] = {address | 0x10U, value & 31U, (value >> 5) & 31U,
                         (value >> 10) & 31U, (value >> 15) & 31U};
    configure(SPI_DATASIZE_5BIT, true);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    SVBus_DelayUs(100);
    bool ok = HAL_SPI_Transmit(&hspi1, frames, 5, 10) == HAL_OK;
    SVBus_DelayUs(100);
    finish(ok);
    return ok;
}
static bool before_timeout(uint32_t start)
{
    return (uint32_t)(DWT->CYCCNT - start) < SystemCoreClock / 1000U;
}
static bool wait_clock(bool high, uint32_t start)
{
    while (((GPIOA->IDR & GPIO_PIN_1) != 0) != high)
        if (!before_timeout(start)) return false;
    return true;
}
bool SVBus_Read(uint8_t address, uint32_t *value)
{
    if (address > 15 || !value || !lock()) return false;
    configure(SPI_DATASIZE_5BIT, true);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    SVBus_DelayUs(100);
    bool ok = HAL_SPI_Transmit(&hspi1, &address, 1, 10) == HAL_OK;
    if (ok) {
        configure(SPI_DATASIZE_10BIT, false); // PA7 released by BIDIOE=0
        uint16_t first = 0, second = 0;
        // RX-only clocks run continuously. Stop during the second (last) 10-bit
        // frame, after its first sample and before its last bit (RM0434).
        // CPU1 IRQs are masked for ~80 us; CPU2 radio remains independent.
        uint32_t mask = __get_PRIMASK();
        __disable_irq();
        uint32_t start = DWT->CYCCNT;
        __HAL_SPI_ENABLE(&hspi1);
        while (!(SPI1->SR & SPI_SR_RXNE) && before_timeout(start)) {}
        ok = (SPI1->SR & SPI_SR_RXNE) != 0;
        if (ok) {
            first = (uint16_t)SPI1->DR;
            // At RXNE, the tenth sample is done. Observe a falling edge then
            // the next rising edge: safely inside the final 10-bit frame.
            ok = wait_clock(false, start) && wait_clock(true, start);
        }
        __HAL_SPI_DISABLE(&hspi1);
        if (ok) {
            while (!(SPI1->SR & SPI_SR_RXNE) && before_timeout(start)) {}
            ok = (SPI1->SR & SPI_SR_RXNE) != 0;
            if (ok) second = (uint16_t)SPI1->DR;
            // Do not rely on BSY for master RX (ES0394 2.16.1).
            ok = ok && wait_clock(false, start);
        }
        __set_PRIMASK(mask);
        if (ok) *value = (first & 0x3ffU) | ((uint32_t)(second & 0x3ffU) << 10);
        SVBus_DelayUs(100);
    }
    finish(ok);
    return ok;
}
bool SVBus_MixPulse(void)
{
    if (!lock()) return false;
    __HAL_SPI_DISABLE(&hspi1);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_7, GPIO_PIN_SET);
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_1 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
    SVBus_DelayMs(100);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
    SVBus_DelayMs(500);
    pins_af();
    osMutexRelease(spi_mutex);
    return true;
}
