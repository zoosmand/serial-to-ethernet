/**
  ******************************************************************************
  * @file           : rs485.h
  * @brief          : Onboard RS485 interface declarations.
  * @project        : STM32F407 Health Check
  * @platform       : STMicroelectronics STM32F407VET6
  * @created        : 29.07.2026
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017-2026 Dmitry Slobodchikov
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#ifndef RS485_H
#define RS485_H

#include "platform.h"

#include <stddef.h>
#include <stdint.h>

/**
  * @brief Initialize USART2 and the onboard RS485 transceiver control.
  * @retval (Platform_StatusTypeDef) PLATFORM_STATUS_OK when the interface is ready.
  */
Platform_StatusTypeDef Rs485_Init(void);

/**
  * @brief Transmit one complete buffer and then release the RS485 bus.
  * @param data (const uint8_t*) Buffer to transmit; must not be null.
  * @param length (size_t) Number of bytes to transmit, up to UINT16_MAX.
  * @retval (Platform_StatusTypeDef) Result of the bounded blocking transfer.
  */
Platform_StatusTypeDef Rs485_Transmit(const uint8_t* data, size_t length);

/**
  * @brief Copy bytes received from the RS485 bus without blocking.
  * @param data (uint8_t*) Destination buffer; must not be null.
  * @param capacity (size_t) Maximum bytes to copy.
  * @retval (size_t) Number of bytes copied in arrival order.
  */
size_t Rs485_Read(uint8_t* data, size_t capacity);

typedef struct {
  uint32_t receivedBytes;
  uint32_t droppedBytes;
  uint32_t overrunErrors;
  uint32_t framingErrors;
  uint32_t noiseErrors;
  uint32_t parityErrors;
} Rs485_StatisticsTypeDef;

/** @brief Read a consistent snapshot of the receive counters. */
void Rs485_GetStatistics(Rs485_StatisticsTypeDef* statistics);

/** @brief USART2 interrupt entry point used by the platform vector table. */
void Rs485_IRQHandler(void);

#endif /* RS485_H */
