/*=======================================================================================
 *
 *	       KK    KK   UU    UU   NN    NN   BBBBBB    UU    UU    SSSSSS
 *	       KK   KK    UU    UU   NNN   NN   BB   BB   UU    UU   SS
 *	       KK  KK     UU    UU   NNNN  NN   BB   BB   UU    UU   SS
 *	+----- KKKKK      UU    UU   NN NN NN   BBBBB     UU    UU    SSSSS
 *	|      KK  KK     UU    UU   NN  NNNN   BB   BB   UU    UU        SS
 *	|      KK   KK    UU    UU   NN   NNN   BB   BB   UU    UU        SS
 *	|      KK    KKK   UUUUUU    NN    NN   BBBBBB     UUUUUU    SSSSSS     GmbH
 *	|
 *	|            [#]  I N D U S T R I A L   C O M M U N I C A T I O N
 *	|             |
 *	+-------------+
 *
 *---------------------------------------------------------------------------------------
 *
 * (C) KUNBUS GmbH, Heerweg 15C, 73770 Denkendorf, Germany
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License V2 as published by
 * the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  For licencing details see COPYING
 *
 *=======================================================================================
 */

#include <linux/module.h>	// included for all kernel modules
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/termios.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <asm/segment.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/gpio.h>
#include <linux/jiffies.h>

#include <project.h>
#include <common_define.h>
#include <RS485FwuCommand.h>
#include "compat.h"
#include "revpi_common.h"
#include "revpi_core.h"
#include "piIOComm.h"


INT8U piIoComm_Crc8(INT8U * pi8uFrame_p, INT16U i16uLen_p)
{
	INT8U i8uRv_l = 0;

	while (i16uLen_p--) {
		i8uRv_l = i8uRv_l ^ pi8uFrame_p[i16uLen_p];
	}
	return i8uRv_l;
}


void piIoComm_writeSniff1A(EGpioValue eVal_p, EGpioMode eMode_p)
{
#ifdef DEBUG_GPIO
	pr_info("sniff1A: mode %d value %d\n", (int)eMode_p, (int)eVal_p);
#endif
	piIoComm_writeSniff(piCore_g.gpio_sniff1a, eVal_p, eMode_p);
}

void piIoComm_writeSniff1B(EGpioValue eVal_p, EGpioMode eMode_p)
{
	if (piDev_g.machine_type == REVPI_CORE) {
		piIoComm_writeSniff(piCore_g.gpio_sniff1b, eVal_p, eMode_p);
#ifdef DEBUG_GPIO
		pr_info("sniff1B: mode %d value %d\n", (int)eMode_p, (int)eVal_p);
#endif
	}
}

void piIoComm_writeSniff2A(EGpioValue eVal_p, EGpioMode eMode_p)
{
#ifdef DEBUG_GPIO
	pr_info("sniff2A: mode %d value %d\n", (int)eMode_p, (int)eVal_p);
#endif
	piIoComm_writeSniff(piCore_g.gpio_sniff2a, eVal_p, eMode_p);
}

void piIoComm_writeSniff2B(EGpioValue eVal_p, EGpioMode eMode_p)
{
	if (piDev_g.machine_type == REVPI_CORE) {
		piIoComm_writeSniff(piCore_g.gpio_sniff2b, eVal_p, eMode_p);
#ifdef DEBUG_GPIO
		pr_info("sniff2B: mode %d value %d\n", (int)eMode_p, (int)eVal_p);
#endif
	}
}

void piIoComm_writeSniff(struct gpio_desc *pGpio, EGpioValue eVal_p, EGpioMode eMode_p)
{
	if (eMode_p == enGpioMode_Input) {
		gpiod_direction_input(pGpio);
	} else {
		gpiod_direction_output(pGpio, eVal_p);
		gpiod_set_value(pGpio, eVal_p);
	}
}

EGpioValue piIoComm_readSniff1A()
{
	EGpioValue v = piIoComm_readSniff(piCore_g.gpio_sniff1a);
#ifdef DEBUG_GPIO
	pr_info("sniff1A: input value %d\n", (int)v);
#endif
	return v;
}

EGpioValue piIoComm_readSniff1B()
{
	if (piDev_g.machine_type == REVPI_CORE) {
		EGpioValue v = piIoComm_readSniff(piCore_g.gpio_sniff1b);
#ifdef DEBUG_GPIO
		pr_info("sniff1B: input value %d\n", (int)v);
#endif
		return v;
	}
	return enGpioValue_Low;
}

