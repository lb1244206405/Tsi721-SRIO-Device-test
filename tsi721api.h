/*++
Copyright (c) Integrated Device Technology, Inc.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

File Name:

    tsi721api.h

Description:

    Contains Tsi721 driver API header information.

--*/

#ifndef _TSI721API_H_
#define _TSI721API_H_

#define INITGUID

#pragma warning(default:4201)

#include <initguid.h> // required for GUID definitions

DEFINE_GUID (GUID_TSI721_INTERFACE,
    0x73e7f717, 0xf608, 0x4f6c, 0x8d, 0x47, 0x20, 0xe8, 0xd, 0xb9, 0xba, 0x2a);

#define TSI721_BUFFER_SIZE      4096
#define TSI721_NUM_ASYNCH_IO    256
#define TSI721_SYNC_IO          0
#define TSI721_ASYNC_IO         1
#define TSI721_NWRITE
#define TSI721_WRITE_TYPE       0
#define TSI721_READ_TYPE        1

// Do not edit this value which will conflict on the Tsi721 Kernel device driver
#define TSI721_BDMA_MAINT_CH            0

#define TSI721_STATUS_INSUFFICIENT_RES  ((DWORD)0x000005AAL)
#define TSI721_RIO_PW_MSG_SIZE          16

#include <cfgmgr32.h>
#include "tsi721ioctl.h"

typedef struct _APP_THREAD_PARAM {
    HANDLE      hDevice;
    CRITICAL_SECTION criticalSection;
    WCHAR      *pDevPath;
    wchar_t     wDevPath[160];
    ULONG       ulDevNum;
    HANDLE      hIoctl;
    HANDLE      hWrite;
    HANDLE      hRead;
    ULONG       uIoType;
    BOOL        uOverapped; // must be 0, not supported by driver yet
    OVERLAPPED  ovlap;
    PUCHAR      pReadBuf;
    PUCHAR      pWriteBuf;
    ULONG       readBufSize;
    ULONG       writeBufSize;
} APP_THREAD_PARAM, * PAPP_THREAD_PARAM;

#define DB_INFO_MAX_BUF     64
#define DB_INFO_SIZE        8
#define DB_MAX_CHNUM        8
#define DMA_MAX_CHNUM       8
#define IBWIN_MAX_CHNUM     8
#define MSG_MAX_CHNUM       8
#define RIO_MSG_MAX_MBOX    4
#define IBMSG_MAX_CHNUM     8
#define OBMSG_MAX_CHNUM     8
#define IBDB_RING_SZ        512 /* Refer to Tsi721.h and it should be kept the same value */

typedef union _IB_DB_ENTRY {
    struct {
        USHORT SrcId;
        USHORT Info;
        USHORT Misc;
        USHORT DstId;
    } db;
    ULONG   raw[2];
} IB_DB_ENTRY, *PIB_DB_ENTRY;

typedef union _PW_MSG {
    struct {
        ULONG CompTag;
        ULONG PortErrDet;
        ULONG PortId;
        ULONG LTErrDet;
        ULONG Cap[TSI721_RIO_PW_MSG_SIZE/sizeof(ULONG)];
    } em;
    ULONG raw[4 + TSI721_RIO_PW_MSG_SIZE/sizeof(ULONG)];
} PW_MSG, *PPW_MSG;

typedef struct _PW_BUF_INFO {
    ULONG  RdPtr;
    ULONG  WrPtr;
    ULONG  PendingPW;
} PW_BUF_INFO, *PPW_BUF_INFO;

typedef struct _P2R_WINCFG {
    DWORD   AddrHi;   // Upper part of physical address of P2R window
    DWORD   AddrLo;   // Lower part of physical address of P2R window
    DWORD   Size;     // Size of window in bytes (32KB - 16GB)
    BOOL    Enable;   // Enable flag
} P2R_WINCFG, *PP2R_WINCFG;

typedef enum _SRIO_RD_TYPE {
    NRead   = 0x01,
    MntRead = 0x02,
} SRIO_RD_TYPE;

typedef enum _SRIO_WR_TYPE {
    NWrite   = 0x01,
    MntWrite = 0x02,
    NWrite_R = 0x04,
} SRIO_WR_TYPE;

