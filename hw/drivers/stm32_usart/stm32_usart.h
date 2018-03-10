#pragma once

#if defined(STM32F4XX)
#    include "stm32f4xx.h"
#elif defined(STM32F2XX)
#    include "stm32f2xx.h"
#else
#    error "I have no idea what kind of stm32 this is; sorry"
#endif

#include <stdint.h>

const typedef struct {
    USART_TypeDef *usart;
    uint32_t usart_periph_bus;
    uint8_t af_usart;
    uint32_t baud;
    uint32_t gpio_pin_tx;
    uint32_t gpio_pin_rx;
    uint32_t gpio_pin_rts;
    uint32_t gpio_pin_cts;
    GPIO_TypeDef *gpio_ptr;
    uint32_t gpio_clock;
    uint32_t usart_clock;
    struct hw_usart_dma_t {
        uint32_t dma_clock;
        DMA_Stream_TypeDef *dma_tx_stream;
        DMA_Stream_TypeDef *dma_rx_stream;
        uint32_t dma_tx_channel;
        uint32_t dma_rx_channel;
        uint8_t dma_irq_tx_pri;
        uint8_t dma_irq_rx_pri;
        uint8_t dma_irq_tx_channel;
        uint8_t dma_irq_rx_channel;
        uint32_t dma_tx_channel_flags; // DMA_FLAG_FEIF7|DMA_FLAG_DMEIF7|DMA_FLAG_TEIF7|DMA_FLAG_HTIF7|DMA_FLAG_TCIF7
        uint32_t dma_rx_channel_flags;
        uint32_t dma_tx_irq_flag;
        uint32_t dma_rx_irq_flag;
    } dma;
} hw_usart_t;

void stm32_usart_rx_isr(hw_usart_t *usart);
void stm32_usart_tx_isr(hw_usart_t *usart);

#define STM32_USART_MK_DMA_FLAGS(CHAN) DMA_FLAG_FEIF##CHAN|DMA_FLAG_DMEIF##CHAN|DMA_FLAG_TEIF##CHAN|DMA_FLAG_HTIF##CHAN|DMA_FLAG_TCIF##CHAN


#define STM32_USART_MK_TX_IRQ_HANDLER(usart, dma_stream, callback) \
    void DMA ## _Stream ## dma_stream ## _IRQHandler(void) \
    { \
        stm32_usart_tx_isr(usart); \
        callback  (); \
    }


#define STM32_USART_MK_RX_IRQ_HANDLER(usart, dma_stream, callback) \
    void DMA ## _Stream ## dma_stream ## _IRQHandler(void) \
    { \
        stm32_usart_rx_isr(usart); \
        callback  ( ); \
    }


    
void stm32_usart_init_device(hw_usart_t *usart);
void stm32_usart_send_dma(hw_usart_t *usart, uint32_t *data, uint32_t len);
void stm32_usart_recv_dma(hw_usart_t *usart, uint32_t *data, size_t len);
void stm32_usart_set_baud(hw_usart_t *usart, uint32_t baud);
size_t stm32_usart_write(hw_usart_t *usart, const uint8_t *buf, size_t len);
size_t stm32_usart_read(hw_usart_t *usart, uint8_t *buf, size_t len);
