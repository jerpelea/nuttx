/****************************************************************************
 * include/nuttx/serial/serial.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#ifndef __INCLUDE_NUTTX_SERIAL_SERIAL_H
#define __INCLUDE_NUTTX_SERIAL_SERIAL_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <termios.h>

#include <nuttx/fs/fs.h>
#include <nuttx/semaphore.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Maximum number of threads than can be waiting for POLL events */

#ifndef CONFIG_SERIAL_NPOLLWAITERS
#  define CONFIG_SERIAL_NPOLLWAITERS 4
#endif

/* RX flow control */

#ifndef CONFIG_SERIAL_IFLOWCONTROL
#  undef CONFIG_SERIAL_IFLOWCONTROL_WATERMARKS
#endif

#ifndef CONFIG_SERIAL_IFLOWCONTROL_WATERMARKS
#  undef CONFIG_SERIAL_IFLOWCONTROL_LOWER_WATERMARK
#  undef CONFIG_SERIAL_IFLOWCONTROL_UPPER_WATERMARK
#endif

#ifdef CONFIG_SERIAL_IFLOWCONTROL_WATERMARKS
#  ifndef CONFIG_SERIAL_IFLOWCONTROL_LOWER_WATERMARK
#    define CONFIG_SERIAL_IFLOWCONTROL_LOWER_WATERMARK 10
#  endif

#  ifndef CONFIG_SERIAL_IFLOWCONTROL_UPPER_WATERMARK
#    define CONFIG_SERIAL_IFLOWCONTROL_UPPER_WATERMARK 90
#  endif

#  if CONFIG_SERIAL_IFLOWCONTROL_LOWER_WATERMARK > \
      CONFIG_SERIAL_IFLOWCONTROL_UPPER_WATERMARK
#    warning Lower watermark pct exceeds upper watermark pct
#  endif
#endif

/* vtable access helpers */

#define uart_setup(dev)          dev->ops->setup(dev)
#define uart_shutdown(dev)       dev->ops->shutdown(dev)
#define uart_attach(dev)         dev->ops->attach(dev)
#define uart_detach(dev)         dev->ops->detach(dev)
#define uart_enabletxint(dev)    dev->ops->txint(dev, true)
#define uart_disabletxint(dev)   dev->ops->txint(dev, false)
#define uart_enablerxint(dev)    dev->ops->rxint(dev, true)
#define uart_disablerxint(dev)   dev->ops->rxint(dev, false)
#define uart_rxavailable(dev)    dev->ops->rxavailable(dev)
#define uart_txready(dev)        dev->ops->txready(dev)
#define uart_txempty(dev)        dev->ops->txempty(dev)
#define uart_send(dev,ch)        dev->ops->send(dev,ch)
#define uart_receive(dev,s)      dev->ops->receive(dev,s)
#define uart_recvbuf(dev,b,l)    dev->ops->recvbuf(dev,b,l)
#define uart_sendbuf(dev,b,l)    dev->ops->sendbuf(dev,b,l)

#define uart_release(dev)      \
  ((dev)->ops->release ? (dev)->ops->release(dev) : -ENOSYS)

#ifdef CONFIG_SERIAL_TXDMA
#define uart_dmasend(dev)      \
  ((dev)->ops->dmasend ? (dev)->ops->dmasend(dev) : -ENOSYS)

#define uart_dmatxavail(dev)   \
  ((dev)->ops->dmatxavail ? (dev)->ops->dmatxavail(dev) : -ENOSYS)
#endif

#ifdef CONFIG_SERIAL_RXDMA
#define uart_dmareceive(dev)   \
  ((dev)->ops->dmareceive ? (dev)->ops->dmareceive(dev) : -ENOSYS)

#define uart_dmarxfree(dev)    \
  ((dev)->ops->dmarxfree ? (dev)->ops->dmarxfree(dev) : -ENOSYS)
#endif

