/*++
Copyright (c) Integrated Device Technology, Inc.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

File Name:

    master.cpp

Description:

    This is a master component of simple program intended for testing basic RapidIO
    data transfers between two TSI721 devices directly connected via SRIO link.

    This program demonstrates how to use IDT TSI721 API routines for provided Windows
    device driver.
   
--*/

#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <stdio.h>
#include <process.h>
#include <conio.h> // for _getch()

#include "tsi721api.h"
#include "master.h"


#define DMA_BUF_SIZE (2 * 1024 * 1024)

#define RIO_DEV_ID_CAR              (0x000000)
#define RIO_BASE_ID_CSR             (0x000060)
#define RIO_COMPONENT_TAG_CSR       (0x00006C)

#define RIO_PORT_GEN_CTRL_CSR       (0x00013C)
#define RIO_PORT_N_ERR_STAT_CSR     (0x000158)

#define TSI721_DEV_ID                0x80ab0038

#define PAGE_SIZE 0x1000    // memory page size (x86)

#define MAINT_THR_NUM   10  // number of threads doing maintenance requests
#define MAINT_THR_LOOP  10  // loop counter for maintenance rd/wr threads

#define DATA_THR_NUM    5   // number of threads doing Data Rd/Wr requests
#define DATA_THR_LOOP   10  // loop counter for Data Rd/Wr threads

#define IMSG_BUF_NUM 32     // Number of pending Inbound message buffers (per MBOX)

typedef struct _EVB_THREAD_PARAM {
    HANDLE hDev;
    DWORD  DestId;  // Destination ID of the SRIO target device
    PVOID  DataBuf;
    DWORD  DataSize;
    DWORD  Id;      // ID assigned to a new thread
} EVB_THREAD_PARAM, *PEVB_THREAD_PARAM;

static VOID maint_rd_thread(PVOID Params);
static VOID data_rw_thread(PVOID Params);
static VOID tsi721_msg_send(HANDLE hDev, DWORD  dwDestId, DWORD  dwMbox, DWORD  msgCount);

HANDLE hEvent = NULL;
EVB_THREAD_PARAM evbThreadParam[MAINT_THR_NUM + DATA_THR_NUM];
volatile BOOL bQuitThread = FALSE;