typedef struct _P2R_ZONECFG {
    DWORD        TAddrHi;    // Upper part of zone translation address
    DWORD        TAddrLo;    // Lower part of zone translation address
    BYTE         TAddrEx;    // Bits [65:64] of 66-bit translation address
    SRIO_RD_TYPE RdType;
    SRIO_WR_TYPE WrType;
    BYTE         HopCnt;     // Hop Count to device (used by SRIO maintenance transactions)
    WORD         DestId;     // Target device destination ID
    BOOL         LrgId;      // device destID size flag (if TRUE 16-bit destID)
    BOOL         RdCrf;
    BOOL         WrCrf;
} P2R_ZONECFG, *PP2R_ZONECFG;

typedef struct _R2P_WINCFG {
    DWORD   BAddrHi;   // Upper part of base address of R2P window
    DWORD   BAddrLo;   // Lower part of base address of R2P window
    BYTE    BAddrEx;   // Bits [65:64] of 66-bit base address
    DWORD   TAddrHi;   // Upper part of inbound translation address
    DWORD   TAddrLo;   // Lower part of inbound translation address
    DWORD   Size;      // Size of window in bytes (32KB - 16GB)
} R2P_WINCFG, *PR2P_WINCFG;

typedef struct _PCI_BARCFG {
    DWORD   BAddrLo;   // Lower part of base address
    DWORD   BAddrHi;   // Upper part of base address (or 0)
    DWORD   Setup;     // Contents of corresponding BAR_SETUP register
} PCI_BARCFG, *PPCI_BARCFG;

typedef struct _DMA_CFG {
    DWORD   StartChNum; // start number of available BDMA channel
    DWORD   TotalChNum; // total number of available BDMA channels
    DWORD   ChMap;      // bitmap of BDMA channel reservation (1 - free, 0 - reserved)
} DMA_CFG, *PDMA_CFG;

#ifndef _TSI721API_
    #undef	EXPORTLINKAGE
    #define	EXPORTLINKAGE __declspec(dllimport)
#else
    #undef	EXPORTLINKAGE
    #define	EXPORTLINKAGE __declspec(dllexport)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * TSI721DeviceOpen()
 *
 * Opens a TSI721 device.
 *
 * Arguments:
 *  hDevice - pointer to variable to save returned device handle 
 *  ulDevNum - sequential number of Tsi721 device to open (0 ... n)
 *  pAppThreadParam - pointer to application thread parameter structure.
                      Currently this parameter must be NULL.
 *
 * Return Value:
 *  TRUE if the specified Tsi721 device was opened successfully.
 * 
 */
EXPORTLINKAGE
BOOL
TSI721DeviceOpen(
    __out    PHANDLE           hDevice,
    __in    ULONG             ulDevNum,
    __inout APP_THREAD_PARAM *pAppThreadParam
    );

/*
 * TSI721DeviceClose()
 *
 * Closes device specified by a handle.
 *
 * Arguments:
 *  hDev - device handle
 *  pAppThreadParam - pointer to application thread parameter structure.
                      Currently this parameter must be NULL.
 *
 * Return Value:
 *  TRUE if the specified Tsi721 device was opened successfully.
 */
EXPORTLINKAGE
DWORD
TSI721DeviceClose(
    __in HANDLE hDev,
    __inout APP_THREAD_PARAM *pAppThreadParam
    );

/*
 * TSI721RegisterRead()
 *
 *   Reads specified number of 32-bit values from the memory mapped registers
 *   in the device Control and Status Registers (CSRs) space trough PCIe memory
 *   read cycles. Address of the register to be read has to be specified as an
 *   offset from the beginning of the CSR space.
 *
 * Arguments:
 *  hDev     - device handle
 *  dwOffset - offset of the register from the beginning CSR space
 *  dwNum    - number of 32-bit registers to read starting from offset
 *  pRegVal  - pointer to the destination buffer for the read result
 *
 * Return Value:
 *  ERROR_SUCCESS - if a specified register was read successfully,
 *                  otherwise value returned by GetLastError().
 */
EXPORTLINKAGE
DWORD
TSI721RegisterRead(
    __in  HANDLE hDev,
    __in  DWORD  dwOffset,
    __in  DWORD  dwNum,
    __out PDWORD pRegVal
    );