#ifdef CONFIG_SERIAL_IFLOWCONTROL
#  define uart_rxflowcontrol(dev,n,u) \
    (dev->ops->rxflowcontrol && dev->ops->rxflowcontrol(dev,n,u))
#endif

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* This structure defines one serial I/O buffer.
 * The serial infrastructure will initialize the 'sem' field but all other
 * fields must be initialized by the caller of uart_register().
 *
 * Maximum buffer size is reduced to 8 bits on architectures where 16bit
 * load takes two instructions and is therefore not atomic. This prevents
 * corrupted read if the value is changed in an interrupt handler while
 * being loaded in non-interrupt code.
 */

#ifndef CONFIG_ARCH_LDST_16BIT_NOT_ATOMIC
typedef int16_t sbuf_size_t;
#else
typedef uint8_t sbuf_size_t;
#endif

struct uart_buffer_s
{
  mutex_t              lock;   /* Used to control exclusive access
                                * to the buffer */
  volatile sbuf_size_t head;   /* Index to the head [IN] index
                                * in the buffer */
  volatile sbuf_size_t tail;   /* Index to the tail [OUT] index
                                * in the buffer */
  sbuf_size_t          size;   /* The allocated size of the buffer */
  FAR char            *buffer; /* Pointer to the allocated buffer memory */
};

#if defined(CONFIG_SERIAL_RXDMA) || defined(CONFIG_SERIAL_TXDMA)
struct uart_dmaxfer_s
{
  FAR char        *buffer;  /* First DMA buffer */
  FAR char        *nbuffer; /* Next DMA buffer */
  size_t           length;  /* Length of first DMA buffer */
  size_t           nlength; /* Length of next DMA buffer */
  size_t           nbytes;  /* Bytes actually transferred by DMA from both buffers */
};
#endif /* CONFIG_SERIAL_RXDMA || CONFIG_SERIAL_TXDMA */

/* This structure defines all of the operations providd by the architecture
 * specific logic.  All fields must be provided with non-NULL function
 * pointers by the caller of uart_register().
 */

struct uart_dev_s;
struct uart_ops_s
{
  /* Configure the UART baud, bits, parity, fifos, etc. This method is called
   * the first time that the serial port is opened. For the serial console,
   * this will occur very early in initialization; for other serial ports
   * this will occur when the port is first opened. This setup does not
   * include attaching or enabling interrupts. That portion of the UART setup
   * is performed when the attach() method is called.
   */

  CODE int (*setup)(FAR struct uart_dev_s *dev);

  /* Disable the UART.  This method is called when the serial port is closed.
   * This method reverses the operation the setup method.
   * NOTE that the serial console is never shutdown.
   */

  CODE void (*shutdown)(FAR struct uart_dev_s *dev);

  /* Configure the UART to operation in interrupt driven mode. This method is
   * called when the serial port is opened. Normally, this is just after the
   * the setup() method is called, however, the serial console may operate in
   * a non-interrupt driven mode during the boot phase.
   *
   * RX and TX interrupts are not enabled when by the attach method (unless
   * the hardware supports multiple levels of interrupt enabling). The RX
   * and TX interrupts are not enabled until the txint() and rxint() methods
   * are called.
   */

  CODE int (*attach)(FAR struct uart_dev_s *dev);

  /* Detach UART interrupts. This method is called when the serial port is
   * closed normally just before the shutdown method is called.
   * The exception is the serial console which is never shutdown.
   */

  CODE void (*detach)(FAR struct uart_dev_s *dev);

  /* All ioctl calls will be routed through this method */

  CODE int (*ioctl)(FAR struct file *filep, int cmd, unsigned long arg);

  /* Called (usually) from the interrupt level to receive one character from
   * the UART.  Error bits associated with the receipt are provided in the
   * the return 'status'.
   */

  CODE int (*receive)(FAR struct uart_dev_s *dev, FAR unsigned int *status);

  /* Call to enable or disable RX interrupts */