int main(int argc, char* argv[])
{
    PVOID  obBuf = NULL; // outbound data buffer
    PVOID  ibBuf = NULL; // inbound data buffer
    HANDLE hDev;
    DWORD  devNum = 0;
    DWORD  destId = 0; // arbitrary value (different from one assigned to the target)
    DWORD  partnDestId, dwRegVal;
    DWORD  dwDataSize;
    DMA_REQ_CTRL dmaCtrl;
    int rnum;
    DWORD  i, dwErr, pass, repeat = 1;

    if (argc == 1) {
        printf_s("Missing Tsi721 device index\n");
        printf_s("Usage:\n");
        printf_s("   master <dev_idx> [local_destID [repeat]]\n");
        return 0;
    }

    if (argc > 1)
        devNum = atoi(argv[1]);
    if (argc > 2)
        destId = atoi(argv[2]);
    if (argc > 3)
        repeat = atoi(argv[3]);

    //
    // Pause to allow user to start the target.
    //
    printf_s("\nPlease make sure that attached test target is started\n");
    printf_s("Press any key to continue ....\n");
#pragma warning(suppress: 6031)
    _getch();

    if (!TSI721DeviceOpen(&hDev, devNum, NULL)) {
        printf_s("(%d) Unable to open device Tsi721_%d\n", __LINE__, devNum);
        return 0;
    }

    obBuf = malloc(DMA_BUF_SIZE);
    ibBuf = malloc(DMA_BUF_SIZE);

    if ((obBuf == NULL) || (ibBuf == NULL)) {
        printf_s("(%d) Unable to allocate test data buffer(s)\n", __LINE__);
        goto exit;
    }

    TSI721PciCfgRead(hDev, 0x10, &dwRegVal);
    printf_s("Opened Tsi721_%d (BAR0=0x%08x)\n", devNum, dwRegVal);

    ZeroMemory(obBuf, DMA_BUF_SIZE);
    ZeroMemory(ibBuf, DMA_BUF_SIZE);

    // Assuming small SRIO system size (address) configuration)
    destId = destId & 0xff;

    // Set SRIO destID assigned to Tsi721
    dwErr = TSI721SetLocalHostId(hDev, destId);
    if (dwErr != ERROR_SUCCESS) {
        printf_s("(%d) Set Local Host ID failed, err = 0x%x\n", __LINE__, dwErr);
        goto exit;
    }

    //
    // Check if SRIO port link is OK
    //
    dwErr = TSI721RegisterRead(hDev, RIO_PORT_N_ERR_STAT_CSR, 1, &dwRegVal);

    if (dwErr != ERROR_SUCCESS) {
        printf_s("(%d) Read port status failed, err = 0x%x\n", __LINE__, dwErr);
        goto exit;
    }

    if (dwRegVal & 0x00010100) {
        printf_s("Port is in error stopped state, status = 0x%08x\n", dwRegVal);
        printf_s("Please reset the board to run this test ...\n");
        goto exit;
    }

    if (dwRegVal & 0x02)
        printf_s("Port status OK, Local SRIO Destination ID = %d\n", destId);
    else {
        printf_s("(%d) Port link status is not OK, status = 0x%08x\n", __LINE__, dwRegVal);
        goto exit;
    }

    //
    // Check attached link partner device
    //

    // Read device ID register

    dwErr = TSI721SrioMaintRead(hDev, 0, 0, RIO_DEV_ID_CAR, &dwRegVal);

    if (dwErr != ERROR_SUCCESS) {
        printf_s("(%d) Failed to read partner device ID, err = 0x%x\n", __LINE__, dwErr);

        // Check if SRIO port link is OK
        dwErr = TSI721RegisterRead(hDev, RIO_PORT_N_ERR_STAT_CSR, 1, &dwRegVal);

        if (dwErr != ERROR_SUCCESS) {
            printf_s("(%d) Read port status failed, err = 0x%x\n", __LINE__, dwErr);
        } else {
            printf_s("SRIO Port status = 0x%08x\n", dwRegVal);
        }

        goto exit;
    }

    // Read partner device destID register

    dwErr = TSI721SrioMaintRead(hDev, 0, 0, RIO_BASE_ID_CSR, &partnDestId);

    if (dwErr != ERROR_SUCCESS) {
        printf_s("(%d) Failed to read partner destID, err = 0x%x\n", __LINE__, dwErr);
        goto exit;
    }

    // Assuming small SRIO system size (address) configuration)
    partnDestId = (partnDestId >> 16) & 0xff;

    printf_s("Tsi721 attached to device 0x%08x (destID=%d)\n", dwRegVal, partnDestId);

    dwErr = TSI721RegisterWrite(hDev, RIO_PORT_GEN_CTRL_CSR, 0xe0000000); // set HOST, MAST_EN and DISC bits

    for (pass = 1; pass <= repeat || repeat == 0; pass++) {

        if (repeat != 1) {
            printf_s("\nPass %d\n", pass);
            printf_s("Press Q to stop cyclic test\n");
        }

        //
        // Initialize write data
        //
        srand(_getpid() + pass);
        rnum = rand();

        for (i = 0; i < DMA_BUF_SIZE; i++)
            ((PUCHAR)obBuf)[i] = (UCHAR)(rnum + i);

        //
        // Perform data write operation (data will be written into inbound memory)
        //

        dwDataSize = DMA_BUF_SIZE/2;

        dmaCtrl.bits.Iof = 0;
        dmaCtrl.bits.Crf = 0;
        dmaCtrl.bits.Prio = 0;
        dmaCtrl.bits.Rtype = LAST_NWRITE_R; // Last packet NWRITE_R, all other NWRITE or SWRITE
        dmaCtrl.bits.XAddr = 0; // bits 65:64 of SRIO address

        printf_s("Writing %d bytes of data. Please wait ....\n", dwDataSize);
        fflush(stdout);

        dwErr = TSI721SrioWrite(hDev, partnDestId, 0, 0, obBuf, &dwDataSize, dmaCtrl);
        if (dwErr != ERROR_SUCCESS) {
            printf_s("(%d) SRIO_WR Failed, err = 0x%x\n", __LINE__, dwErr);
            goto exit;
        }

        // Read back into different buffer

        dwDataSize = DMA_BUF_SIZE/2;
        dmaCtrl.bits.Prio = 1;

        printf_s("Reading %d bytes of data. Please wait ....\n", dwDataSize);
        fflush(stdout);

        dwErr = TSI721SrioRead(hDev, partnDestId, 0, 0, ibBuf, &dwDataSize, dmaCtrl);
        if (dwErr != ERROR_SUCCESS) {
            printf_s("(%d) SRIO_RD Failed, err = 0x%x\n", __LINE__, dwErr);
            goto exit;
        }

        rnum = memcmp(obBuf, ibBuf, DMA_BUF_SIZE/2);

        if (rnum == 0)
            printf_s("Data transfer test completed successfully\n");
        else {
            printf_s("ERROR: Data transfer test failed\n");
            goto exit;
        }

        fflush(stdout);

        //
        // Test concurrent operations (maintenance and data transfer)
        //

        printf_s("Run multi-threaded test...\n");

        do {
            HANDLE hThread[MAINT_THR_NUM + DATA_THR_NUM];

            //
            // Create thread control event
            //
            hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            if (hEvent == NULL) {
                break;
            }

            for (i = 0; i < MAINT_THR_NUM; i++) {
                evbThreadParam[i].hDev = hDev;
                evbThreadParam[i].Id = i;
                evbThreadParam[i].DestId = partnDestId; 
                hThread[i] = (HANDLE)_beginthread(maint_rd_thread, 0, &evbThreadParam[i]);
                if ((HANDLE)-1L == hThread[i]) {
                    printf_s("Error starting thread %d\n", i);
                    break;
                }
            }

            // Continue filling the hTread array for data transfer threads (starting from MAINT_THR_NUM)
            for (; i < MAINT_THR_NUM + DATA_THR_NUM; i++) {
                evbThreadParam[i].hDev = hDev;
                evbThreadParam[i].Id = i;

                evbThreadParam[i].DestId = partnDestId; 
                evbThreadParam[i].DataBuf = obBuf;
                evbThreadParam[i].DataSize = 0x4000;

                hThread[i] = (HANDLE)_beginthread(data_rw_thread, 0, &evbThreadParam[i]);
                if ((HANDLE)-1L == hThread[i]) {
                    printf_s("Error starting thread %d\n", i);
                    break;
                }
            }

            // Start ALL pending threads 
            SetEvent(hEvent);

            dwErr = WaitForMultipleObjects(MAINT_THR_NUM + DATA_THR_NUM, hThread, TRUE, 60*1000);
            if (dwErr != WAIT_OBJECT_0) {
                bQuitThread = TRUE;
                printf_s("Thread Wait finished with 0x%x\n", dwErr);
                dwErr = WaitForMultipleObjects(MAINT_THR_NUM + DATA_THR_NUM, hThread, TRUE, 180*1000);
            }

            CloseHandle(hEvent);
        } while (0);

        //
        // Run message exchange test (MBOX0 only)
        //

        printf_s("Sending  messages to MBOX0 ...\n");
        tsi721_msg_send(hDev, partnDestId, 0, 10);

        Sleep(200);

        if (repeat != 1 && _kbhit()) {
            int ch;

            ch = _getch();
            ch = toupper(ch);
            if (ch == 'Q') {
                break;
            }
        }
    } // end of for()

    printf_s("\nTsi721 EVB Test finished.\n");

    //
    // The pause below allows user to test asynchronous notification events
    // generated by external sources (Port-Writes, inbound doorbells and messages to MBOX0) 
    //
    printf_s("\nPress any key to exit ....\n");
#pragma warning(suppress: 6031)
    _getch();

exit:

    TSI721DeviceClose(hDev, NULL);

    free(obBuf);
    free(ibBuf);

    return 0;
}

