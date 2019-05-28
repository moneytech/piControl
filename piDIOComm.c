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

	pr_info("piDIOComm_Init %d of %d  addr %d numCnt %d\n", i8uDevice_p, i8uConfigured_s,
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
			if (ret){ 
				pr_err("piDIOComm_Init ret %d\n", ret);
				return 1;
			}
		}
	}
	pr_info("piDIOComm:%d\n", piCore_g.eBridgeState);
	return 0;
}

#define DIO_PIN_COUNT 16
#define DIO_ENCODER_MAX	8

#define DIO_OUTPUT_BUF_PWM (DIO_PIN_COUNT * sizeof(u8))
#define DIO_OUTPUT_BUF_MAX  (DIO_OUTPUT_BUF_PWM + sizeof(u16))

#pragma pack(push, 1)
struct dio_pwm_hdr {
	u16     output;
	u16     channels;
};

struct dio_pwm {
	struct 	dio_pwm_hdr 	hdr;
	u8     			value[DIO_PIN_COUNT];
};
struct dio_resp_hdr {
	u16	input;
	u16 	output;
	u16 	mod_status;
};
struct dio_resp {
	struct dio_resp_hdr	hdr;
	u32 			counter[DIO_ENCODER_MAX];
};
#pragma pack(pop)

INT32U piDIOComm_sendCyclicTelegram(INT8U i8uDevice_p)
{
	static u8 last_out[40][DIO_OUTPUT_BUF_PWM];
	u8 buf_out[DIO_OUTPUT_BUF_MAX];
	struct dio_resp	*img_input;
	struct dio_resp	buf_resp;
	u16 sndlen;
	u16 rcvlen;
	int ret;
	u8 addr;
	u8 cmd;
	int i;
	int p;

	if (piDev_g.stopIO == false) {
		rt_mutex_lock(&piDev_g.lockPI);
		memcpy(buf_out, piDev_g.ai8uPI + RevPiDevice_getDev(i8uDevice_p)->i16uOutputOffset, DIO_OUTPUT_BUF_MAX);
		rt_mutex_unlock(&piDev_g.lockPI);
	} else {
		memset(buf_out, 0, DIO_OUTPUT_BUF_MAX);
	}

	addr = RevPiDevice_getDev(i8uDevice_p)->i8uAddress;
	rcvlen = sizeof(struct dio_resp_hdr) + i8uNumCounter[addr] * sizeof(u32);

	if (0 == memcmp(buf_out + 2, last_out[addr], DIO_OUTPUT_BUF_PWM)) {
		sndlen = sizeof(u16);
		cmd = IOP_TYP1_CMD_DATA;
		ret = pibridge_req_io(addr, cmd, buf_out, sndlen, (u8 *)&buf_resp, rcvlen);
		if (ret){
			pr_err_ratelimited("io direct request error\n");
			return -1;
		}
	} else {
		u8 *last = last_out[addr];
		u8 *buf = buf_out + sizeof(u16);
		struct dio_pwm pwm;
		int cnt = 0;

		memcpy(&pwm.hdr.output, buf_out, sizeof(pwm.hdr.output));
		pwm.hdr.channels = 0;
		for (i = 0; i < DIO_PIN_COUNT; i++) {
			if (*(last + i) != *(buf + i)) {
				pwm.hdr.channels |= 1 << i;
				pwm.value[cnt++] = *(buf + i);
			}
		}
		sndlen = cnt * sizeof(u8) + sizeof(pwm.hdr);
		cmd = IOP_TYP1_CMD_DATA2;
		ret = pibridge_req_io(addr, cmd, (u8 *)&pwm, sndlen,(u8 *)&buf_resp, rcvlen);
		if (ret){
			pr_err_ratelimited("io pwm request error\n");
			return -2;
		}
	}
	memcpy(last_out[addr], buf_out + sizeof(u16), DIO_OUTPUT_BUF_MAX);

	img_input = (struct dio_resp *)(piDev_g.ai8uPI + RevPiDevice_getDev(i8uDevice_p)->i16uInputOffset);
	rt_mutex_lock(&piDev_g.lockPI);

	img_input->hdr.input = buf_resp.hdr.input;
	img_input->hdr.output = buf_resp.hdr.output;
	img_input->hdr.mod_status = buf_resp.hdr.mod_status;

	/* to be consistent with original code, should be not needed */
	/*70 = 6 + 64 = 6 + sizeof(u32) * 16, currently 8 is the maximun as two pins consist one encoder. */
	memset(img_input, 0, 70 - sizeof(struct dio_resp_hdr));

	for (p = 0, i = 0; i < DIO_ENCODER_MAX; i++) {
		if (i16uCounterAct[addr] & (1 << i)) {
			img_input->counter[i] = buf_resp.counter[p];
			p++;
		}
	}
	rt_mutex_unlock(&piDev_g.lockPI);

	return 0;
}

