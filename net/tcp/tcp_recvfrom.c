/****************************************************************************
 * net/tcp/tcp_recvfrom.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#ifdef CONFIG_NET_TCP

#include <string.h>
#include <errno.h>
#include <debug.h>
#include <assert.h>

#include <nuttx/semaphore.h>
#include <nuttx/tls.h>
#include <nuttx/net/net.h>
#include <nuttx/mm/iob.h>
#include <nuttx/net/netdev.h>
#include <nuttx/net/ip.h>
#include <nuttx/net/tcp.h>

#include "netdev/netdev.h"
#include "devif/devif.h"
#include "tcp/tcp.h"
#include "socket/socket.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct tcp_recvfrom_s
{
  FAR struct tcp_conn_s   *ir_conn;      /* Connection associated with the socket */
  FAR struct devif_callback_s *ir_cb;    /* Reference to callback instance */
  sem_t                    ir_sem;       /* Semaphore signals recv completion */
  size_t                   ir_buflen;    /* Length of receive buffer */
  uint8_t                 *ir_buffer;    /* Pointer to receive buffer */
  FAR struct sockaddr     *ir_from;      /* Address of sender */
  FAR socklen_t           *ir_fromlen;   /* Number of bytes allocated for address of sender */
  ssize_t                  ir_recvlen;   /* The received length */
  int                      ir_result;    /* Success:OK, failure:negated errno */
  int                      ir_flags;     /* Flags on received message.  */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: tcp_update_recvlen
 *
 * Description:
 *   Update information about space available for new data and update size
 *   of data in buffer.
 *
 * Input Parameters:
 *   pstate   recvfrom state structure
 *   recvlen  size of new data appended to buffer
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

static inline void tcp_update_recvlen(FAR struct tcp_recvfrom_s *pstate,
                                      size_t recvlen)
{
  if (pstate->ir_recvlen < 0)
    {
      pstate->ir_recvlen = 0;
    }

  pstate->ir_recvlen += recvlen;
  pstate->ir_buffer  += recvlen;
  pstate->ir_buflen  -= recvlen;
}

/****************************************************************************
 * Name: tcp_recvfrom_newdata
 *
 * Description:
 *   Copy the read data from the packet
 *
 * Input Parameters:
 *   dev      The structure of the network driver that generated the event.
 *   pstate   recvfrom state structure
 *
 * Returned Value:
 *   The number of bytes taken from the packet.
 *
 * Assumptions:
 *   The network is locked.
 *
 ****************************************************************************/

static size_t tcp_recvfrom_newdata(FAR struct net_driver_s *dev,
                                   FAR struct tcp_recvfrom_s *pstate)
{
  unsigned int offset;
  size_t recvlen;

  /* Get the length of the data to return */

  if (dev->d_len > pstate->ir_buflen)
    {
      recvlen = pstate->ir_buflen;
    }
  else
    {
      recvlen = dev->d_len;
    }

  /* Copy the new appdata into the user buffer */

  offset = (dev->d_appdata - dev->d_iob->io_data) - dev->d_iob->io_offset;

  recvlen = iob_copyout(pstate->ir_buffer, dev->d_iob, recvlen, offset);

  /* Trim the copied buffers */

  dev->d_iob = iob_trimhead(dev->d_iob,
      pstate->ir_flags & MSG_PEEK ? offset : recvlen + offset);

  ninfo("Received %d bytes (of %d)\n", (int)recvlen, (int)dev->d_len);

  /* Update the accumulated size of the data read */

  tcp_update_recvlen(pstate, recvlen);

  return pstate->ir_flags & MSG_PEEK ? 0: recvlen;
}

/****************************************************************************
 * Name: tcp_newdata
 *
 * Description:
 *   Copy the read data from the packet
 *
 * Input Parameters:
 *   dev      The structure of the network driver that generated the event
 *   pstate   recvfrom state structure
 *
 * Returned Value:
 *   None.
 *
 * Assumptions:
 *   The network is locked.
 *
 ****************************************************************************/