VOID
maint_rd_thread(
    PVOID Params
    )
/*++

Routine Description:

    Thread waiting for inbound doorbell event and doing callback into an app.

Arguments:

    params - pointer to tread parameter structure

Return Value:

    NONE

--*/
{
    HANDLE hDev = ((PEVB_THREAD_PARAM)Params)->hDev;
    DWORD  i, dwErr;
    DWORD  dwRegVal;
    DWORD  id = ((PEVB_THREAD_PARAM)Params)->Id;
    DWORD destId = ((PEVB_THREAD_PARAM)Params)->DestId;

    // Wait until start signal is given
    WaitForSingleObject(hEvent, INFINITE);

    for (i = 0; i < MAINT_THR_LOOP && !bQuitThread; i++) {

        dwRegVal = 0;

        // Read from device ID register of the attached CPS1432 switch
        dwErr = TSI721SrioMaintRead(hDev, 0, 0, RIO_DEV_ID_CAR, &dwRegVal);
        if (dwErr != ERROR_SUCCESS) {
            printf_s("MNT_THR_%d: Maint Read request %d failed with err=0x%08x\n", id, i, dwErr);
            break;
        }

        //
        // ATTN: This returned value check is specific for TSI721 Eval Card which has CPS1432 SRIO
        // switch attached to TSI721 SRIO port. If this test program is used with different hardware
        // this check has to be commented out or value has to be changed to reflect actual HW.
        //
        if (dwRegVal != TSI721_DEV_ID) {
            printf_s("MNT_THR_%d: Maint Read %d returned 0x%08x (expected 0x%08x)\n",
                     id, i, dwRegVal, TSI721_DEV_ID);
            break;
        }

        // Write to Component Tag register 
        dwErr = TSI721SrioMaintWrite(hDev, 0, 0, RIO_COMPONENT_TAG_CSR, 0xaabbccdd);
        if (dwErr != ERROR_SUCCESS) {
            printf_s("MNT_THR_%d: Maint Write request %d failed with err=0x%08x\n", id, i, dwErr);
            break;
        }
    }

    dwErr = TSI721SrioDoorbellSend(hDev, destId, 0xffff & id);
    if (dwErr) {
        printf_s( "MNT_THR_%d: Error TSI721SrioDoorbellSend(): 0x%x (%d)\n", id, dwErr, dwErr);
    }

    _endthread();
}

