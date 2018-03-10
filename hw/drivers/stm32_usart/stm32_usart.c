/* display.c
 * Implementation of a modular usart driver
 * RebbleOS
 *
 * Author: Barry Carter <barry.carter@gmail.com>
 */

#if defined(STM32F4XX)
#    include "stm32f4xx.h"
#elif defined(STM32F2XX)
#    include "stm32f2xx.h"
#    include "stm32f2xx_gpio.h"
#    include "stm32f2xx_dma.h"
#    include "stm32f2xx_syscfg.h"
#    include "stm32f2xx_rcc.h"
#    include "stm32f2xx_usart.h"
#    include "misc.h"
#else
#    error "I have no idea what kind of stm32 this is; sorry"
#endif
#include "stdio.h"
#include "stm32_power.h"
#include "debug.h"
#include "log.h"
#include "stm32_usart.h"

void hw_usart_init(void)
{
    
}

#define USART_FLOW_CONTROL_DISABLED 0
#define USART_FLOW_CONTROL_ENABLED  1
#define USART_DMA_DISABLED 0
#define USART_DMA_ENABLED  1

static void _init_dma(hw_usart_t *usart);
static void _usart_init(hw_usart_t *usart);


void stm32_usart_init_device(hw_usart_t *usart)
{
    _usart_init(usart);
    
    if (usart->dma.dma_clock > 0)
    {
        _init_dma(usart);
    }
}


/*
 * Intialise the USART used for bluetooth
 * 
 * baud: How fast do you want to go. 
 * 0 does not mean any special. 
 * Please use a baud rate apprpriate for the clock
 */
static void _usart_init(hw_usart_t *usart)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    USART_InitTypeDef USART_InitStruct;
    NVIC_InitTypeDef nvic_init_struct;

    stm32_power_request(usart->usart_periph_bus, usart->usart_clock);
    stm32_power_request(STM32_POWER_AHB1, usart->gpio_clock);
    
    /* RX (10) TX (9) */
    GPIO_InitStruct.GPIO_Pin = usart->gpio_pin_tx | usart->gpio_pin_rx;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(usart->gpio_ptr, &GPIO_InitStruct);
    
    if (usart->gpio_pin_cts > 0)
    {
        /* CTS (11) RTS (12) */
        GPIO_InitStruct.GPIO_Pin = usart->gpio_pin_cts | usart->gpio_pin_rts;
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
        GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
        GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
        GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
        GPIO_Init(usart->gpio_ptr, &GPIO_InitStruct);
    }
    
    USART_DeInit(usart->usart);
    USART_StructInit(&USART_InitStruct);

    USART_InitStruct.USART_BaudRate = usart->baud;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    
    if (usart->gpio_pin_cts > 0)
    {
        /* AF for USART with hardware flow control */
        GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, usart->af_usart);
        GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, usart->af_usart);
        GPIO_PinAFConfig(GPIOA, GPIO_PinSource11, usart->af_usart);
        GPIO_PinAFConfig(GPIOA, GPIO_PinSource12, usart->af_usart);
        
        USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_RTS_CTS;
    }
    USART_Init(usart->usart, &USART_InitStruct);
    
    USART_Cmd(usart->usart, ENABLE);
    
//     stm32_power_release(STM32_POWER_APB2, usart->usart_clock);
    stm32_power_release(STM32_POWER_AHB1, usart->gpio_clock);
}

/*
 * initialise the DMA channels for transferring data
 */
static void _init_dma(hw_usart_t *usart)
{
    NVIC_InitTypeDef nvic_init_struct;
    DMA_InitTypeDef dma_init_struct;
    
    stm32_power_request(STM32_POWER_AHB1, usart->dma.dma_clock);

    /* TX init */
    DMA_DeInit(usart->dma.dma_tx_stream);
    DMA_StructInit(&dma_init_struct);
    dma_init_struct.DMA_PeripheralBaseAddr = (uint32_t)&usart->usart->DR;
    dma_init_struct.DMA_Memory0BaseAddr = (uint32_t)0;
    dma_init_struct.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    dma_init_struct.DMA_Channel = usart->dma.dma_tx_channel;
    dma_init_struct.DMA_BufferSize = 1;
    dma_init_struct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma_init_struct.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma_init_struct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma_init_struct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    dma_init_struct.DMA_Mode = DMA_Mode_Normal;
    dma_init_struct.DMA_Priority = DMA_Priority_VeryHigh;
    dma_init_struct.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_Init(usart->dma.dma_tx_stream, &dma_init_struct);
    
    /* Enable the interrupt for stream copy completion */
    nvic_init_struct.NVIC_IRQChannel = usart->dma.dma_irq_rx_channel;
    nvic_init_struct.NVIC_IRQChannelPreemptionPriority = usart->dma.dma_irq_rx_pri;
    nvic_init_struct.NVIC_IRQChannelSubPriority = 0;
    nvic_init_struct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic_init_struct);
    
    nvic_init_struct.NVIC_IRQChannel = usart->dma.dma_irq_tx_channel;
    nvic_init_struct.NVIC_IRQChannelPreemptionPriority = usart->dma.dma_irq_tx_pri;
    nvic_init_struct.NVIC_IRQChannelSubPriority = 0;
    nvic_init_struct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic_init_struct);    
    
    stm32_power_release(STM32_POWER_AHB1, usart->dma.dma_clock);
}


