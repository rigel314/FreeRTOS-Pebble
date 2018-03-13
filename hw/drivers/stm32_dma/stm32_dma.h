/* 
 * stm32_dma.h
 * External-facing API for stm32 dma. Each driver that supports dma 
 * is expected to implement this module
 * RebbleOS
 *
 * Barry Carter <barry.carter@gmail.com>
 *
 */
#pragma once

#define STM32_DMA_MK_FLAGS(CHAN) DMA_FLAG_FEIF##CHAN|DMA_FLAG_DMEIF##CHAN|DMA_FLAG_TEIF##CHAN|DMA_FLAG_HTIF##CHAN|DMA_FLAG_TCIF##CHAN


const typedef struct {
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
} stm32_dma_t;

typedef void (*dma_callback)(void);

#define STM32_DMA_MK_TX_IRQ_HANDLER(dma_t, dma_channel, dma_stream, callback) \
    void DMA ## dma_channel ## _Stream ## dma_stream ## _IRQHandler(void) \
    { \
        stm32_dma_tx_isr(dma_t); \
        callback  (); \
        stm32_power_release(STM32_POWER_AHB1, dma_t->dma_clock); \
    }


#define STM32_DMA_MK_RX_IRQ_HANDLER(dma_t, dma_channel, dma_stream, callback) \
    void DMA ## dma_channel ## _Stream ## dma_stream ## _IRQHandler(void) \
    { \
        stm32_dma_rx_isr(dma_t); \
        callback  (); \
        stm32_power_release(STM32_POWER_AHB1, dma_t->dma_clock); \
    }

    
void stm32_dma_init_device(stm32_dma_t *dma);
void stm32_dma_tx_reset(stm32_dma_t *dma);
void stm32_dma_tx_init(stm32_dma_t *dma, void *periph_address, uint32_t *data, uint32_t len);
void stm32_dma_tx_begin(stm32_dma_t *dma);
void stm32_dma_rx_reset(stm32_dma_t *dma);
void stm32_dma_rx_init(stm32_dma_t *dma, void *periph_addr, uint32_t *data, size_t len);
void stm32_dma_rx_begin(stm32_dma_t *dma);

void stm32_dma_rx_isr(stm32_dma_t *dma);
void stm32_dma_tx_isr(stm32_dma_t *dma);