VOID
data_rw_thread(
    PVOID Params
    )
/*++

Routine Description:

    Thread waiting for inbound doorbell event and doing callback into an app.

Arguments:

    params - pointer to tread parameter structure

Return Value:

    NONE

--*/
{
    HANDLE hDev = ((PEVB_THREAD_PARAM)Params)->hDev;
    DWORD  i, loop, dwErr;
    PEVB_THREAD_PARAM thrParams = (PEVB_THREAD_PARAM)Params;
    DWORD id = ((PEVB_THREAD_PARAM)Params)->Id;
    DWORD destId = ((PEVB_THREAD_PARAM)Params)->DestId;
    DWORD dwDataSize;
    DMA_REQ_CTRL dmaCtrl;
    int rnum;
    PVOID  obBuf = NULL; // outbound data buffer
    PVOID  ibBuf = NULL; // inbound data buffer

    obBuf = malloc(((PEVB_THREAD_PARAM)Params)->DataSize);
    ibBuf = malloc(((PEVB_THREAD_PARAM)Params)->DataSize);

    if ((obBuf == NULL) || (ibBuf == NULL)) {
       printf_s("DATA_THR_%d: Failed to allocate data buffers\n", id);
       goto exit_err;
    }

    // Wait until start signal is given
    WaitForSingleObject(hEvent, INFINITE);

    for (loop = 0; loop < DATA_THR_LOOP && !bQuitThread; loop++) {

        // Initialize write data
        rnum = rand();
        dwDataSize = ((PEVB_THREAD_PARAM)Params)->DataSize;

        for (i = 0; i < dwDataSize; i++)
            ((PUCHAR)obBuf)[i] = (UCHAR)(rnum + i);

        dmaCtrl.bits.Iof = 0;
        dmaCtrl.bits.Crf = 0;
        dmaCtrl.bits.Prio = 0;
        dmaCtrl.bits.Rtype = 1; // Last packet NWRITE_R, all other NWRITE or SWRITE
        dmaCtrl.bits.XAddr = 0; // bits 65:64 of SRIO address

        dwErr = TSI721SrioWrite(hDev, destId, 0, 0, obBuf, &dwDataSize, dmaCtrl);
        if (dwErr != ERROR_SUCCESS) {
            printf_s("DATA_THR_%d: Data Write request %d failed with err=0x%08x\n", id, loop, dwErr);
            break;
        }
    }

exit_err:

    if (obBuf)
        free(obBuf);
    if (ibBuf)
        free(ibBuf);

    dwErr = TSI721SrioDoorbellSend(hDev, destId, 0xffff & id);
    if (dwErr) {
        printf_s( "DATA_THR_%d: Error TSI721SrioDoorbellSend(): 0x%x (%d)\n", id, dwErr, dwErr);
    }

    _endthread();
}