/*
 * Request transmission of the buffer provider
 */
void stm32_usart_send_dma(hw_usart_t *usart, uint32_t *data, uint32_t len)
{
    /* XXX released in IRQ */
    stm32_power_request(usart->usart_periph_bus, usart->usart_clock);
    stm32_power_request(STM32_POWER_AHB1, usart->gpio_clock);
    stm32_power_request(STM32_POWER_AHB1, usart->dma.dma_clock);
    
//     stm32_power_request(STM32_POWER_AHB1, RCC_AHB1Periph_GPIOB);   
//     stm32_power_request(STM32_POWER_AHB1, RCC_AHB1Periph_GPIOE);
//     stm32_power_request(STM32_POWER_APB1, RCC_APB1Periph_UART8);
   
    DMA_InitTypeDef dma_init_struct;
    NVIC_InitTypeDef nvic_init_struct;
    
    /* Configure DMA controller to manage TX DMA requests */
    DMA_Cmd(usart->dma.dma_tx_stream, DISABLE);
    while (usart->dma.dma_tx_stream->CR & DMA_SxCR_EN);

    USART_DMACmd(usart->usart, USART_DMAReq_Tx, DISABLE);
    DMA_DeInit(usart->dma.dma_tx_stream);
    DMA_ClearFlag(usart->dma.dma_tx_stream, usart->dma.dma_tx_channel_flags);

    DMA_StructInit(&dma_init_struct);
    dma_init_struct.DMA_Channel = usart->dma.dma_tx_channel;
    /* set the pointer to the USART DR register */
    dma_init_struct.DMA_PeripheralBaseAddr = (uint32_t)&(usart->usart->DR);
    dma_init_struct.DMA_Memory0BaseAddr = (uint32_t)data;
    dma_init_struct.DMA_BufferSize = len;
    dma_init_struct.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    dma_init_struct.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma_init_struct.DMA_Mode = DMA_Mode_Normal;
    dma_init_struct.DMA_PeripheralInc  = DMA_PeripheralInc_Disable;
    dma_init_struct.DMA_FIFOMode  = DMA_FIFOMode_Disable;
    dma_init_struct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
    dma_init_struct.DMA_PeripheralDataSize = DMA_MemoryDataSize_Byte;
    dma_init_struct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    dma_init_struct.DMA_Priority = DMA_Priority_Low;
    DMA_Init(usart->dma.dma_tx_stream, &dma_init_struct);
    
    /* Enable the stream IRQ, USART, DMA and then DMA interrupts in that order */
    NVIC_EnableIRQ(usart->dma.dma_irq_tx_channel);
    USART_Cmd(usart->usart, ENABLE);
    DMA_Cmd(usart->dma.dma_tx_stream, ENABLE);
    USART_DMACmd(usart->usart, USART_DMAReq_Tx, ENABLE);
    DMA_ITConfig(usart->dma.dma_tx_stream, DMA_IT_TC, ENABLE);
}

/*
 * Some data arrived from the bluetooth stack
 */
void stm32_usart_recv_dma(hw_usart_t *usart, uint32_t *data, size_t len)
{
    DMA_InitTypeDef dma_init_struct;

    stm32_power_request(usart->usart_periph_bus, usart->usart_clock);
    stm32_power_request(STM32_POWER_AHB1, usart->gpio_clock);
    stm32_power_request(STM32_POWER_AHB1, usart->dma.dma_clock);
    
//     stm32_power_request(STM32_POWER_AHB1, RCC_AHB1Periph_GPIOB); 
//     stm32_power_request(STM32_POWER_AHB1, RCC_AHB1Periph_GPIOE);
//     stm32_power_request(STM32_POWER_APB1, RCC_APB1Periph_UART8);
    
    /* Configure DMA controller to manage RX DMA requests */
    DMA_Cmd(usart->dma.dma_rx_stream, DISABLE);
    while (usart->dma.dma_rx_stream->CR & DMA_SxCR_EN);

    DMA_ClearFlag(usart->dma.dma_rx_stream, usart->dma.dma_rx_channel_flags);
    DMA_StructInit(&dma_init_struct);
    /* set the pointer to the USART DR register */
    dma_init_struct.DMA_PeripheralBaseAddr = (uint32_t) &usart->usart->DR;
    dma_init_struct.DMA_Channel = usart->dma.dma_rx_channel;
    dma_init_struct.DMA_DIR = DMA_DIR_PeripheralToMemory;
    dma_init_struct.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma_init_struct.DMA_Memory0BaseAddr = (uint32_t)data;
    dma_init_struct.DMA_BufferSize = len;
    dma_init_struct.DMA_PeripheralInc  = DMA_PeripheralInc_Disable;
    dma_init_struct.DMA_FIFOMode  = DMA_FIFOMode_Disable;
    dma_init_struct.DMA_PeripheralDataSize = DMA_MemoryDataSize_Byte;
    dma_init_struct.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    dma_init_struct.DMA_Priority = DMA_Priority_High;
    DMA_Init(usart->dma.dma_rx_stream, &dma_init_struct);
    
    DMA_Cmd(usart->dma.dma_rx_stream, ENABLE);
    USART_DMACmd(usart->usart, USART_DMAReq_Rx, ENABLE);
    DMA_ITConfig(usart->dma.dma_rx_stream, DMA_IT_TC, ENABLE);
}

