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

#include <project.h>
#include <common_define.h>

#include <linux/module.h>    // included for all kernel modules
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/slab.h>    // included for KERN_INFO
#include <linux/delay.h>

#include "piIOComm.h"
#include "RS485FwuCommand.h"


#define TEL_MAX_BUF_LEN  300

////*************************************************************************************************
INT32S fwuEnterFwuMode (INT8U address)
{
	pibridge_req_send_gate(address, eCmdSetFwUpdateMode, NULL, 0); 

	msleep(100);

	// there is no response for this command
	return 0;
}


////*************************************************************************************************
INT32S fwuWriteSerialNum (
    INT8U address,
    INT32U i32uSerNum_p)

{

	u8  rcv_buf[4];

	return pibridge_req_gate_tmt(
			address, 
			eCmdWriteSerialNumber, 
			(INT8U*)&i32uSerNum_p, 
			sizeof (INT32U),
			rcv_buf, 4, 200); 
}


INT32S fwuEraseFlash (INT8U address)
{
	u8  rcv_buf[4];

	return pibridge_req_gate_tmt(
			address, 
			eCmdEraseFwFlash, 
			NULL, 
			0,
			rcv_buf, 
			4, 
			6000); 
}

INT32S fwuWrite(INT8U address, INT32U flashAddr, char *data, INT32U length)
{
	INT8U ai8uSendBuf_l[TEL_MAX_BUF_LEN];
	u8  rcv_buf[4];

	memcpy (ai8uSendBuf_l, &flashAddr, sizeof (flashAddr));
	if (length <= 0 || length > TEL_MAX_BUF_LEN-sizeof(flashAddr))
		return -14;
	memcpy (ai8uSendBuf_l + sizeof (flashAddr), data, length);

	
	return pibridge_req_gate_tmt(
			address, 
			eCmdWriteFwFlash, 
			ai8uSendBuf_l, 
			sizeof(flashAddr) + length,
			rcv_buf, 
			4, 
			1000); 

}

INT32S fwuResetModule (INT8U address)
{

	return pibridge_req_gate_tmt(
			address, 
			eCmdResetModule, 
			NULL, 
			0, 
			NULL,
			0, 
			10000 + 100); 
			/*100ms more for the waiting, maybe*/
}