VOID
tsi721_msg_send(
    HANDLE hDev,
    DWORD  dwDestId,
    DWORD  dwMbox,
    DWORD  msgCount
    )
{
    OVERLAPPED ovl;
    DWORD dwError;
    PUCHAR msgBuf = NULL;
    DWORD dwBufLen;

    memset(&ovl, 0, sizeof(ovl));

    //
    // Create request completion event
    //
    ovl.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (ovl.hEvent == NULL) {
        printf_s( "ERR: failed to create request completion event\n");
        return;
    }

    //
    // Allocate single message buffer 4KB (has to be aligned to the page boundary)
    // NOTE: we will send messages shorter than allocated buffer but size can be increased
    // up to 4KB if required.
    //
    msgBuf = (PUCHAR)_aligned_malloc(0x1000, PAGE_SIZE);
    if (msgBuf == NULL) {
        printf_s("ERR: Unable to allocate aligned buffer for IB_MSG\n");
        goto err_exit;
    }

    while (msgCount--) {
        dwBufLen = 128;

        memset(msgBuf, msgCount, dwBufLen);
        dwError = TSI721SrioMsgSend(hDev, 0, dwDestId, msgBuf, &dwBufLen, &ovl);

        if (ERROR_IO_PENDING == dwError) {
            BOOL bResult = FALSE;

            WaitForSingleObject(ovl.hEvent, INFINITE);
            bResult = GetOverlappedResult(hDev, &ovl, &dwBufLen, FALSE);
            if (!bResult)
                dwError = GetLastError();
            else
                dwError = ERROR_SUCCESS;
        }
                    
        if (ERROR_SUCCESS != dwError) {
            printf_s("MSG_SEND: IOCTL error: 0x%x (%d) \n", dwError, dwError);
            break;
        }
    }

err_exit:

    if (msgBuf)
        _aligned_free(msgBuf);

    CloseHandle(ovl.hEvent);
}