EGpioValue piIoComm_readSniff2A()
{
	EGpioValue v = piIoComm_readSniff(piCore_g.gpio_sniff2a);
#ifdef DEBUG_GPIO
	pr_info("sniff2A: input value %d\n", (int)v);
#endif
	return v;
}

EGpioValue piIoComm_readSniff2B()
{
	if (piDev_g.machine_type == REVPI_CORE) {
		EGpioValue v = piIoComm_readSniff(piCore_g.gpio_sniff2b);
#ifdef DEBUG_GPIO
		pr_info("sniff2B: input value %d\n", (int)v);
#endif
		return v;
	}
	return enGpioValue_Low;
}

EGpioValue piIoComm_readSniff(struct gpio_desc * pGpio)
{
	EGpioValue ret = enGpioValue_Low;

	if (gpiod_get_value_cansleep(pGpio))
		ret = enGpioValue_High;

	return ret;
}

INT32S piIoComm_sendRS485Tel(INT16U i16uCmd_p, INT8U i8uAddress_p,
			     INT8U * pi8uSendData_p, INT8U i8uSendDataLen_p, INT8U * pi8uRecvData_p, INT16U * pi16uRecvDataLen_p)
{
	SRs485Telegram suSendTelegram_l;
	SRs485Telegram suRecvTelegram_l;
	INT32S i32uRv_l = 0;
	INT8U i8uLen_l;

	memset(&suSendTelegram_l, 0, sizeof(SRs485Telegram));
	suSendTelegram_l.i8uDstAddr = i8uAddress_p;	// receiver address
	suSendTelegram_l.i8uSrcAddr = 0;	// sender Master
	suSendTelegram_l.i16uCmd = i16uCmd_p;	// command
	if (pi8uSendData_p != NULL) {
		suSendTelegram_l.i8uDataLen = i8uSendDataLen_p;
		memcpy(suSendTelegram_l.ai8uData, pi8uSendData_p, i8uSendDataLen_p);
	} else {
		suSendTelegram_l.i8uDataLen = 0;
	}
	suSendTelegram_l.ai8uData[i8uSendDataLen_p] = piIoComm_Crc8((INT8U *) & suSendTelegram_l, RS485_HDRLEN + i8uSendDataLen_p);

	if (piIoComm_send((INT8U *) & suSendTelegram_l, RS485_HDRLEN + i8uSendDataLen_p + 1) == 0) {
		uint16_t timeout_l;
		pr_info_serial2("send gateprotocol addr %d cmd 0x%04x\n", suSendTelegram_l.i8uDstAddr, suSendTelegram_l.i16uCmd);

		if (i8uAddress_p == 255)	// address 255 is for broadcasts without reply
			return 0;

		if (i8uSendDataLen_p > 0 && pi8uSendData_p[0] == 'F') {
			// this starts a flash erase on the master module
			// this usually take longer than the normal timeout
			// -> increase the timeout value to 1s
			timeout_l = 1000; // ms
		} else {
			timeout_l = REV_PI_IO_TIMEOUT;
		}
		if (piIoComm_recv_timeout((INT8U *) & suRecvTelegram_l, RS485_HDRLEN, timeout_l) == RS485_HDRLEN) {
			// header was received -> receive data part
#ifdef DEBUG_SERIALCOMM
			if (pi16uRecvDataLen_p != NULL) {
				i8uLen_l = *pi16uRecvDataLen_p;
			}
			pr_info("recv gateprotocol cmd/addr/len %x/%d/%d -> %x/%d/%d\n",
				i16uCmd_p, i8uAddress_p, i8uLen_l,
				suRecvTelegram_l.i16uCmd, suRecvTelegram_l.i8uSrcAddr, RS485_HDRLEN + suRecvTelegram_l.i8uDataLen + 1);
#endif
			if ((suRecvTelegram_l.i16uCmd & MODGATE_RS485_COMMAND_ANSWER_FILTER) != suSendTelegram_l.i16uCmd) {
				pr_info_serial("wrong cmd code in response\n");
				i32uRv_l = 5;
			} else {
				i8uLen_l = piIoComm_recv(suRecvTelegram_l.ai8uData, suRecvTelegram_l.i8uDataLen + 1);
				if (i8uLen_l != suRecvTelegram_l.i8uDataLen + 1
				    || suRecvTelegram_l.ai8uData[suRecvTelegram_l.i8uDataLen] !=
				    piIoComm_Crc8((INT8U *) & suRecvTelegram_l, RS485_HDRLEN + suRecvTelegram_l.i8uDataLen)) {
					pr_info_serial
					    ("recv gateprotocol crc error: len=%d, %02x %02x %02x %02x %02x %02x %02x %02x\n",
					     suRecvTelegram_l.i8uDataLen, suRecvTelegram_l.ai8uData[0],
					     suRecvTelegram_l.ai8uData[1], suRecvTelegram_l.ai8uData[2],
					     suRecvTelegram_l.ai8uData[3], suRecvTelegram_l.ai8uData[4],
					     suRecvTelegram_l.ai8uData[5], suRecvTelegram_l.ai8uData[6], suRecvTelegram_l.ai8uData[7]);

					i32uRv_l = 4;
				} else if (suRecvTelegram_l.i16uCmd & MODGATE_RS485_COMMAND_ANSWER_ERROR) {
					pr_info_serial("recv gateprotocol error %08x\n", *(INT32U *) (suRecvTelegram_l.ai8uData));
					i32uRv_l = 3;
				} else {
					if (pi16uRecvDataLen_p != NULL) {
						*pi16uRecvDataLen_p = suRecvTelegram_l.i8uDataLen;
					}
					if (pi8uRecvData_p != NULL) {
						memcpy(pi8uRecvData_p, suRecvTelegram_l.ai8uData, suRecvTelegram_l.i8uDataLen);
					}
					i32uRv_l = 0;
				}
			}
		} else {
			i32uRv_l = 2;
		}
	} else {
		i32uRv_l = 1;
	}
	return i32uRv_l;
}

