/*++
Copyright (c) Integrated Device Technology, Inc.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

File Name:

    tsi721ioctl.h

Description:

    Contains definitions of proprietary IOCTL codes supported by
    the Tsi721 driver for Windows

--*/

#ifndef _TSI721IOCTL_H_
#define _TSI721IOCTL_H_

enum DMA_RTYPE {
    NREAD = 0,
    LAST_NWRITE_R = 1,
    ALL_NWRITE = 2,
    ALL_NWRITE_R = 3,
    MAINT_RD = 4,
    MAINT_WR = 5
};

typedef union _DMA_REQ_CTRL {
    struct {
        ULONG        Prio     : 2 ; // SRIO request priority
        ULONG        XAddr    : 2 ; // Most significant bits [65:64] of 66-bit RapidIO address
        ULONG        Crf      : 1 ; // SRIO critical request flow flag
        ULONG        Iof      : 1 ; // (ignored by driver, will be removed)
        ULONG        Rtype    : 4 ; // DMA RType
        ULONG        reserved : 19 ;
    } bits;
    ULONG dword;
} DMA_REQ_CTRL, * PDMA_REQ_CTRL;

///////////////////////////////////////////////////////////////////////////////
//
//                 Driver-specific IOCTL codes
//
//////////////////////////////////////////////////////////////////////////////

// NOTE: in[] and out[] arrays shown in comment for an IOCTL code represent
// input and output parameters (UINT32) for that request.

//
// BASIC REGISTER ACCESS:
//

// REG_READ : read from TSI721 memory mapped register(s)
//
// in[0] - register offset in TSI721 PCIe memory mapped register space
// out[0 ... n] - values read from register block (number of registers to read is 
//                defined by size of request output buffer)
#define IOCTL_TSI_REG_READ \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x880, METHOD_BUFFERED, FILE_ANY_ACCESS)

// REG_WRITE : write to TSI721 memory mapped register
//
// in[0] - register offset in TSI721 PCIe memory mapped register space
// in[1] - 32-bit value to write into register
//
#define IOCTL_TSI_REG_WRITE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x881, METHOD_BUFFERED, FILE_WRITE_ACCESS)

// MNT_READ : read form a register of target RapidIO device using RapidIO MAINTENANCE
// request
//
// in[0] - destination ID of a target RapidIO device
// in[1] - Hop Count to the target RapidIO device (0xff for endpoints)
// in[2] - register offset
// out[0] - 32-bit values read from the target RapidIO device
//
#define IOCTL_TSI_MNT_READ \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x882, METHOD_BUFFERED, FILE_ANY_ACCESS)

// MNT_WRITE : write to a register of target RapidIO device using RapidIO MAINTENANCE
// request
//
// in[0] - destination ID of a target RapidIO device
// in[1] - Hop Count to the target RapidIO device (0xff for endpoints)
// in[2] - register offset
// in[3] - 32-bit value to write
//
#define IOCTL_TSI_MNT_WRITE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x883, METHOD_BUFFERED, FILE_WRITE_ACCESS)

// PCFG_READ : read form a TSI721 register in PCIe Config space
//
// in[0] - register offset in TSI721 PCIe memory mapped register space
// out[0] - 32-bit values read from TSI721 register in PCIe Config space
//
#define IOCTL_TSI_PCFG_READ \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x884, METHOD_BUFFERED, FILE_ANY_ACCESS)

// PCFG_WRITE : write to a TSI721 register in PCIe Config space
//
// in[0] - register offset in TSI721 PCIe Config space
// in[1] - 32-bit value to write into register
//
#define IOCTL_TSI_PCFG_WRITE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x885, METHOD_BUFFERED, FILE_WRITE_ACCESS)

//
// DATA TRANSFER: 
//

// SRIO_WRITE : send a data block to a target RapidIO device using BDMA Engine
//
// in[0] - destination ID of a target RapidIO device
// in[1] - bits 63:32 of 66-bit starting address in the target device
// in[2] - bits 31:00 of 66-bit starting address in the target device
// in[3] - DMA request-specific control word (see DMA_REQ_CTRL structure above)
//
// This IOCTL sends data directly from a user mode buffer specified as an output
// buffer for DeviceIoControl() function.
//
#define IOCTL_TSI_SRIO_WRITE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x890, METHOD_IN_DIRECT, FILE_ANY_ACCESS)

// SRIO_READ : read a data block from a target RapidIO device using BDMA engine
//
// in[0] - destination ID of a target RapidIO device
// in[1] - bits 63:32 of 66-bit starting address in the target device
// in[2] - bits 31:00 of 66-bit starting address in the target device
// in[3] - DMA request-specific control word (see DMA_REQ_CTRL structure above)
//
// This IOCTL places data directly into a user mode buffer specified as an output
// buffer for DeviceIoControl() function.
//
#define IOCTL_TSI_SRIO_READ \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x891, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)

// IBW_BUFFER_GET : returns data from the driver's internal kernel mode buffer
// allocated for data exchange using inbound RapidIO NREAD/NWRITE requests
// initiated by an external device (using Tsi721 PCIe bus mastering capability).
//
// in[0] - inbound mapping (SR2PC) window number (0 - 7) associated with buffer
// in[1] - start offset of data block within a data buffer
//
// This IOCTL places data directly into a user mode buffer specified as an output
// buffer for DeviceIoControl() function.
//
#define IOCTL_TSI_IBW_BUFFER_GET \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x892, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)

// IBW_BUFFER_PUT : places a data block into the driver's internal kernel mode buffer
// allocated for data exchange using inbound RapidIO NREAD/NWRITE requests
// initiated by an external device (using Tsi721 PCIe bus mastering capability).
//
// in[0] - inbound mapping (SR2PC) window number (0 - 7) associated with buffer
// in[1] - start offset of data block within a data buffer
//
// This IOCTL sends data directly from a user mode buffer specified as an output
// buffer for DeviceIoControl() function.
//
#define IOCTL_TSI_IBW_BUFFER_PUT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x893, METHOD_IN_DIRECT, FILE_ANY_ACCESS)

