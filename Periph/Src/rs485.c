/**
  ******************************************************************************
  * @file           : rs485.c
  * @brief          : Onboard RS485 interface implementation.
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

#include "rs485.h"

#define RS485_DIRECTION_PORT GPIOD
#define RS485_DIRECTION_PIN  7U
#define RS485_TIMEOUT_MS     1000U
#define RS485_USART_CLOCK_HZ 42000000UL
#define RS485_BAUD_RATE      115200UL
#define RS485_RX_CAPACITY    512U
#define RS485_RX_INDEX_MASK  (RS485_RX_CAPACITY - 1U)

static uint8_t rs485Initialized;
static uint8_t rs485RxBuffer[RS485_RX_CAPACITY];
static volatile uint16_t rs485RxHead;
static volatile uint16_t rs485RxTail;
static volatile Rs485_StatisticsTypeDef rs485Statistics;

Platform_StatusTypeDef Rs485_Init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
  (void)RCC->APB1ENR;
  Platform_GpioWrite(RS485_DIRECTION_PORT, RS485_DIRECTION_PIN, 0U);
  Platform_GpioConfigure(
    GPIOD, 7U, PLATFORM_GPIO_MODE_OUTPUT, PLATFORM_GPIO_PULL_NONE,
    PLATFORM_GPIO_SPEED_VERY_HIGH, 0U
  );
  Platform_GpioConfigure(
    GPIOD, 5U, PLATFORM_GPIO_MODE_ALTERNATE, PLATFORM_GPIO_PULL_UP,
    PLATFORM_GPIO_SPEED_VERY_HIGH, 7U
  );
  Platform_GpioConfigure(
    GPIOD, 6U, PLATFORM_GPIO_MODE_ALTERNATE, PLATFORM_GPIO_PULL_UP,
    PLATFORM_GPIO_SPEED_VERY_HIGH, 7U
  );
  USART2->BRR = (RS485_USART_CLOCK_HZ + (RS485_BAUD_RATE / 2U))
    / RS485_BAUD_RATE;
  rs485RxHead = 0U;
  rs485RxTail = 0U;
  rs485Statistics = (Rs485_StatisticsTypeDef){0};
  NVIC_SetPriority(USART2_IRQn, 6U);
  NVIC_EnableIRQ(USART2_IRQn);
  USART2->CR3 = USART_CR3_EIE;
  USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;
  rs485Initialized = 1U;
  return PLATFORM_STATUS_OK;
}

Platform_StatusTypeDef Rs485_Transmit(const uint8_t* data, size_t length) {
  if ((rs485Initialized == 0U) || (data == NULL))
    return PLATFORM_STATUS_ERROR;

  if (length == 0U)
    return PLATFORM_STATUS_OK;

  if (length > UINT16_MAX)
    return PLATFORM_STATUS_ERROR;

  Platform_GpioWrite(RS485_DIRECTION_PORT, RS485_DIRECTION_PIN, 1U);
  uint32_t started = Platform_GetTick();
  Platform_StatusTypeDef status = PLATFORM_STATUS_OK;
  for (size_t offset = 0U; offset < length; ++offset) {
    while ((USART2->SR & USART_SR_TXE) == 0U) {
      if ((Platform_GetTick() - started) >= RS485_TIMEOUT_MS) {
        status = PLATFORM_STATUS_TIMEOUT;
        break;
      }
    }
    if (status != PLATFORM_STATUS_OK)
      break;
    USART2->DR = data[offset];
  }
  while ((status == PLATFORM_STATUS_OK) && ((USART2->SR & USART_SR_TC) == 0U)) {
    if ((Platform_GetTick() - started) >= RS485_TIMEOUT_MS)
      status = PLATFORM_STATUS_TIMEOUT;
  }
  Platform_GpioWrite(RS485_DIRECTION_PORT, RS485_DIRECTION_PIN, 0U);

  return status;
}

size_t Rs485_Read(uint8_t* data, size_t capacity) {
  if ((rs485Initialized == 0U) || (data == NULL))
    return 0U;

  size_t copied = 0U;
  while ((copied < capacity) && (rs485RxTail != rs485RxHead)) {
    data[copied++] = rs485RxBuffer[rs485RxTail];
    rs485RxTail = (uint16_t)((rs485RxTail + 1U) & RS485_RX_INDEX_MASK);
  }
  return copied;
}

void Rs485_GetStatistics(Rs485_StatisticsTypeDef* statistics) {
  if (statistics == NULL)
    return;

  uint32_t interruptState = __get_PRIMASK();
  __disable_irq();
  statistics->receivedBytes = rs485Statistics.receivedBytes;
  statistics->droppedBytes = rs485Statistics.droppedBytes;
  statistics->overrunErrors = rs485Statistics.overrunErrors;
  statistics->framingErrors = rs485Statistics.framingErrors;
  statistics->noiseErrors = rs485Statistics.noiseErrors;
  statistics->parityErrors = rs485Statistics.parityErrors;
  if (interruptState == 0U)
    __enable_irq();
}

void Rs485_IRQHandler(void) {
  uint32_t status = USART2->SR;
  const uint32_t errorMask = USART_SR_ORE | USART_SR_FE
    | USART_SR_NE | USART_SR_PE;

  if ((status & USART_SR_ORE) != 0U)
    ++rs485Statistics.overrunErrors;
  if ((status & USART_SR_FE) != 0U)
    ++rs485Statistics.framingErrors;
  if ((status & USART_SR_NE) != 0U)
    ++rs485Statistics.noiseErrors;
  if ((status & USART_SR_PE) != 0U)
    ++rs485Statistics.parityErrors;

  if ((status & (USART_SR_RXNE | errorMask)) != 0U) {
    uint8_t value = (uint8_t)USART2->DR;
    if (((status & USART_SR_RXNE) != 0U) && ((status & errorMask) == 0U)) {
      uint16_t next = (uint16_t)((rs485RxHead + 1U) & RS485_RX_INDEX_MASK);
      if (next == rs485RxTail) {
        ++rs485Statistics.droppedBytes;
      } else {
        rs485RxBuffer[rs485RxHead] = value;
        rs485RxHead = next;
        ++rs485Statistics.receivedBytes;
      }
    }
  }
}