/*
 * TSI721RegisterWrite()
 *
 *  Writes the specified 32-bit value into the register in the device Control
 *  and Status Registers (CSR) space trough PCIe memory write cycles.
 *  Address of the register has to be specified as an offset from the beginning
 *  of the CSR space.
 *
 * Arguments:
 *  hDev     - device handle
 *  dwOffset - offset of the register from the beginning of the CSR space
 *  dwValue  - a value to be written into a register
 *
 * Return Value:
 *  ERROR_SUCCESS - if a value was written successfully,
 *                   otherwise value returned by GetLastError().
 */
EXPORTLINKAGE
DWORD
TSI721RegisterWrite(
    __in HANDLE hDev,
    __in DWORD  dwOffset,
    __in DWORD  dwValue
    );

/*
 * TSI721PciCfgRead()
 *
 *  Reads 32-bit value from Tsi721 register in the PCI Express configuration space.
 *
 * Arguments:
 *  hDev     - device handle
 *  dwOffset - offset of the register from the beginning of PCIe Config space
 *  pRegVal  - pointer to the destination buffer for the read result
 *
 * Return Value:
 *  ERROR_SUCCESS - if a specified register was read successfully,
 *                  otherwise value returned by GetLastError().
 */
EXPORTLINKAGE
DWORD
TSI721PciCfgRead(
    __in  HANDLE hDev,
    __in  DWORD  dwOffset,
    __out PDWORD pRegVal
    );

/*
 * TSI721PciCfgWrite()
 *
 *  Writes the specified 32-bit value into the register in the PCIExpress
 *  configuration space.
 *
 * Arguments:
 *  hDev     - device handle
 *  dwOffset - offset of the register from the beginning of the PCIe Config space
 *  dwValue  - a value to be written into a register
 *
 * Return Value:
 *  ERROR_SUCCESS - if a value was written successfully,
 *                  otherwise value returned by GetLastError().
 */
EXPORTLINKAGE
DWORD
TSI721PciCfgWrite(
    __in HANDLE hDev,
    __in DWORD  dwOffset,
    __in DWORD  dwValue
    );

/*
 * TSI721GetLocalHostId()
 *
 *   Get the base RapidIO destination ID assigned to the specified Tsi721 device.
 *
 * Arguments:
 *  hDev     - device handle
 *  dwHostId - pointer to store retrieved destination ID
 *
 * Return Value:
 *  ERROR_SUCCESS - if the request was completed successfully,
 *                  otherwise value returned by GetLastError().
 */
EXPORTLINKAGE
DWORD
TSI721GetLocalHostId(
    __in  HANDLE hDev,
    __out PDWORD dwHostId
    );

/*
 * TSI721SetLocalHostId()
 *
 *   Set the base RapidIO destination ID assigned for the specified Tsi721 device.
 *
 * Arguments:
 *  hDev     - device handle
 *  dwHostId - destination ID to be set
 *
 * Return Value:
 *  ERROR_SUCCESS - if the request was completed successfully,
 *                  otherwise value returned by GetLastError().
 */
EXPORTLINKAGE
DWORD
TSI721SetLocalHostId(
    __in HANDLE hDev,
    __in DWORD  dwHostId
    );

/*
 * TSI721SrioMaintRead()
 *
 *  Generates a 32-bit SRIO Maintenance Read request to the specified
 *  target RapidIO device.
 *  NOTE: This routine is serviced by TSI721 BDMA Channel reserved for
 *        generating RapidIO Maintenance requests.
 *
 * Arguments:
 *  hDev     - device handle
 *  dwDestId - destID of target SRIO device
 *  dwHopCnt - number of hops to target SRIO device
 *  dwOffset - register offset in the target device
 *  pData  - pointer to a buffer where to place data returned by read request
 *
 * Return Value:
 *  ERROR_SUCCESS - if a request was completed successfully,
 *                  otherwise value returned by GetLastError().
 */
EXPORTLINKAGE
DWORD
TSI721SrioMaintRead(
    __in  HANDLE hDev,
    __in  DWORD  dwDestId,
    __in  DWORD  dwHopCnt,
    __in  DWORD  dwOffset,
    __out PDWORD pData
    );