/* 
 * Set or change the baud rate of the USART
 * This is safe to be done any time there is no transaction in progress
 */
void stm32_usart_set_baud(hw_usart_t *usart, uint32_t baud)
{
    /* we don't want to go through init or set baud in the struct */
    /* just update it */
}

/*
 * IRQ Handler for RX of data complete
 */
void stm32_usart_rx_isr(hw_usart_t *usart)
{
    if (DMA_GetITStatus(usart->dma.dma_rx_stream, usart->dma.dma_rx_irq_flag) != RESET)
    {
        DMA_ClearITPendingBit(usart->dma.dma_rx_stream, usart->dma.dma_rx_irq_flag);
        USART_DMACmd(usart->usart, USART_DMAReq_Rx, DISABLE);
        
        /* release the clocks we are no longer requiring */
        stm32_power_release(usart->usart_periph_bus, usart->usart_clock);
        stm32_power_release(STM32_POWER_AHB1, usart->gpio_clock);
        stm32_power_release(STM32_POWER_AHB1, usart->dma.dma_clock);
        
//         stm32_power_release(STM32_POWER_AHB1, RCC_AHB1Periph_GPIOB);
//         stm32_power_release(STM32_POWER_AHB1, RCC_AHB1Periph_GPIOE);
//         stm32_power_release(STM32_POWER_APB1, RCC_APB1Periph_UART8);
        /* Trigger the recipient interrupt handler */
    }
    else
    {
        DRV_LOG("BT", APP_LOG_LEVEL_DEBUG, "DMA2 RX ERROR?");
    }        
}

/*
 * IRQ Handler for TX of data complete
 */
void stm32_usart_tx_isr(hw_usart_t *usart)
{
    if (DMA_GetITStatus(usart->dma.dma_tx_stream, usart->dma.dma_tx_irq_flag) != RESET)
    {
        DMA_ClearITPendingBit(usart->dma.dma_tx_stream, usart->dma.dma_tx_irq_flag);
        USART_DMACmd(usart->usart, USART_DMAReq_Tx, DISABLE);

        stm32_power_release(usart->usart_periph_bus, usart->usart_clock);
        stm32_power_release(STM32_POWER_AHB1, usart->gpio_clock);
        stm32_power_release(STM32_POWER_AHB1, usart->dma.dma_clock);
        
//         stm32_power_release(STM32_POWER_AHB1, RCC_AHB1Periph_GPIOB);
//         stm32_power_release(STM32_POWER_AHB1, RCC_AHB1Periph_GPIOE);
//         stm32_power_release(STM32_POWER_APB1, RCC_APB1Periph_UART8);
        /* Trigger the stack's interrupt handler */

    }
    else
    {
        DRV_LOG("HW_USART", APP_LOG_LEVEL_ERROR, "DMA TX ERROR TEIF");
    }
}


/* Util function to directly read and write the USART */
size_t stm32_usart_write(hw_usart_t *usart, const uint8_t *buf, size_t len)
{
    /* From tintin. Checks TC not TXE
        for (i = 0; i < len; i++) {
        if (p[i] == '\n') {
            while (!(USART3->SR & USART_SR_TC));
            USART3->DR = '\r';
        }

        while (!(USART3->SR & USART_SR_TC));
        USART3->DR = p[i];
    }
    */
    int i;
    for (i = 0; i < len; i++)
    {
        if (buf[i] == '\n') {
            while (!(usart->usart->SR & USART_FLAG_TXE));
            usart->usart->DR = '\r';
        }
        while (!(usart->usart->SR & USART_FLAG_TXE));
        USART_SendData(usart->usart, ((uint8_t *) buf)[i]);
    }
    
    return i;
}

size_t stm32_usart_read(hw_usart_t *usart, uint8_t *buf, size_t len)
{
    int i;
    for (i = 0; i < len; i++)
    {
        while (!(usart->usart->SR & USART_FLAG_RXNE));
        ((uint8_t *) buf)[i] = USART_ReceiveData(usart->usart);
    }
    return i;
}