  CODE void (*rxint)(FAR struct uart_dev_s *dev, bool enable);

  /* Return true if the receive data is available */

  CODE bool (*rxavailable)(FAR struct uart_dev_s *dev);

#ifdef CONFIG_SERIAL_IFLOWCONTROL
  /* Return true if UART activated RX flow control to block more incoming
   * data.
   */

  CODE bool (*rxflowcontrol)(FAR struct uart_dev_s *dev,
                             unsigned int nbuffered, bool upper);
#endif

#ifdef CONFIG_SERIAL_TXDMA
  /* Start transfer bytes from the TX circular buffer using DMA */

  CODE void (*dmasend)(FAR struct uart_dev_s *dev);
#endif

#ifdef CONFIG_SERIAL_RXDMA
  /* Start transfer bytes from the TX circular buffer using DMA */

  CODE void (*dmareceive)(FAR struct uart_dev_s *dev);

  /* Notify DMA that there is free space in the RX buffer */

  CODE void (*dmarxfree)(FAR struct uart_dev_s *dev);
#endif

#ifdef CONFIG_SERIAL_TXDMA
  /* Notify DMA that there is data to be transferred in the TX buffer */

  CODE void (*dmatxavail)(FAR struct uart_dev_s *dev);
#endif

  /* This method will send one byte on the UART */

  CODE void (*send)(FAR struct uart_dev_s *dev, int ch);

  /* Call to enable or disable TX interrupts */

  CODE void (*txint)(FAR struct uart_dev_s *dev, bool enable);

  /* Return true if the tranmsit hardware is ready to send another byte.
   * This is used to determine if send() method can be called.
   */

  CODE bool (*txready)(FAR struct uart_dev_s *dev);

  /* Return true if all characters have been sent.  If for example, the UART
   * hardware implements FIFOs, then this would mean the transmit FIFO is
   * empty.  This method is called when the driver needs to make sure that
   * all characters are "drained" from the TX hardware.
   */

  CODE bool (*txempty)(FAR struct uart_dev_s *dev);

  /* Call to release some resource about the device when device was close
   * and unregistered.
   */

  CODE int (*release)(FAR struct uart_dev_s *dev);

  /* Receive multiple bytes.
   * Returns the actual number of characters received.
   */

  CODE ssize_t (*recvbuf)(FAR struct uart_dev_s *dev,
                          FAR void *buf, size_t len);

  /* This method will send multiple bytes.
   * Returns the actual number of characters sent.
   */

  CODE ssize_t (*sendbuf)(FAR struct uart_dev_s *dev,
                          FAR const void *buf, size_t len);
};

/* This structure is used for U(S)ART frame, overrun, parity and brk error
 * counters. This way applications will know if the UART communition is
 * having some trouble.
 */

struct serial_icounter_s
{
  uint32_t frame;
  uint32_t overrun;
  uint32_t parity;
  uint32_t brk;
  uint32_t buf_overrun;
};

/* This is the device structure used by the driver.  The caller of
 * uart_register() must allocate and initialize this structure.  The
 * calling logic need only set all fields to zero except:
 *
 *   'isconsole', 'xmit.buffer', 'rcv.buffer', the elements
 *   of 'ops', and 'private'
 *
 * The common logic will initialize all semaphores.
 */

struct uart_dev_s
{
  /* State data */

  uint8_t              open_count;   /* Number of times the device has been opened */
  uint8_t              escape;       /* Number of the character to be escaped */
#ifdef CONFIG_SERIAL_REMOVABLE
  volatile bool        disconnected; /* true: Removable device is not connected */
#endif
  bool                 isconsole;    /* true: This is the serial console */
  bool                 unlinked;     /* true: This device driver has been unlinked. */

#if defined(CONFIG_TTY_SIGINT) || defined(CONFIG_TTY_SIGTSTP) || \
    defined(CONFIG_TTY_FORCE_PANIC) || defined(CONFIG_TTY_LAUNCH)
  pid_t                pid;          /* Thread PID to receive signals (-1 if none) */
#endif

#ifdef CONFIG_TTY_FORCE_PANIC
  int                  panic_count;
#endif