static inline uint16_t tcp_newdata(FAR struct net_driver_s *dev,
                                   FAR struct tcp_recvfrom_s *pstate,
                                   uint16_t flags)
{
  FAR struct tcp_conn_s *conn = pstate->ir_conn;

  /* Take as much data from the packet as we can */

  size_t recvlen = tcp_recvfrom_newdata(dev, pstate);

  /* If there is more data left in the packet that we could not buffer, then
   * add it to the read-ahead buffers.
   */

  if (recvlen < dev->d_len)
    {
      uint16_t nsaved;
      uint16_t buflen = dev->d_len - recvlen;

      nsaved = tcp_datahandler(dev, conn, 0);
      if (nsaved < buflen)
        {
          nwarn("WARNING: packet data not fully saved "
                "(%d/%u/%zu/%u bytes)\n",
                buflen - nsaved,
                (unsigned int)nsaved,
                recvlen,
                (unsigned int)dev->d_len);
        }

      recvlen += nsaved;
    }
  else
    {
      netdev_iob_release(dev);
    }

  if (recvlen < dev->d_len)
    {
      /* Clear the TCP_CLOSE because we effectively dropped the FIN as well.
       */

      flags &= ~TCP_CLOSE;
    }

  net_incr32(conn->rcvseq, recvlen);

  /* Indicate no data in the buffer */

  dev->d_len = 0;

  return flags;
}

/****************************************************************************
 * Name: tcp_readahead
 *
 * Description:
 *   Copy the read-ahead data from the packet
 *
 * Input Parameters:
 *   pstate   recvfrom state structure
 *
 * Returned Value:
 *   None
 *
 * Assumptions:
 *   The network is locked.
 *
 ****************************************************************************/

static inline void tcp_readahead(struct tcp_recvfrom_s *pstate)
{
  FAR struct tcp_conn_s *conn = pstate->ir_conn;
  FAR struct iob_s *iob;
  int recvlen;

  /* Check there is any TCP data already buffered in a read-ahead
   * buffer.
   */

  while ((iob = conn->readahead) != NULL &&
          pstate->ir_buflen > 0)
    {
      DEBUGASSERT(iob->io_pktlen > 0);

      /* Transfer that buffered data from the I/O buffer chain into
       * the user buffer.
       */

      recvlen = iob_copyout(pstate->ir_buffer, iob, pstate->ir_buflen, 0);
      ninfo("Received %d bytes (of %d)\n", recvlen, iob->io_pktlen);

      /* Update the accumulated size of the data read */

      tcp_update_recvlen(pstate, recvlen);

      /* If it is in read-ahead mode,
       * exit directly to avoid iob being released
       */

      if (pstate->ir_flags & MSG_PEEK)
        {
          break;
        }

      /* If we took all of the data from the I/O buffer chain is empty, then
       * release it.  If there is still data available in the I/O buffer
       * chain, then just trim the data that we have taken from the
       * beginning of the I/O buffer chain.
       */

      if (recvlen >= iob->io_pktlen)
        {
          /* Free free the I/O buffer chain */

          iob_free_chain(iob);
          conn->readahead = NULL;
        }
      else
        {
          /* The bytes that we have received from the head of the I/O
           * buffer chain.
           */

          conn->readahead = iob_trimhead(iob, recvlen);
        }
    }
}

/****************************************************************************
 * Name: tcp_sender
 *
 * Description:
 *   Getting the sender's address from the UDP packet
 *
 * Input Parameters:
 *   dev    - The device driver data structure
 *   pstate - the recvfrom state structure
 *
 * Returned Value:
 *   None
 *
 * Assumptions:
 *   The network is locked
 *
 ****************************************************************************/

static inline void tcp_sender(FAR struct net_driver_s *dev,
                              FAR struct tcp_recvfrom_s *pstate)
{
  /* Get the family from the packet type, IP address from the IP header, and
   * the port number from the TCP header.
   */

#ifdef CONFIG_NET_IPv6
#ifdef CONFIG_NET_IPv4
  if (IFF_IS_IPv6(dev->d_flags))
#endif
    {
      FAR struct sockaddr_in6 *infrom =
        (FAR struct sockaddr_in6 *)pstate->ir_from;

      if (infrom)
        {
          FAR struct tcp_hdr_s *tcp   = TCPIPv6BUF;
          FAR struct ipv6_hdr_s *ipv6 = IPv6BUF;

          infrom->sin6_family = AF_INET6;
          infrom->sin6_port   = tcp->srcport;

          net_ipv6addr_copy(infrom->sin6_addr.s6_addr, ipv6->srcipaddr);
        }
    }
#endif /* CONFIG_NET_IPv6 */

#ifdef CONFIG_NET_IPv4
#ifdef CONFIG_NET_IPv6
  else
#endif
    {
      FAR struct sockaddr_in *infrom  =
        (FAR struct sockaddr_in *)pstate->ir_from;

      if (infrom)
        {
          FAR struct tcp_hdr_s *tcp   = TCPIPv4BUF;
          FAR struct ipv4_hdr_s *ipv4 = IPv4BUF;

          infrom->sin_family = AF_INET;
          infrom->sin_port   = tcp->srcport;

          net_ipv4addr_copy(infrom->sin_addr.s_addr,
                            net_ip4addr_conv32(ipv4->srcipaddr));
          memset(infrom->sin_zero, 0, sizeof(infrom->sin_zero));
        }
    }
#endif /* CONFIG_NET_IPv4 */
}

