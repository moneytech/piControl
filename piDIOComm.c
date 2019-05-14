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
#include <linux/pibridge.h>

#include <project.h>
#include <common_define.h>

#include "revpi_common.h"
#include "revpi_core.h"

static INT8U i8uConfigured_s = 0;
static SDioConfig dioConfig_s[10];
static INT8U i8uNumCounter[64];
static INT16U i16uCounterAct[64];

void piDIOComm_InitStart(void)
{
	i8uConfigured_s = 0;
}

INT32U piDIOComm_Config(uint8_t i8uAddress, uint16_t i16uNumEntries, SEntryInfo * pEnt)
{
	uint16_t i;

	if (i8uConfigured_s >= sizeof(dioConfig_s) / sizeof(SDioConfig)) {
		pr_err("max. number of DIOs reached\n");
		return -1;
	}

	pr_info_dio("piDIOComm_Config addr %d entries %d  num %d\n", i8uAddress, i16uNumEntries, i8uConfigured_s);
	memset(&dioConfig_s[i8uConfigured_s], 0, sizeof(SDioConfig));

	dioConfig_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitAddress = i8uAddress;
	dioConfig_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitIoHeaderType = 0;
	dioConfig_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitReqResp = 0;
	dioConfig_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitLength = sizeof(SDioConfig) - IOPROTOCOL_HEADER_LENGTH - 1;
	dioConfig_s[i8uConfigured_s].uHeader.sHeaderTyp1.bitCommand = IOP_TYP1_CMD_CFG;

	i8uNumCounter[i8uAddress] = 0;
	i16uCounterAct[i8uAddress] = 0;

	for (i = 0; i < i16uNumEntries; i++) {
		pr_info_dio("addr %2d  type %d  len %3d  offset %3d  value %d 0x%x\n",
				pEnt[i].i8uAddress, pEnt[i].i8uType, pEnt[i].i16uBitLength, pEnt[i].i16uOffset,
				pEnt[i].i32uDefault, pEnt[i].i32uDefault);

		if (pEnt[i].i16uOffset >= 88 && pEnt[i].i16uOffset <= 103) {
			dioConfig_s[i8uConfigured_s].i32uInputMode |=
				(pEnt[i].i32uDefault & 0x03) << ((pEnt[i].i16uOffset - 88) * 2);
			if ((pEnt[i].i32uDefault == 1 || pEnt[i].i32uDefault == 2)
					|| (pEnt[i].i32uDefault == 3 && ((pEnt[i].i16uOffset - 88) % 2) == 0)) {
				i8uNumCounter[i8uAddress]++;
				i16uCounterAct[i8uAddress] |= (1 << (pEnt[i].i16uOffset - 88));
			}
		} else {
			switch (pEnt[i].i16uOffset) {
				case 104:
					dioConfig_s[i8uConfigured_s].i8uInputDebounce = pEnt[i].i32uDefault;
					break;
				case 106:
					dioConfig_s[i8uConfigured_s].i16uOutputPushPull = pEnt[i].i32uDefault;
					break;
				case 108:
					dioConfig_s[i8uConfigured_s].i16uOutputOpenLoadDetect = pEnt[i].i32uDefault;
					break;
				case 110:
					dioConfig_s[i8uConfigured_s].i16uOutputPWM = pEnt[i].i32uDefault;
					break;
				case 112:
					dioConfig_s[i8uConfigured_s].i8uOutputPWMIncrement = pEnt[i].i32uDefault;
					break;
			}
		}
	}
	dioConfig_s[i8uConfigured_s].i8uCrc =
		piIoComm_Crc8((INT8U *) & dioConfig_s[i8uConfigured_s], sizeof(SDioConfig) - 1);

	pr_info_dio("piDIOComm_Config done addr %d input mode %08x  numCnt %d\n", i8uAddress,
			dioConfig_s[i8uConfigured_s].i32uInputMode, i8uNumCounter[i8uAddress]);
	i8uConfigured_s++;

	return 0;
}