INT32S piIoComm_sendTelegram(SIOGeneric * pRequest_p, SIOGeneric * pResponse_p)
{
	INT32S i32uRv_l = 0;
	INT8U len_l;
	int ret;
#if 0				//def DEBUG_DEVICE_IO
	static INT8U last_out[40][2];
	static INT8U last_in[40][2];
#endif

	len_l = pRequest_p->uHeader.sHeaderTyp1.bitLength;

	pRequest_p->ai8uData[len_l] = piIoComm_Crc8((INT8U *) pRequest_p, IOPROTOCOL_HEADER_LENGTH + len_l);

#if 0				//def DEBUG_DEVICE_IO
	if (last_out[pRequest_p->uHeader.sHeaderTyp1.bitAddress][0] != pRequest_p->ai8uData[0]
	    || last_out[pRequest_p->uHeader.sHeaderTyp1.bitAddress][1] != pRequest_p->ai8uData[1]) {
		last_out[pRequest_p->uHeader.sHeaderTyp1.bitAddress][0] = pRequest_p->ai8uData[0];
		last_out[pRequest_p->uHeader.sHeaderTyp1.bitAddress][1] = pRequest_p->ai8uData[1];
		pr_info("dev %2d: send cyclic Data addr %d + %d output 0x%02x 0x%02x\n",
			pRequest_p->uHeader.sHeaderTyp1.bitAddress,
			pRequest_p->uHeader.sHeaderTyp1.bitAddress,
			RevPiDevice.dev[i8uDevice_p].i16uOutputOffset, sRequest_l.ai8uData[0], sRequest_l.ai8uData[1]);
	}
#endif

	ret = piIoComm_send((INT8U *) pRequest_p, IOPROTOCOL_HEADER_LENGTH + len_l + 1);
	if (ret == 0) {
		ret = piIoComm_recv((INT8U *) pResponse_p, REV_PI_RECV_IO_HEADER_LEN);
		if (ret > 0) {
			len_l = pResponse_p->uHeader.sHeaderTyp1.bitLength;
			if (pResponse_p->ai8uData[len_l] == piIoComm_Crc8((INT8U *) pResponse_p, IOPROTOCOL_HEADER_LENGTH + len_l)) {
				// success
#ifdef DEBUG_DEVICE_IO
				int i;
				pr_info("len %d, resp %d, cmd %d\n",
					pResponse_p->uHeader.sHeaderTyp1.bitLength,
					pResponse_p->uHeader.sHeaderTyp1.bitReqResp, pResponse_p->uHeader.sHeaderTyp1.bitCommand);
				for (i = 0; i < pResponse_p->uHeader.sHeaderTyp1.bitLength; i++) {
					pr_info("%02x ", pResponse_p->ai8uData[i]);
				}
				pr_info("\n");
#endif
#if 0				//def DEBUG_DEVICE_IO
				if (last_in[pRequest_p->uHeader.sHeaderTyp1.bitAddress][0] != pResponse_p->ai8uData[0]
				    || last_in[pRequest_p->uHeader.sHeaderTyp1.bitAddress][1] != pResponse_p->ai8uData[1]) {
					last_in[pRequest_p->uHeader.sHeaderTyp1.bitAddress][0] = pResponse_p->ai8uData[0];
					last_in[pRequest_p->uHeader.sHeaderTyp1.bitAddress][1] = pResponse_p->ai8uData[1];
					pr_info("dev %2d: recv cyclic Data addr %d + %d input 0x%02x 0x%02x\n\n",
						RevPiDevice.dev[i8uDevice_p].i8uAddress,
						sResponse_l.uHeader.sHeaderTyp1.bitAddress,
						RevPiDevice.dev[i8uDevice_p].i16uInputOffset, sResponse_l.ai8uData[0], sResponse_l.ai8uData[1]);
				}
#endif
			} else {
				i32uRv_l = 1;
#ifdef DEBUG_DEVICE_IO
				pr_info("dev %2d: recv ioprotocol crc error\n", pRequest_p->uHeader.sHeaderTyp1.bitAddress);
				pr_info("len %d, resp %d, cmd %d\n", pResponse_p->uHeader.sHeaderTyp1.bitLength,
					pResponse_p->uHeader.sHeaderTyp1.bitReqResp, pResponse_p->uHeader.sHeaderTyp1.bitCommand);
#endif
			}
		} else {
			i32uRv_l = 2;
#ifdef DEBUG_DEVICE_IO
			pr_info("dev %2d: recv ioprotocol timeout error\n", pRequest_p->uHeader.sHeaderTyp1.bitAddress);
#endif
		}
	} else {
		i32uRv_l = 3;
#ifdef DEBUG_DEVICE_IO
		pr_info("dev %2d: send ioprotocol send error %d\n", pRequest_p->uHeader.sHeaderTyp1.bitAddress, ret);
#endif
	}
	return i32uRv_l;
}