/****************************************************************************
 * Name: tcp_recvhandler
 *
 * Description:
 *   This function is called with the network locked to perform the actual
 *   TCP receive operation via by the lower, device interfacing layer.
 *
 * Input Parameters:
 *   dev      The structure of the network driver that generated the event.
 *   pvpriv   An instance of struct tcp_recvfrom_s cast to void*
 *   flags    Set of events describing why the callback was invoked
 *
 * Returned Value:
 *   None
 *
 * Assumptions:
 *   The network is locked.
 *
 ****************************************************************************/

static uint16_t tcp_recvhandler(FAR struct net_driver_s *dev,
                                FAR void *pvpriv, uint16_t flags)
{
  FAR struct tcp_recvfrom_s *pstate = pvpriv;
  FAR struct iob_s *iob = NULL;

  ninfo("flags: %04x\n", flags);

  /* 'priv' might be null in some race conditions (?) */

  if (pstate)
    {
      /* If new data is available, then complete the read action. */

      if ((flags & TCP_NEWDATA) != 0)
        {
          /* Save the sender's address in the caller's 'from' location */

          tcp_sender(dev, pstate);

          if ((flags & TCP_ACKDATA) != 0)
            {
              iob = iob_tryalloc(false);
              if (iob == NULL)
                {
                  nerr("ERROR: IOB alloc failed !\n");
                  return flags;
                }

              iob_reserve(iob, CONFIG_NET_LL_GUARDSIZE);
              int ret = iob_clone_partial(dev->d_iob, dev->d_iob->io_pktlen,
                                          0, iob, 0, false, false);
              if (ret < 0)
                {
                  iob_free_chain(iob);
                  nerr("ERROR: IOB clone failed ret=%d!\n", ret);
                  return flags;
                }
            }

          /* Copy the data from the packet (saving any unused bytes from the
           * packet in the read-ahead buffer).
           */

          flags = tcp_newdata(dev, pstate, flags);

          if (iob != NULL)
            {
              netdev_iob_replace(dev, iob);
            }

          /* Indicate that the data has been consumed and that an ACK
           * should be sent.
           */

          flags = (flags & ~TCP_NEWDATA) | TCP_SNDACK;

          /* Check for transfer complete.  We will consider the
           * TCP/IP transfer complete as soon as any data has been received.
           * This is safe because if any additional data is received, it
           * will be retained in the TCP/IP read-ahead buffer until the
           * next receive is performed.
           */

          if ((pstate->ir_recvlen > 0 &&
               (pstate->ir_cb->flags & TCP_WAITALL) == 0) ||
              pstate->ir_buflen == 0)
            {
              ninfo("TCP resume\n");

              /* The TCP receive buffer is non-empty.  Return now and don't
               * allow any further TCP call backs.
               */

              pstate->ir_cb->flags   = 0;
              pstate->ir_cb->priv    = NULL;
              pstate->ir_cb->event   = NULL;

              /* Wake up the waiting thread, returning the number of bytes
               * actually read.
               */

              nxsem_post(&pstate->ir_sem);
            }
        }

      /* Check for a loss of connection.
       *
       * TCP_DISCONN_EVENTS:
       *   TCP_CLOSE:    The remote host has closed the connection
       *   TCP_ABORT:    The remote host has aborted the connection
       *   TCP_TIMEDOUT: Connection aborted due to too many retransmissions.
       *   NETDEV_DOWN:  The network device went down
       */

      else if ((flags & TCP_DISCONN_EVENTS) != 0)
        {
          FAR struct tcp_conn_s *conn = pstate->ir_conn;

          nwarn("WARNING: Lost connection\n");

          /* We could get here recursively through the callback actions of
           * tcp_lost_connection().  So don't repeat that action if we have
           * already been disconnected.
           */

          DEBUGASSERT(conn != NULL);
          if (_SS_ISCONNECTED(conn->sconn.s_flags))
            {
              /* Handle loss-of-connection event */

              tcp_lost_connection(conn, pstate->ir_cb, flags);
            }

          /* Check if the peer gracefully closed the connection. */

          if ((flags & TCP_CLOSE) != 0)
            {
              /* This case should always return success (zero)! The value of
               * ir_recvlen, if zero, will indicate that the connection was
               * gracefully closed.
               */

              pstate->ir_result = 0;
            }
          else
            {
              pstate->ir_result = -ENOTCONN;
            }

          /* Wake up the waiting thread */

          nxsem_post(&pstate->ir_sem);
        }
    }

  return flags;
}