INT32U piDIOComm_Init(INT8U i8uDevice_p)
{
	int ret;
	INT8U i;

	pr_info_dio("piDIOComm_Init %d of %d  addr %d numCnt %d\n", i8uDevice_p, i8uConfigured_s,
			RevPiDevice_getDev(i8uDevice_p)->i8uAddress,
			i8uNumCounter[RevPiDevice_getDev(i8uDevice_p)->i8uAddress]);

	for (i = 0; i < i8uConfigured_s; i++) {
		if (dioConfig_s[i].uHeader.sHeaderTyp1.bitAddress == RevPiDevice_getDev(i8uDevice_p)->i8uAddress) {
			ret = pibridge_req_io(
					dioConfig_s[i].uHeader.sHeaderTyp1.bitAddress,
					dioConfig_s[i].uHeader.sHeaderTyp1.bitCommand,
					((SIOGeneric *)&dioConfig_s[i])->ai8uData,
					dioConfig_s[i].uHeader.sHeaderTyp1.bitLength,
					NULL,
					0);
			if (ret) 
				return 1;
		}
	}

	return 0;
}

struct dio_pwm {
	u16	i16uOutput;
	u16	i16uChannels;
	u8	ai8uValue[16];
};

INT32U piDIOComm_sendCyclicTelegram(INT8U i8uDevice_p)
{
	INT8U len_l, data_out[18], i, p, data_in[70];
	static INT8U last_out[40][18];
	INT8U i8uAddress;
	u16 rcv_len;
	int ret;
	u8 cmd;

	if (RevPiDevice_getDev(i8uDevice_p)->sId.i16uFBS_OutputLength != 18) {
		return 4;
	}

	len_l = 18;
	i8uAddress = RevPiDevice_getDev(i8uDevice_p)->i8uAddress;

	if (piDev_g.stopIO == false) {
		rt_mutex_lock(&piDev_g.lockPI);
		memcpy(data_out, piDev_g.ai8uPI + RevPiDevice_getDev(i8uDevice_p)->i16uOutputOffset, len_l);
		rt_mutex_unlock(&piDev_g.lockPI);
	} else {
		memset(data_out, 0, len_l);
	}

	p = 255;
	for (i = len_l; i > 0; i--) {
		if (data_out[i - 1] != last_out[i8uAddress][i - 1]) {
			p = i - 1;
			break;
		}
	}


	if (p == 255 || p < 2) {
		// nur die direkten output bits haben sich geändert -> SDioRequest
		len_l = sizeof(INT16U);
		cmd = IOP_TYP1_CMD_DATA;
	} else {
		struct dio_pwm *pReq = (struct dio_pwm*) &data_out;

		// kopiere die pwm werte die sich geändert haben
		pReq->i16uChannels = 0;
		p = 0;
		for (i = 0; i < 16; i++) {
			if (last_out[i8uAddress][i + 2] != data_out[i + 2]) {
				pReq->i16uChannels |= 1 << i;
				pReq->ai8uValue[p++] = data_out[i + 2];
			}
		}
		len_l = p + 2 * sizeof(INT16U);
		cmd = IOP_TYP1_CMD_DATA2;
	}


	memcpy(last_out[i8uAddress], data_out, sizeof(data_out));

	rcv_len = 3 * sizeof(INT16U) + i8uNumCounter[i8uAddress] * sizeof(INT32U);
	ret = pibridge_req_io(i8uAddress, cmd, data_out, len_l, data_in, rcv_len); 
	if (ret)
		return 1; 

	memcpy(&piDev_g.ai8uPI + RevPiDevice_getDev(i8uDevice_p)->i16uInputOffset, data_in, 3 * sizeof(INT16U));

	memset(&piDev_g.ai8uPI + RevPiDevice_getDev(i8uDevice_p)->i16uInputOffset + 6, 0, 64);

	rt_mutex_lock(&piDev_g.lockPI);
	p = 0;
	for (i = 0; i < 16; i++) {
		if (i16uCounterAct[i8uAddress] & (1 << i)) {
			memcpy(piDev_g.ai8uPI + RevPiDevice_getDev(i8uDevice_p)->i16uInputOffset + 3 * sizeof(INT16U) + i * sizeof(INT32U), 
					&data_in[3 * sizeof(INT16U) + i * sizeof(INT32U)], sizeof(INT32U));
		} 
	}

	rt_mutex_unlock(&piDev_g.lockPI);


	return 0;
}