//
// DOORBELL OPERATIONS:
//

// DB_SEND : sends a doorbell notification to another RapidIO device
//
// in[0] - destination ID of a target RapidIO device
// in[1] - SRIO doorbell INFO field (lower 16 bits) combined with CRF bit (bit 31)
//
#define IOCTL_TSI_DB_SEND \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8a0, METHOD_BUFFERED, FILE_WRITE_ACCESS)

// DB_WAIT : wait for doorbell notification from an external device.
//
// in[0] - inbound doorbell queue number (must be 0 at this moment)
// out[0 ... n] - doorbell messages placed into output buffer
//
#define IOCTL_TSI_DB_WAIT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8a1, METHOD_BUFFERED, FILE_ANY_ACCESS)

// DB_CHECK : checks for pending inbound doorbell notification(s)
//
// in[0] - inbound doorbell queue number (must be 0 at this moment)
// out[0] - number of doorbell messages in the queue
//
#define IOCTL_TSI_DB_CHECK \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8a2, METHOD_BUFFERED, FILE_ANY_ACCESS)

// DB_GET : fetches queued inbound doorbell notification
//
// in[0] - inbound doorbell queue number (must be 0 at this moment)
// out[0 ... n] - doorbell messages placed into output buffer
//
#define IOCTL_TSI_DB_GET \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8a3, METHOD_BUFFERED, FILE_ANY_ACCESS)

//
// MESSAGING OPERATIONS:
//

// MSG_SEND : sends a message to the specified RapidIO device
//
// in[0] - RapidIO messaging mailbox number (0 ... 3)
// in[1] - destination ID of a target RapidIO device
//
// This IOCTL sends data directly from a user mode buffer specified as an output
// buffer for DeviceIoControl() function.
//
#define IOCTL_TSI_MSG_SEND \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8b0, METHOD_IN_DIRECT, FILE_ANY_ACCESS)

// IB_MSG_DEV_ID_SET : set Inbound Messaging host id for the RapidIO controller
//
// in[0] - IB_MSG destID for a local RapidIO controller (tsi721)
//         (valid device ID: 16-bit value 0 - 0xffff)
//
#define IOCTL_TSI_IB_MSG_DEV_ID_SET \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8b1, METHOD_BUFFERED, FILE_WRITE_ACCESS)

// IB_MSG_DEV_ID_GET : get an Inbound Messaging host id assigned to tsi721
//
// out[0] - IB_MSG destID assigned to a local RapidIO controller (tsi721)
//
#define IOCTL_TSI_IB_MSG_DEV_ID_GET \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8b2, METHOD_BUFFERED, FILE_READ_ACCESS)

// MSG_ADDBUF : add a buffer into an inbound message queue
//
// in[0] - RapidIO messaging mailbox number (0 ... 3)
//
// This IOCTL request places page aligned user mode buffer into driver's
// inbound message queue. When request returns data are directly placed into a user mode buffer
// (specified as an output buffer for DeviceIoControl() function).
//
#define IOCTL_TSI_MSG_ADDRXBUF \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8b3, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)

//
// PORT-WRITE OPERATIONS:
//

// PW_WAIT : wait for port-write notification from an external device.
//
// out[0 ... 3] - port-write message placed into output buffer
//
#define IOCTL_TSI_PW_WAIT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8c0, METHOD_BUFFERED, FILE_ANY_ACCESS)

// PW_ENABLE : enable or disable port-write event notification.
//
// in[0] - enable/disable operation flag (0 = DISABLE, non-zero = ENABLE)
//
#define IOCTL_TSI_PW_ENABLE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8c1, METHOD_BUFFERED, FILE_ANY_ACCESS)

//
// DEVICE CONFIGURATION:
//

// R2P_WIN_SET : Creates a shared memory buffer that both the driver and
// a TSI721 PCIe bus master (inbound mapping engine SR2PC) can access simultaneously.
// Initializes mapping path using specified TSI721 Inbound Mapping Window IBWIN.
//
// in[0] - inbound window number combined with SRIO Ex address bits 65:64 ((BAddrEx << 24) | WinNum);
// in[1] - bits 31:00 of 66-bit of IB window base address in the RapidIO space
// in[2] - bits 63:32 of 66-bit of IB window base address in the RapidIO space
// in[3] - IB window size (2^N = 4KB ... 16GB)
//
// NOTE: IB window base address must to be size-aligned 
//
#define IOCTL_TSI_R2P_WIN_SET \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8d2, METHOD_BUFFERED, FILE_WRITE_ACCESS)

// R2P_WIN_FREE : frees allocated SR2PC mapping window and common buffer
//
// in[0] - inbound mapping window number
//
#define IOCTL_TSI_R2P_WIN_FREE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8d3, METHOD_BUFFERED, FILE_WRITE_ACCESS)

// SET_LOCAL_HOST_ID : set a local host id for the RapidIO controller
//
// in[0] - destination ID of a local RapidIO controller (tsi721)
//
#define IOCTL_TSI_SET_LOCAL_HOST_ID \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8f2, METHOD_BUFFERED, FILE_WRITE_ACCESS)

// GET_LOCAL_HOST_ID : get a local host id from the local host
//
// out[0] - destination ID assigned to a local RapidIO controller (tsi721)
//
#define IOCTL_TSI_GET_LOCAL_HOST_ID \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8f3, METHOD_BUFFERED, FILE_READ_ACCESS)

#endif // _TSI721IOCTL_H_