/*
 * TSI721SrioMaintWrite()
 *
 * Generates a 32-bit SRIO Maintenance Write request to the specified
 * target RapidIO device.
 * NOTE: This routine is serviced by TSI721 BDMA Channel reserved for
 *       generating RapidIO Maintenance requests (currently by BDMA CH0).
 *
 * Arguments:
 *  hDev     - device handle
 *  dwDestId - destID of target SRIO device
 *  dwHopCnt - number of hops to target SRIO device
 *  dwOffset - register offset in the target device
 *  dwValue  - value to be written into the register
 *
 * Return Value:
 *  ERROR_SUCCESS - if a specified request was completed successfully,
 *                   otherwise value returned by GetLastError().
 */
EXPORTLINKAGE
DWORD
TSI721SrioMaintWrite(
    __in HANDLE hDev,
    __in DWORD  dwDestId,
    __in DWORD  dwHopCnt,
    __in DWORD  dwOffset,
    __in DWORD  dwValue
    );

/*
 * TSI721SrioWrite()
 *
 *  Writes data from a user mode source buffer to a destination address on a specified
 *  target SRIO device using TSI721 BDMA engine. Uses NWRITE, NWRITE_R and SWRITE RapidIO
 *  data transfer requests as it is specified for BDMA Engine in TSI721 User Manual.
 *
 * Arguments:
 *  hDev       - device handle
 *  dwDestId   - destID of target SRIO device
 *  dwAddrHi   - bits 63:32 of 66-bit SRIO starting address in the target device
 *  dwAddrLo   - bits 31:00 of 66-bit SRIO starting address in the target device
 *               (NOTE: bits 65:64 of 66-bit SRIO address are part of DMA_REQ_CTRL
 *                data structure)
 *  pBuffer    - pointer to a source data buffer.
 *  pdwBufSize - points to a size variable of the source data buffer.
 *  dwCtrl     - DMA request specific control word (defined in tsi721_ioctl.h)
 * 
 * Return Value:
 *  ERROR_SUCCESS - if a specified data block was written successfully,
 *                  otherwise value returned by GetLastError().
 *                  Buffer size variable will be updated with actual number of bytes transferred.
 */
EXPORTLINKAGE
DWORD
TSI721SrioWrite(
    __in    HANDLE       hDev,
    __in    DWORD        dwDestId,
    __in    DWORD        dwAddrHi,
    __in    DWORD        dwAddrLo,
    __in    PVOID        pBuffer,
    __inout PDWORD       pdwBufSize,
    __in    DMA_REQ_CTRL dwCtrl
    );

/*
 * TSI721SrioRead()
 *
 *  Reads data from a source address at the specified RapidIO target device and 
 *  places them into application's destination buffer using TSI721 BDMA engine.
 *  Uses NREAD RapidIO data transfer requests as it is specified for BDMA Engine
 *  in TSI721 User Manual.
 *
 * Arguments:
 *  hDev       - device handle
 *  dwDestId   - destID of target SRIO device
 *  dwAddrHi   - bits 63:32 of 66-bit starting address in the target device
 *  dwAddrLo   - bits 31:00 of 66-bit starting address in the target device
 *               (NOTE: bits 65:64 of 66-bit SRIO address are part of DMA_REQ_CTRL
 *                data structure)
 *  pBuffer    - pointer to a destination data buffer.
 *  pdwBufSize - points to a size variable of the destination data buffer.
 *  dwCtrl     - DMA request specific control word
 *
 * Return Value:
 *  ERROR_SUCCESS - if a specified data block was read successfully,
 *                  otherwise value returned by GetLastError().
 *                  Buffer size variable will be updated with actual number of bytes transferred.
 */
EXPORTLINKAGE
DWORD
TSI721SrioRead(
    __in    HANDLE       hDev,
    __in    DWORD        dwDestId,
    __in    DWORD        dwAddrHi,
    __in    DWORD        dwAddrLo,
    __out   PVOID        pBuffer,
    __inout PDWORD       pdwBufSize,
    __in    DMA_REQ_CTRL dwCtrl
    );