  /* Terminal control flags */

  tcflag_t             tc_iflag;     /* Input modes */
  tcflag_t             tc_oflag;     /* Output modes */
  tcflag_t             tc_lflag;     /* Local modes */

  /* Semaphores & mutex */

  sem_t                xmitsem;      /* Wakeup user waiting for space in xmit.buffer */
  sem_t                recvsem;      /* Wakeup user waiting for data in recv.buffer */
  mutex_t              closelock;    /* Locks out new open while close is in progress */

  /* I/O buffers */

  struct uart_buffer_s xmit;         /* Describes transmit buffer */
  struct uart_buffer_s recv;         /* Describes receive buffer */

  /* DMA transfers */

#ifdef CONFIG_SERIAL_TXDMA
  struct uart_dmaxfer_s dmatx;       /* Describes transmit DMA transfer */
#endif
#ifdef CONFIG_SERIAL_RXDMA
  struct uart_dmaxfer_s dmarx;       /* Describes receive DMA transfer */
#endif

  /* Driver interface */

  FAR const struct uart_ops_s *ops;  /* Arch-specific operations */
  FAR void                    *priv; /* Used by the arch-specific logic */

  /* The following is a list if poll structures of threads waiting for
   * driver events. The 'struct pollfd' reference for each open is also
   * retained in the f_priv field of the 'struct file'.
   */

#ifdef CONFIG_SERIAL_TERMIOS
  uint8_t minrecv;                   /* Minimum received bytes */
  uint8_t minread;                   /* c_cc[VMIN] */
  uint8_t timeout;                   /* c_cc[VTIME] */
#endif

  FAR struct pollfd *fds[CONFIG_SERIAL_NPOLLWAITERS];
};

typedef struct uart_dev_s uart_dev_t;

/****************************************************************************
 * Public Data
 ****************************************************************************/

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: uart_register
 *
 * Description:
 *   Register serial console and serial ports.
 *
 ****************************************************************************/

int uart_register(FAR const char *path, FAR uart_dev_t *dev);

/****************************************************************************
 * Name: uart_xmitchars
 *
 * Description:
 *  This function is called from the UART interrupt handler when an interrupt
 *  is received indicating that there is more space in the transmit FIFO.
 *  This function will send characters from the tail of the xmit buffer while
 *  the driver write() logic adds data to the head of the xmit buffer.
 *
 ****************************************************************************/

void uart_xmitchars(FAR uart_dev_t *dev);

/****************************************************************************
 * Name: uart_recvchars
 *
 * Description:
 *  This function is called from the UART interrupt handler when an interrupt
 *  is received indicating that are bytes available to be received.  This
 *  function will add chars to head of receive buffer.  Driver read() logic
 *  will take characters from the tail of the buffer.
 *
 ****************************************************************************/

void uart_recvchars(FAR uart_dev_t *dev);

/****************************************************************************
 * Name: uart_datareceived
 *
 * Description:
 *  This function is called from uart_recvchars when new serial data is place
 *  in the driver's circular buffer.  This function will wake-up any stalled
 *  read() operations that are waiting for incoming data.
 *
 ****************************************************************************/

void uart_datareceived(FAR uart_dev_t *dev);

/****************************************************************************
 * Name: uart_datasent
 *
 * Description:
 *  This function is called from uart_xmitchars after serial data has been
 *  sent, freeing up some space in the driver's circular buffer.
 *  This function will wake-up any stalled write() operations that was
 *  waiting for space to buffer outgoing data.
 *
 ****************************************************************************/

void uart_datasent(FAR uart_dev_t *dev);

