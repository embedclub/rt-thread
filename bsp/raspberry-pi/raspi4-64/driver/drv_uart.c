/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2020-04-16     bigmagic       first version
 */

#include <rthw.h>
#include <rtthread.h>
#include <rtdevice.h>

#include "board.h"
#include "drv_uart.h"
#include "drv_gpio.h"

struct hw_uart_device
{
    rt_ubase_t hw_base;
    rt_uint32_t irqno;
};

#ifdef RT_USING_UART0
static struct rt_serial_device _serial0;
#endif

static rt_err_t uart_configure(struct rt_serial_device *serial, struct serial_configure *cfg)
{
    struct hw_uart_device *uart;
    uint32_t bauddiv = (UART_REFERENCE_CLOCK / cfg->baud_rate)* 1000 / 16;
    uint32_t ibrd = bauddiv / 1000;

    RT_ASSERT(serial != RT_NULL);
    uart = (struct hw_uart_device *)serial->parent.user_data;

    if(uart->hw_base == AUX_BASE)
    {
        prev_raspi_pin_mode(GPIO_PIN_14, ALT5);
        prev_raspi_pin_mode(GPIO_PIN_15, ALT5);

        AUX_ENABLES(uart->hw_base)      = 1;    /* Enable UART1 */
        AUX_MU_IER_REG(uart->hw_base)   = 0;    /* Disable interrupt */
        AUX_MU_CNTL_REG(uart->hw_base)  = 0;    /* Disable Transmitter and Receiver */
        AUX_MU_LCR_REG(uart->hw_base)   = 3;    /* Works in 8-bit mode */
        AUX_MU_MCR_REG(uart->hw_base)   = 0;    /* Disable RTS */
        AUX_MU_IIR_REG(uart->hw_base)   = 0xC6; /* Enable FIFO, Clear FIFO */
        AUX_MU_BAUD_REG(uart->hw_base)  = 270;  /* 115200 = system clock 250MHz / (8 * (baud + 1)), baud = 270 */
        AUX_MU_CNTL_REG(uart->hw_base)  = 3;    /* Enable Transmitter and Receiver */
        return RT_EOK;
    }

    if(uart->hw_base == UART0_BASE)
    {
        prev_raspi_pin_mode(GPIO_PIN_14, ALT0);
        prev_raspi_pin_mode(GPIO_PIN_15, ALT0);
    }

    if(uart->hw_base == UART3_BASE)
    {
        prev_raspi_pin_mode(GPIO_PIN_4, ALT4);
        prev_raspi_pin_mode(GPIO_PIN_5, ALT4);
    }

    if(uart->hw_base == UART4_BASE)
    {
        prev_raspi_pin_mode(GPIO_PIN_8, ALT4);
        prev_raspi_pin_mode(GPIO_PIN_9, ALT4);
    }

    if(uart->hw_base == UART5_BASE)
    {
        prev_raspi_pin_mode(GPIO_PIN_12, ALT4);
        prev_raspi_pin_mode(GPIO_PIN_13, ALT4);
    }

    PL011_REG_CR(uart->hw_base) = 0;/*Clear UART setting*/
    PL011_REG_LCRH(uart->hw_base) = 0;/*disable FIFO*/
    PL011_REG_IBRD(uart->hw_base) = ibrd;
    PL011_REG_FBRD(uart->hw_base) = (((bauddiv - ibrd * 1000) * 64 + 500) / 1000);
    PL011_REG_LCRH(uart->hw_base) = PL011_LCRH_WLEN_8;/*FIFO*/
    PL011_REG_CR(uart->hw_base) = PL011_CR_UARTEN | PL011_CR_TXE | PL011_CR_RXE;/*art enable, TX/RX enable*/

    return RT_EOK;
}

static rt_err_t uart_control(struct rt_serial_device *serial, int cmd, void *arg)
{
    struct hw_uart_device *uart;

    RT_ASSERT(serial != RT_NULL);
    uart = (struct hw_uart_device *)serial->parent.user_data;

    switch (cmd)
    {
    case RT_DEVICE_CTRL_CLR_INT:
        /* disable rx irq */
        PL011_REG_IMSC(uart->hw_base) &= ~((uint32_t)PL011_IMSC_RXIM);
        rt_hw_interrupt_mask(uart->irqno);
        break;

    case RT_DEVICE_CTRL_SET_INT:
        /* enable rx irq */
        if(uart->hw_base == AUX_BASE)
        {
            AUX_MU_IER_REG(uart->hw_base) = 0x1;
        }
        else
        {
            PL011_REG_IMSC(uart->hw_base) |= PL011_IMSC_RXIM;
        }
        rt_hw_interrupt_umask(uart->irqno);
        break;
    }

    return RT_EOK;
}

static int uart_putc(struct rt_serial_device *serial, char c)
{
    struct hw_uart_device *uart;

    RT_ASSERT(serial != RT_NULL);
    uart = (struct hw_uart_device *)serial->parent.user_data;

    if(uart->hw_base == AUX_BASE)
    {
        while (!(AUX_MU_LSR_REG(uart->hw_base) & 0x20));
        AUX_MU_IO_REG(uart->hw_base) = c;
    }
    else
    {
        while ((PL011_REG_FR(uart->hw_base) & PL011_FR_TXFF));
        PL011_REG_DR(uart->hw_base) = (uint8_t)c;
    }

    return 1;
}

static int uart_getc(struct rt_serial_device *serial)
{
    int ch = -1;
    struct hw_uart_device *uart;

    RT_ASSERT(serial != RT_NULL);
    uart = (struct hw_uart_device *)serial->parent.user_data;

    if(uart->hw_base == AUX_BASE)
    {
        if ((AUX_MU_LSR_REG(uart->hw_base) & 0x01))
        {
            ch = AUX_MU_IO_REG(uart->hw_base) & 0xff;
        }
    }
    else
    {
        if((PL011_REG_FR(uart->hw_base) & PL011_FR_RXFE) == 0)
        {
            ch = PL011_REG_DR(uart->hw_base) & 0xff;
        }
    }

    return ch;
}

static const struct rt_uart_ops _uart_ops =
{
    uart_configure,
    uart_control,
    uart_putc,
    uart_getc,
};

static void rt_hw_uart_isr(int irqno, void *param)
{
#ifdef RT_USING_UART0
    if((PACTL_CS & IRQ_UART0) == IRQ_UART0)
    {
        PACTL_CS &=  ~(IRQ_UART0);
        rt_hw_serial_isr(&_serial0, RT_SERIAL_EVENT_RX_IND);
        PL011_REG_ICR(UART0_BASE) = PL011_INTERRUPT_RECEIVE;
    }
#endif
}

#ifdef RT_USING_UART0
/* UART device driver structure */
static struct hw_uart_device _uart0_device =
{
    UART0_BASE,
    IRQ_PL011,
};
#endif

static struct rt_serial_device _serial0;

int rt_hw_uart_init(void)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;
#ifdef RT_USING_UART0
    struct hw_uart_device *uart0;
    uart0 = &_uart0_device;

    _serial0.ops    = &_uart_ops;
    _serial0.config = config;

    uart0->hw_base = UART0_BASE;


    /* register UART0 device */
    rt_hw_serial_register(&_serial0, "uart0",
                          RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX,
                          uart0);
    rt_hw_interrupt_install(uart0->irqno, rt_hw_uart_isr, &_serial0, "uart0");

#endif
    return 0;
}