/****************************************************************************
 * Name: tcp_recvfrom_initialize
 *
 * Description:
 *   Initialize the state structure
 *
 * Input Parameters:
 *   conn     The TCP connection of interest
 *   buf      Buffer to receive data
 *   len      Length of buffer
 *   pstate   A pointer to the state structure to be initialized
 *
 * Returned Value:
 *   None
 *
 * Assumptions:
 *
 ****************************************************************************/

static void tcp_recvfrom_initialize(FAR struct tcp_conn_s *conn,
                                    FAR void *buf, size_t len,
                                    FAR struct sockaddr *infrom,
                                    FAR socklen_t *fromlen,
                                    FAR struct tcp_recvfrom_s *pstate,
                                    int flags)
{
  /* Initialize the state structure. */

  memset(pstate, 0, sizeof(struct tcp_recvfrom_s));
  nxsem_init(&pstate->ir_sem, 0, 0); /* Doesn't really fail */

  pstate->ir_buflen    = len;
  pstate->ir_buffer    = buf;
  pstate->ir_from      = infrom;
  pstate->ir_fromlen   = fromlen;
  pstate->ir_flags     = flags;

  /* Set up the start time for the timeout */

  pstate->ir_conn      = conn;
}

/* The only un-initialization that has to be performed is destroying the
 * semaphore.
 */

#define tcp_recvfrom_uninitialize(s) nxsem_destroy(&(s)->ir_sem)

/****************************************************************************
 * Name: tcp_recvfrom_result
 *
 * Description:
 *   Evaluate the result of the recv operations
 *
 * Input Parameters:
 *   result   The result of the net_sem_timedwait operation
 *            (may indicate EINTR)
 *   pstate   A pointer to the state structure to be initialized
 *
 * Returned Value:
 *   The result of the recv operation with errno set appropriately
 *
 * Assumptions:
 *
 ****************************************************************************/

static ssize_t tcp_recvfrom_result(int result, struct tcp_recvfrom_s *pstate)
{
  /* Check if any data were received. If so, then return their length and
   * ignore any error codes.
   */

  if (pstate->ir_recvlen > 0)
    {
      return pstate->ir_recvlen;
    }

  /* If no data were received, return the error code instead. The event
   * handler error is prioritized over any previous error.
   */

  return (pstate->ir_result < 0) ? pstate->ir_result : result;
}

/****************************************************************************
 * Name: tcp_notify_recvcpu
 *
 * Description:
 *   This function will check current cpu id with conn->rcvcpu, if
 *   not same, then use netdev_notify_recvcpu to notify the new cpu id
 *
 * Input Parameters:
 *   conn    - The TCP connection of interest
 *
 * Returned Value:
 *   None
 *
 * Assumptions:
 *   conn is not NULL.
 *
 ****************************************************************************/

#ifdef CONFIG_NETDEV_RSS
static void tcp_notify_recvcpu(FAR struct tcp_conn_s *conn)
{
  int cpu = this_cpu();

  if (cpu != conn->rcvcpu)
    {
      conn->rcvcpu = cpu;

      if (conn->domain == PF_INET)
        {
          netdev_notify_recvcpu(conn->dev, cpu, conn->domain,
                                &(conn->u.ipv4.laddr), conn->lport,
                                &(conn->u.ipv4.raddr), conn->rport);
        }
      else
        {
          netdev_notify_recvcpu(conn->dev, cpu, conn->domain,
                                &(conn->u.ipv6.laddr), conn->lport,
                                &(conn->u.ipv6.raddr), conn->rport);
        }
    }
}
#else
#  define tcp_notify_recvcpu(c)
#endif /* CONFIG_NETDEV_RSS */