/*
 * TSI721CfgR2pWin()
 *
 *  Creates a shared memory buffer that both the driver and a TSI721 PCIe bus
 *  master (inbound mapping engine SR2PC) can access simultaneously.
 *  Initializes mapping path using specified TSI721 Inbound Mapping Window IBWIN.
 *  NOTES:
 *   1) Specified IBWIN should be available for mapping (not in use), otherwise
 *   an error will be reported.
 *   2) Specified window size must be equal to 2^N in range 4KB - 16GB, otherwise
 *   an error will be reported.
 *   3) A caller should be careful when requesting large size for inbound buffer,
 *   if an error is returned the caller may try to request a smaller size mapping.
 *
 * Arguments:
 *  hDev     - device handle
 *  dwWinNum - number of IB window to configure (0-7)
 *  pWinCfg  - pointer to structure with window configuration data
 *
 * Return Value:
 *  ERROR_SUCCESS - if the request was completed successfully,
 *                  otherwise value returned by GetLastError().
 */
EXPORTLINKAGE
DWORD
TSI721CfgR2pWin(
    __in HANDLE      hDev,
    __in DWORD       bWinNum,
    __in PR2P_WINCFG pWinCfg
    );

/*
 * TSI721FreeR2pWin()
 *
 *  Instructs the device driver to free specified inbound mapping window
 *  and a shared memory buffer associated with it.
 *
 * Arguments:
 *  hDev     - device handle
 *  dwWinNum - number of IB window to free (0-7)
 *
 * Return Value:
 *  ERROR_SUCCESS - if the request was completed successfully,
 *                  otherwise value returned by GetLastError().
 */
EXPORTLINKAGE
DWORD
TSI721FreeR2pWin(
    __in HANDLE      hDev,
    __in DWORD       bWinNum
    );

/*
 * TSI721IbwBufferGet()
 *
 *  Reads data from the driver’s internal kernel mode buffer allocated for
 *  data exchange using inbound RapidIO NREAD/NWRITE requests (see TSI721CfgR2pWin).
 *  Expected to be used together with doorbell notification.
 *
 * Arguments:
 *  hDev       - device handle
 *  dwMapNum   - inbound mapping window number (0 - 7)
 *  dwOffset   - offset within driver's inbound buffer
 *  pBuffer    - pointer to an application's receiving data buffer.
 *  pdwBufSize - points to a size variable of the receiving data buffer.
 *                The size is initially assigned to the maximum user buffer size.
 *
 * Return Value:
 *  ERROR_SUCCESS - if a specified data block was read successfully,
 *                  otherwise value returned by GetLastError().
 *                  Buffer size variable will be updated with actual number of bytes transferred.
 */
EXPORTLINKAGE
DWORD
TSI721IbwBufferGet(
    __in  HANDLE  hDev,
    __in  DWORD   dwMapNum,
    __in  DWORD   dwOffset,
    __out PVOID   pBuffer,
    __inout PDWORD  pdwBufSize
    );

/*
 * TSI721IbwBufferPut()
 *
 *  Places data into the driver’s internal kernel mode buffer allocated for
 *  data exchange using inbound RapidIO NREAD/NWRITE requests (see TSI721CfgR2pWin).
 *  Expected to be used together with doorbell notification.
 *
 * Arguments:
 *  hDev       - device handle
 *  dwMapNum   - inbound mapping window number (0 - 7)
 *  dwOffset   - offset within driver's inbound buffer
 *  pBuffer    - pointer to an application's source data buffer.
 *  pdwBufSize - points to a size variable of the source data buffer.
 *               The size is initially assigned to the maximum user buffer size.
 *
 * Return Value:
 *  ERROR_SUCCESS - if a specified data block was read successfully,
 *                  otherwise value returned by GetLastError().
 *                  Buffer size variable will be updated with actual number of bytes transferred.
 */
EXPORTLINKAGE
DWORD
TSI721IbwBufferPut(
    __in  HANDLE  hDev,
    __in  DWORD   dwMapNum,
    __in  DWORD   dwOffset,
    __in  PVOID   pBuffer,
    __in PDWORD  pdwBufSize
    );

/*
 * TSI721SrioDoorbellSend()
 *
 * Sends SRIO Doorbell packet to the specified target device.
 *
 * Arguments:
 *  hDev     - device handle
 *  dwDestId - destID of target SRIO device
 *  dwInfo   - SRIO doorbell INFO field (lower 16 bits)
 *              CRF : 1 bit  bit[31]
 *
 * Return Value:
 *  ERROR_SUCCESS - if a doorbell packet was sent successfully,
 *                  otherwise value returned by GetLastError().
 */