/****************************************************************************
 * Name: uart_connected
 *
 * Description:
 *  Serial devices (like USB serial) can be removed. In that case, the "upper
 *  half" serial driver must be informed that there is no longer a valid
 *  serial channel associated with the driver.
 *
 *  In this case, the driver will terminate all pending transfers wint
 *  ENOTCONN and will refuse all further transactions while the "lower half"
 *  is disconnected.
 *  The driver will continue to be registered, but will be in an unusable
 *  state.
 *
 *  Conversely, the "upper half" serial driver needs to know when the
 *  serial device is reconnected so that it can resume normal operations.
 *
 * Assumptions/Limitations:
 *  This function may be called from an interrupt handler.
 *
 ****************************************************************************/

#ifdef CONFIG_SERIAL_REMOVABLE
void uart_connected(FAR uart_dev_t *dev, bool connected);
#endif

/****************************************************************************
 * Name: uart_xmitchars_dma
 *
 * Description:
 *  Set up to transfer bytes from the TX circular buffer using DMA
 *
 ****************************************************************************/

#ifdef CONFIG_SERIAL_TXDMA
void uart_xmitchars_dma(FAR uart_dev_t *dev);
#endif

/****************************************************************************
 * Name: uart_xmitchars_done
 *
 * Description:
 *  Perform operations necessary at the complete of DMA including adjusting
 *  the TX circular buffer indices and waking up of any threads that may
 *  have been waiting for space to become available in the TX circular
 *  buffer.
 *
 ****************************************************************************/

#ifdef CONFIG_SERIAL_TXDMA
void uart_xmitchars_done(FAR uart_dev_t *dev);
#endif

/****************************************************************************
 * Name: uart_recvchars_dma
 *
 * Description:
 *  Set up to receive bytes into the RX circular buffer using DMA
 *
 ****************************************************************************/

#ifdef CONFIG_SERIAL_RXDMA
void uart_recvchars_dma(FAR uart_dev_t *dev);
#endif

/****************************************************************************
 * Name: uart_recvchars_done
 *
 * Description:
 *  Perform operations necessary at the complete of DMA including adjusting
 *  the RX circular buffer indices and waking up of any threads that may have
 *  been waiting for new data to become available in the RX circular buffer.
 *
 ****************************************************************************/

#ifdef CONFIG_SERIAL_RXDMA
void uart_recvchars_done(FAR uart_dev_t *dev);
#endif

/****************************************************************************
 * Name: uart_reset_sem
 *
 * Description:
 *  This function is called when need reset uart semaphore, this may used in
 *  kill one process, but this process was reading/writing with the
 *  semaphore.
 *
 ****************************************************************************/

void uart_reset_sem(FAR uart_dev_t *dev);

/****************************************************************************
 * Name: uart_check_special
 *
 * Description:
 *   Check if the SIGINT or SIGTSTP character is in the contiguous Rx DMA
 *   buffer region.  The first signal associated with the first such
 *   character is returned.
 *
 *   If there multiple such characters in the buffer, only the signal
 *   associated with the first is returned (this a bug!)
 *
 * Returned Value:
 *   0 if a signal-related character does not appear in the.  Otherwise,
 *   SIGKILL or SIGTSTP may be returned to indicate the appropriate signal
 *   action.
 *
 ****************************************************************************/

#if defined(CONFIG_TTY_SIGINT) || defined(CONFIG_TTY_SIGTSTP) || \
    defined(CONFIG_TTY_FORCE_PANIC) || defined(CONFIG_TTY_LAUNCH)
int uart_check_special(FAR uart_dev_t *dev, FAR const char *buf,
                       size_t size);
#endif

/****************************************************************************
 * Name: uart_gdbstub_register
 *
 * Description:
 *   Use the uart device to register gdbstub.
 *   gdbstub run with serial interrupt.
 *
 ****************************************************************************/

#ifdef CONFIG_SERIAL_GDBSTUB
int uart_gdbstub_register(FAR uart_dev_t *dev, FAR const char *path);
#endif

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __INCLUDE_NUTTX_SERIAL_SERIAL_H */