/****************************************************************************
 * Name: tcp_recvfrom_one
 *
 * Description:
 *   This function attempts to receive data from the specified TCP connection
 *   'conn' and stores it in the provided buffer 'buf'. It handles data that
 *   may already be in the read-ahead buffer and manages blocking or
 *   non-blocking operations based on connection flags.
 *   It also monitors connection state, handles connection timeouts, and
 *   updates the receive window when necessary.
 *
 * Input Parameters:
 *   conn    - The TCP connection from which data is to be received.
 *   buf     - The buffer to store the received data.
 *   len     - The maximum number of bytes to receive.
 *   from    - Socket address structure to store the source address
 *             (if provided).
 *   fromlen - Length of the address structure.
 *   flags   - Flags indicating specific receive options
 *             (e.g., non-blocking).
 *
 * Returned Value:
 *   Returns the number of bytes received, or a negative error code in
 *   case of failure (e.g., -ENOTCONN if not connected).
 *
 * Assumptions:
 *   conn, buf, and from are non-NULL pointers.
 *
 ****************************************************************************/

static ssize_t tcp_recvfrom_one(FAR struct tcp_conn_s *conn, FAR void *buf,
                                size_t len, FAR struct sockaddr *from,
                                FAR socklen_t *fromlen, int flags)
{
  struct tcp_recvfrom_s state;
  struct tcp_callback_s info;
  ssize_t ret;

  /* Initialize the state structure.  This is done with the network
   * locked because we don't want anything to happen until we are ready.
   */

  tcp_recvfrom_initialize(conn, buf, len, from, fromlen, &state, flags);

  /* Handle any any TCP data already buffered in a read-ahead buffer.
   * NOTE that there may be read-ahead data to be retrieved even after
   * the socket has been disconnected.
   */

  tcp_readahead(&state);

  /* The default return value is the number of bytes that we just copied
   * into the user buffer.  We will return this if the socket has become
   * disconnected or if the user request was completely satisfied with
   * data from the readahead buffers.
   */

  ret = state.ir_recvlen;

  /* Verify that the SOCK_STREAM has been and still is connected */

  if (!_SS_ISCONNECTED(conn->sconn.s_flags))
    {
      /* Was any data transferred from the readahead buffer after we were
       * disconnected?  If so, then return the number of bytes received.
       * We will wait to return end disconnection indications the next
       * time that recvfrom() is called.
       *
       * If no data was received (i.e.,  ret == 0  -- it will not be
       * negative) and the connection was gracefully closed by the remote
       * peer, then return success.  If ir_recvlen is zero, the caller of
       * recvfrom() will get an end-of-file indication.
       */

      if (ret <= 0 && !_SS_ISCLOSED(conn->sconn.s_flags))
        {
          /* Nothing was previously received from the read-ahead buffers.
           * The SOCK_STREAM must be (re-)connected in order to receive
           * any additional data.
           */

          ret = -ENOTCONN;
        }
    }

  /* In general, this implementation will not support non-blocking socket
   * operations... except in a few cases:  Here for TCP receive with
   * read-ahead enabled.  If this socket is configured as non-blocking
   * then return EAGAIN if no data was obtained from the read-ahead
   * buffers.
   */

  else if (_SS_ISNONBLOCK(conn->sconn.s_flags) ||
          (flags & MSG_DONTWAIT) != 0)
    {
      /* Return the number of bytes read from the read-ahead buffer if
       * something was received (already in 'ret'); EAGAIN if not.
       */

      if (ret <= 0)
        {
          /* Nothing was received */

          ret = -EAGAIN;
        }
    }

  /* It is okay to block if we need to.  If there is space to receive
   * anything more, then we will wait to receive the data.  Otherwise
   * return the number of bytes read from the read-ahead buffer
   * (already in 'ret').
   */

  else

  /* We get here when we we decide that we need to setup the wait for
   * incoming TCP/IP data.  Just a few more conditions to check:
   *
   * 1) Make sure that there is buffer space to receive additional data
   *    (state.ir_buflen > 0).  This could be zero, for example,  we
   *    filled the user buffer with data from the read-ahead buffers. And
   * 2) then we not want to wait if we already obtained some data from
   *    the read-ahead buffer.  In that case, return now with what we
   *    have (don't want for more because there may be no timeout).
   * 3) If however MSG_WAITALL flag is set, block here till all requested
   *    data are received (or there is a timeout / error).
   */

  if (((flags & MSG_WAITALL) != 0 || state.ir_recvlen == 0) &&
      state.ir_buflen > 0)
    {
      /* Set up the callback in the connection */

      state.ir_cb = tcp_callback_alloc(conn);
      if (state.ir_cb)
        {
          state.ir_cb->flags   = (TCP_NEWDATA | TCP_DISCONN_EVENTS);
          state.ir_cb->flags  |= (flags & MSG_WAITALL) ? TCP_WAITALL : 0;
          state.ir_cb->priv    = (FAR void *)&state;
          state.ir_cb->event   = tcp_recvhandler;

          /* Push a cancellation point onto the stack.  This will be
           * called if the thread is canceled.
           */

          info.tc_conn = conn;
          info.tc_cb   = state.ir_cb;
          info.tc_sem  = &state.ir_sem;
          tls_cleanup_push(tls_get_info(), tcp_callback_cleanup, &info);

          /* Wait for either the receive to complete or for an
           * error/timeout to occur.  net_sem_timedwait will also
           * terminate if a signal is received.
           */

          ret = net_sem_timedwait(&state.ir_sem,
                              _SO_TIMEOUT(conn->sconn.s_rcvtimeo));
          tls_cleanup_pop(tls_get_info(), 0);
          if (ret == -ETIMEDOUT)
            {
              ret = -EAGAIN;
            }

          /* Make sure that no further events are processed */

          tcp_callback_free(conn, state.ir_cb);
          ret = tcp_recvfrom_result(ret, &state);
        }
      else if (ret <= 0)
        {
          ret = -EBUSY;
        }
    }

  /* Receive additional data from read-ahead buffer, send the ACK timely.
   * Revisit: Because IOBs are system-wide resources, consuming the read
   * ahead buffer would update recv window of all connections in the
   * system, not only this particular connection.
   */

  if (tcp_should_send_recvwindow(conn))
    {
      netdev_txnotify_dev(conn->dev);
    }

  tcp_notify_recvcpu(conn);
  tcp_recvfrom_uninitialize(&state);
  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: psock_tcp_recvfrom
 *
 * Description:
 *   Perform the recvfrom operation for a TCP/IP SOCK_STREAM
 *
 * Input Parameters:
 *   psock    Pointer to the socket structure for the SOCK_DRAM socket
 *   msg      Receive info and buffer for receive data
 *   flags    Receive flags
 *
 * Returned Value:
 *   On success, returns the number of characters received.  On  error,
 *   -errno is returned (see recvfrom for list of errnos).
 *
 * Assumptions:
 *
 ****************************************************************************/

ssize_t psock_tcp_recvfrom(FAR struct socket *psock, FAR struct msghdr *msg,
                           int flags)
{
  FAR struct sockaddr   *from    = msg->msg_name;
  FAR socklen_t         *fromlen = &msg->msg_namelen;
  FAR struct tcp_conn_s *conn;
  ssize_t                nrecv   = 0;
  ssize_t                ret     = 0;
  int                    i;

  net_lock();

  conn = psock->s_conn;
  for (i = 0; i < msg->msg_iovlen; i++)
    {
      FAR void *buf = msg->msg_iov[i].iov_base;
      size_t len = msg->msg_iov[i].iov_len;

      ret = tcp_recvfrom_one(conn, buf, len, from, fromlen, flags);
      if (ret <= 0)
        {
          break;
        }

      nrecv += ret;

      /* User has not set MSG_WAITALL: If the first buffer is full
       * when received for the first time, then check the next buffer,
       * otherwise return the received data.
       * User sets MSG_WAITALL: Ensure that each iov is filled before
       * returning
       */

      if (!(flags & MSG_WAITALL) && ret < msg->msg_iov[i].iov_len)
        {
          break;
        }
    }

  net_unlock();
  return nrecv ? nrecv : ret;
}

#endif /* CONFIG_NET_TCP */