EXPORTLINKAGE
DWORD
TSI721SrioDoorbellSend(
    __in HANDLE hDev,
    __in DWORD  dwDestId,
    __in DWORD  dwInfo
    );

/*
 * TSI721SrioIbDoorbellWait()
 *
 *  Sends notification request to the device driver to wait and notify when
 *  inbound doorbell(s) is (are) received by SRIO controller. This is an asynchronous
 *  request which may be pending for long time (until DB arrives).
 *  To perform asynchronous processing of pending notification a caller must provide
 *  OVERLAPPED data structure.
 *
 * Arguments:
 *  hDev          - device handle
 *  pDbBuf        - buffer to store received doorbell messages 
 *  dwBufSize     - size of the buffer
 *  lpBytesReturned - actual size of returned data (valid only if returned ERROR_SUCCESS) 
 *  lpOvl     - pointer to OVERLAPPED structure
 *
 * Return Value:
 *  ERROR_SUCCESS - if doorbell message was available immediately
 *  ERROR_INVALID_PARAMETER - if one of the input parameters is invalid
 *  ERROR_IO_PENDING - caller must wait for asynchronous completion of IOCTL request.
 *  Other values as returned by GetLastError():
 */
EXPORTLINKAGE
DWORD
TSI721SrioIbDoorbellWait(
    __in  HANDLE hDev,
    __out PVOID pDbBuf,
    __in  DWORD dwBufSize,
    __out LPDWORD lpBytesReturned,
    __inout LPOVERLAPPED lpOvl
    );

/*
 * TSI721SrioDoorbellCheck()
 *
 * Checks for available inbound doorbell messages.
 *
 * Arguments:
 *  hDev       - device handle
 *  pDbNum     - number of doorbell messages in a queue for the given channel.
 *
 * Return Value:
 *  ERROR_SUCCESS - if a specified request was completed successfully,
 *                  otherwise value returned by GetLastError().
 */
EXPORTLINKAGE
DWORD
TSI721SrioDoorbellCheck(
    __in  HANDLE hDev,
    __out PDWORD pDbNum
    );

/*
 * TSI721SrioDoorbellGet()
 *
 * Fetches inbound doorbell messages received by device.
 *
 * Arguments:
 *  hDev     - device handle
 *  pIbDbBuf - destination buffer for retrieved doorbell messages
 *  pBufSize - pointer to size of destination buffer (updated to real size of data on exit)
 *
 * Return Value:
 *  ERROR_SUCCESS - if a specified request was completed successfully,
 *  ERROR_INVALID_PARAMETER - if one of input parameters is invalid,
 *  value returned by GetLastError() for all other cases.
 */
EXPORTLINKAGE
DWORD
TSI721SrioDoorbellGet(
    __in  HANDLE       hDev,
    __out PVOID        pIbDbBuf,
    __in  PDWORD       pBufSize
    );

/*
 * TSI721SrioPortWriteWait()
 *
 * Sends notification request to the device driver to wait and notify when
 * port-write message(s) is (are) received by SRIO controller.
 * This is an asynchronous request which may be pending for long time (until DB arrives).
 * To perform asynchronous processing of pending notification a caller must provide
 * OVERLAPPED data structure.
 *
 * Arguments:
 *  hDev      - device handle
 *  pDbBuf    - buffer to store received port-write message(s)
 *  dwBufSize - size of the buffer
 *  lpBytesReturned - actual size of returned data (valid only if returned ERROR_SUCCESS) 
 *  lpOvl     - pointer to OVERLAPPED structure
 *
 * Return Value:
 *  ERROR_SUCCESS - if port-write message was available immediately
 *  ERROR_INVALID_PARAMETER - if one of the input parameters is invalid
 *  ERROR_IO_PENDING - caller must wait for asynchronous completion of IOCTL request.
 *  Other values as returned by GetLastError():
 */
EXPORTLINKAGE
DWORD
TSI721SrioPortWriteWait(
    __in HANDLE hDev,
    __out PVOID pPwBuf,
    __in DWORD dwBufSize,
    __out LPDWORD lpBytesReturned,
    __inout LPOVERLAPPED lpOvl
    );

