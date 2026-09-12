#include "pcd8544.h"

#define PCD8544_RST_PORT GPIOA
#define PCD8544_RST_PIN  GPIO_PIN_0
#define PCD8544_CE_PORT  GPIOA
#define PCD8544_CE_PIN   GPIO_PIN_1
#define PCD8544_DC_PORT  GPIOA
#define PCD8544_DC_PIN   GPIO_PIN_2

static SPI_HandleTypeDef *pcd8544_hspi;

static void pcd8544_gpio_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = PCD8544_RST_PIN | PCD8544_CE_PIN | PCD8544_DC_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    HAL_GPIO_WritePin(PCD8544_CE_PORT, PCD8544_CE_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PCD8544_RST_PORT, PCD8544_RST_PIN, GPIO_PIN_SET);
}

void pcd8544_send_cmd(uint8_t cmd)
{
    HAL_GPIO_WritePin(PCD8544_DC_PORT, PCD8544_DC_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PCD8544_CE_PORT, PCD8544_CE_PIN, GPIO_PIN_RESET);
    HAL_SPI_Transmit(pcd8544_hspi, &cmd, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(PCD8544_CE_PORT, PCD8544_CE_PIN, GPIO_PIN_SET);
}

void pcd8544_send_data(uint8_t data)
{
    HAL_GPIO_WritePin(PCD8544_DC_PORT, PCD8544_DC_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PCD8544_CE_PORT, PCD8544_CE_PIN, GPIO_PIN_RESET);
    HAL_SPI_Transmit(pcd8544_hspi, &data, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(PCD8544_CE_PORT, PCD8544_CE_PIN, GPIO_PIN_SET);
}

void pcd8544_init(SPI_HandleTypeDef *hspi)
{
    pcd8544_hspi = hspi;

    pcd8544_gpio_init();

    /* PCD8544 reset pulse: hold RST low for 100ms before releasing. */
    HAL_GPIO_WritePin(PCD8544_RST_PORT, PCD8544_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(PCD8544_RST_PORT, PCD8544_RST_PIN, GPIO_PIN_SET);

    pcd8544_send_cmd(0x21); /* extended instruction set, PD=0, V=0, H=1 */
    pcd8544_send_cmd(0xB8); /* set Vop (contrast) */
    pcd8544_send_cmd(0x04); /* temperature coefficient */
    pcd8544_send_cmd(0x14); /* bias system, 1:48 */
    pcd8544_send_cmd(0x20); /* back to basic instruction set, H=0 */
    pcd8544_send_cmd(0x0C); /* normal display mode */
}