INT32S piIoComm_gotoGateProtocol(void)
{
	SIOGeneric sRequest_l;
	INT8U len_l;
	int ret;

	len_l = 0;

	sRequest_l.uHeader.sHeaderTyp2.bitCommand = IOP_TYP2_CMD_GOTO_GATE_PROTOCOL;
	sRequest_l.uHeader.sHeaderTyp2.bitIoHeaderType = 1;
	sRequest_l.uHeader.sHeaderTyp2.bitReqResp = 0;
	sRequest_l.uHeader.sHeaderTyp2.bitLength = len_l;
	sRequest_l.uHeader.sHeaderTyp2.bitDataPart1 = 0;

	sRequest_l.ai8uData[len_l] = piIoComm_Crc8((INT8U *) & sRequest_l, IOPROTOCOL_HEADER_LENGTH + len_l);

	ret = piIoComm_send((INT8U *) & sRequest_l, IOPROTOCOL_HEADER_LENGTH + len_l + 1);
	if (ret == 0) {
		// there is no reply
	} else {
#ifdef DEBUG_DEVICE_IO
		pr_info("dev all: send ioprotocol send error %d\n", ret);
#endif
	}
	return 0;
}

INT32S piIoComm_gotoFWUMode(int address)
{
	return fwuEnterFwuMode(address);
}

INT32S piIoComm_fwuSetSerNum(int address, INT32U serNum)
{
	return fwuWriteSerialNum(address, serNum);
}

INT32S piIoComm_fwuFlashErase(int address)
{
	return fwuEraseFlash(address);
}

INT32S piIoComm_fwuFlashWrite(int address, INT32U flashAddr, char *data, INT32U length)
{
	return fwuWrite(address, flashAddr, data, length);
}

INT32S piIoComm_fwuReset(int address)
{
	return fwuResetModule(address);
}
