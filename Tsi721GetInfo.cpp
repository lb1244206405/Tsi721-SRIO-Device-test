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
#include "Tsi721GetInfo.h"

#pragma comment(lib,"tsi721_api.lib")

#define DMA_BUF_SIZE (2 * 1024 * 1024)

#define RIO_DEV_ID_CAR              (0x000000)
#define RIO_BASE_ID_CSR             (0x000060)
#define RIO_COMPONENT_TAG_CSR       (0x00006C)

#define RIO_PORT_GEN_CTRL_CSR       (0x00013C)
#define RIO_PORT_N_ERR_STAT_CSR     (0x000158)


#define RIO_PE_FEAT                 (0x000010)
#define RIO_SR_XADDR                (0x00004c)
#define RIO_HOST_BASE_ID_LOCK       (0x000068)
#define RIO_SP_LM_REQ               (0x000140)
#define RIO_SP_LM_RESP              (0x000144)
#define RIO_SP_ACKID_STAT           (0x000148)
#define RIO_SP_CTL2                 (0x000154)


#define RIO_SP_ERR_DET              (0x001040)
#define RIO_SP_RATE_EN              (0x001044)


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
	DWORD  i, dwErr, pass, MODE, repeat = 1;

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
	if (argc > 4)
		MODE = atoi(argv[4]);
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
	destId = 22;
	// Set SRIO destID assigned to Tsi721
	dwErr = TSI721SetLocalHostId(hDev, destId);
	if (dwErr != ERROR_SUCCESS) {
		printf_s("(%d) Set Local Host ID failed, err = 0x%x\n", __LINE__, dwErr);
		goto exit;
	}
	//for (int i = 0; i < 2; i++) {
		/*******************************************************************/
		dwErr = TSI721RegisterRead(hDev, RIO_PE_FEAT, 1, &dwRegVal);

		if (dwErr != ERROR_SUCCESS) {
			printf_s("(%d) Read port status failed, err = 0x%x\n", __LINE__, dwErr);
			goto exit;
		}
		printf_s("LB: Register RIO_PE_FEAT status = 0x%08x\n", dwRegVal);

		dwErr = TSI721RegisterRead(hDev, RIO_SR_XADDR, 1, &dwRegVal);

		if (dwErr != ERROR_SUCCESS) {
			printf_s("(%d) Read port status failed, err = 0x%x\n", __LINE__, dwErr);
			goto exit;
		}
		printf_s("LB: Register RIO_SR_XADDR status = 0x%08x\n", dwRegVal);

		dwErr = TSI721RegisterRead(hDev, RIO_HOST_BASE_ID_LOCK, 1, &dwRegVal);

		if (dwErr != ERROR_SUCCESS) {
			printf_s("(%d) Read port status failed, err = 0x%x\n", __LINE__, dwErr);
			goto exit;
		}
		printf_s("LB: Register RIO_HOST_BASE_ID_LOCK status = 0x%08x\n", dwRegVal);

		dwErr = TSI721RegisterRead(hDev, RIO_SP_LM_REQ, 1, &dwRegVal);

		if (dwErr != ERROR_SUCCESS) {
			printf_s("(%d) Read port status failed, err = 0x%x\n", __LINE__, dwErr);
			goto exit;
		}
		printf_s("LB: Register RIO_SP_LM_REQ status = 0x%08x\n", dwRegVal);

		dwErr = TSI721RegisterRead(hDev, RIO_SP_LM_RESP, 1, &dwRegVal);

		if (dwErr != ERROR_SUCCESS) {
			printf_s("(%d) Read port status failed, err = 0x%x\n", __LINE__, dwErr);
			goto exit;
		}
		printf_s("LB: Register RIO_SP_LM_RESP status = 0x%08x\n", dwRegVal);

		dwErr = TSI721RegisterRead(hDev, RIO_SP_ACKID_STAT, 1, &dwRegVal);

		if (dwErr != ERROR_SUCCESS) {
			printf_s("(%d) Read port status failed, err = 0x%x\n", __LINE__, dwErr);
			goto exit;
		}
		printf_s("LB: Register RIO_SP_ACKID_STAT status = 0x%08x\n", dwRegVal);

		if (MODE == 1) {

			if (repeat == 1) {
				dwErr = TSI721RegisterWrite(hDev, RIO_SP_CTL2, 0x01000000);
				if (dwErr != ERROR_SUCCESS) {
					printf_s("(%d) write port status failed, err = 0x%x\n", __LINE__, dwErr);
					goto exit;
				}
			}
			if (repeat == 2) {
				dwErr = TSI721RegisterWrite(hDev, RIO_SP_CTL2, 0x00400000);
				if (dwErr != ERROR_SUCCESS) {
					printf_s("(%d) write port status failed, err = 0x%x\n", __LINE__, dwErr);
					goto exit;
				}
			}

			if (repeat == 3) {
				dwErr = TSI721RegisterWrite(hDev, RIO_SP_CTL2, 0x00100000);
				if (dwErr != ERROR_SUCCESS) {
					printf_s("(%d) write port status failed, err = 0x%x\n", __LINE__, dwErr);
					goto exit;
				}
			}
			if (repeat == 4) {
				dwErr = TSI721RegisterWrite(hDev, RIO_SP_CTL2, 0x00040000);
				if (dwErr != ERROR_SUCCESS) {
					printf_s("(%d) write port status failed, err = 0x%x\n", __LINE__, dwErr);
					goto exit;
				}
			}
		}
		dwErr = TSI721RegisterRead(hDev, RIO_SP_CTL2, 1, &dwRegVal);

		if (dwErr != ERROR_SUCCESS) {
			printf_s("(%d) Read port status failed, err = 0x%x\n", __LINE__, dwErr);
			goto exit;
		}
		printf_s("LB: Register RIO_SP_CTL2 status = 0x%08x\n", dwRegVal);

		dwErr = TSI721RegisterRead(hDev, RIO_PORT_N_ERR_STAT_CSR, 1, &dwRegVal);

		if (dwErr != ERROR_SUCCESS) {
			printf_s("(%d) Read port status failed, err = 0x%x\n", __LINE__, dwErr);
			goto exit;
		}
		printf_s("LB: Register RIO_PORT_N_ERR_STAT_CSR status = 0x%08x\n", dwRegVal);


		/**********************************************************************/

		dwErr = TSI721RegisterRead(hDev, RIO_SP_ERR_DET, 1, &dwRegVal);

		if (dwErr != ERROR_SUCCESS) {
			printf_s("(%d) Read port status failed, err = 0x%x\n", __LINE__, dwErr);
			goto exit;
		}
		printf_s("LB: ERROR Register RIO_SP_ERR_DET status = 0x%08x\n", dwRegVal);

		dwErr = TSI721RegisterRead(hDev, RIO_SP_RATE_EN, 1, &dwRegVal);

		if (dwErr != ERROR_SUCCESS) {
			printf_s("(%d) Read port status failed, err = 0x%x\n", __LINE__, dwErr);
			goto exit;
		}
		printf_s("LB: ERROR Register RIO_SP_RATE_EN status = 0x%08x\n", dwRegVal);

	//	dwErr = TSI721RegisterWrite(hDev, RIO_SP_LM_REQ, 0x00000003);
	//	if (dwErr != ERROR_SUCCESS) {
	//		printf_s("(%d) write port status failed, err = 0x%x\n", __LINE__, dwErr);
	//		goto exit;
	//	}
	//}

	/**********************************************************************/
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
		}
		else {
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
	 
	

exit:

	TSI721DeviceClose(hDev, NULL);

	free(obBuf);
	free(ibBuf);

	return 0;
}
