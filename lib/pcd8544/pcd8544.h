#ifndef PCD8544_H
#define PCD8544_H

#include "stm32f1xx_hal.h"

#define PCD8544_WIDTH  84
#define PCD8544_HEIGHT 48

/* Initialize SPI/GPIO and run the PCD8544 reset + init command sequence. */
void pcd8544_init(SPI_HandleTypeDef *hspi);

/* Send a single command byte (DC low). */
void pcd8544_send_cmd(uint8_t cmd);

/* Send a single data byte (DC high). */
void pcd8544_send_data(uint8_t data);

#endif /* PCD8544_H */