/*
 * TSI721PortWriteEnable()
 *
 * Enables/disables inbound port-write notification.
 *
 * Arguments:
 *  hDev     - device handle
 *  bEnable  - operation flag (TRUE = enable, FALSE = disable)
 *
 * Return Value:
 *  ERROR_SUCCESS - if a specified request was completed successfully,
 *  ERROR_INVALID_PARAMETER - if one of input parameters is invalid,
 *  value returned by GetLastError() for all other cases.
 */
EXPORTLINKAGE
DWORD
TSI721PortWriteEnable(
    __in HANDLE hDev,
    __in BOOL   bEnable
    );

/*
 * TSI721SrioMsgSend()
 *
 * Sends an SRIO message to the specified MBOX on target device.
 *
 * Arguments:
 *  hDev       - device handle
 *  dwMbox     - SRIO messaging mailbox (MBOX = 0...4)
 *  dwDestId   - destID of target SRIO device
 *  pBuffer    - pointer to a message data buffer.
 *  pdwBufSize - points to a size variable of the message data buffer.
 *  lpOvl      - pointer to OVERLAPPED structure
 *
 * Return Value:
 *  ERROR_SUCCESS - if an SRIO message was sent successfully,
 *                  otherwise value returned by GetLastError().
 *  ERROR_INVALID_PARAMETER - if one of input parameters is invalid,
 *  ERROR_IO_PENDING - caller must wait for asynchronous completion of IOCTL request.
 *  value returned by GetLastError() for all other cases.
 */
EXPORTLINKAGE
DWORD
TSI721SrioMsgSend(
    __in    HANDLE       hDev,
    __in    DWORD        dwMbox,
    __in    DWORD        dwDestId,
    __in    PVOID        pBuffer,
    __inout PDWORD       pdwBufSize,
    __inout LPOVERLAPPED lpOvl
    );

/*
 * TSI721SrioMsgAddRcvBuffer()
 *
 * Add buffer for inbound SRIO message to the specified MBOX on target device.
 *
 * Arguments:
 *  hDev       - device handle
 *  dwMbox     - SRIO messaging mailbox (MBOX = 0...4)
 *  pBuffer    - pointer to a message data buffer.
 *  pdwBufSize - points to a size variable of the message data buffer.
 *  lpOvl      - pointer to OVERLAPPED structure
 *
 * Return Value:
 *  ERROR_INVALID_PARAMETER - if one of input parameters is invalid,
 *  ERROR_IO_PENDING - caller must wait for asynchronous completion of IOCTL request.
 *  value returned by GetLastError() for all other cases.
 */
EXPORTLINKAGE
DWORD
TSI721SrioMsgAddRcvBuffer(
    __in    HANDLE       hDev,
    __in    DWORD        dwMbox,
    __in    PVOID        pBuffer,
    __inout PDWORD       pdwBufSize,
    __inout LPOVERLAPPED lpOvl
    );

/*
 * TSI721SrioIbMsgDevIdSet()
 *
 *  Set Device ID for Inbound Message Engine.
 *
 * Arguments:
 *  hDev      - device handle
 *  dwIbMsgDevId - Device ID for Inbound Message DMA Engine
 *                 (valid device ID is 16-bit value 0 - 0xffff)
 *
 * Return Value:
 *  ERROR_SUCCESS - if the request was completed successfully,
 *                  otherwise value returned by GetLastError().
 */
EXPORTLINKAGE
DWORD
TSI721SrioIbMsgDevIdSet(
    __in HANDLE hDev,
    __in DWORD  dwIbMsgDevId
    );

/*
 * TSI721SrioIbMsgDevIdGet()
 *
 *  Return Device ID for Inbound Message Engine.
 *
 * Arguments:
 *  hDev      - device handle
 *  dwIbMsgId - pointer to store retrieved device ID
 *
 * Return Value:
 *  ERROR_SUCCESS - if the request was completed successfully,
 *                  otherwise value returned by GetLastError().
 */
EXPORTLINKAGE
DWORD
TSI721SrioIbMsgDevIdGet(
    __in  HANDLE hDev,
    __out PDWORD dwIbMsgDevId
    );

#ifdef __cplusplus
}
#endif

#endif // _TSI721API_H_