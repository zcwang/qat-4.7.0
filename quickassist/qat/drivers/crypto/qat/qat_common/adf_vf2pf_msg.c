/*
  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY
  Copyright(c) 2015 Intel Corporation.
  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  Contact Information:
  qat-linux@intel.com

  BSD LICENSE
  Copyright(c) 2015 Intel Corporation.
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "adf_accel_devices.h"
#include "adf_common_drv.h"
#include "adf_pf2vf_msg.h"

/**
 * adf_vf2pf_init() - send init msg to PF
 * @accel_dev:  Pointer to acceleration VF device.
 *
 * Function sends an init messge from the VF to a PF
 *
 * Return: 0 on success, error code otherwise.
 */
int adf_vf2pf_init(struct adf_accel_dev *accel_dev)
{
	u32 msg = (ADF_VF2PF_MSGORIGIN_SYSTEM |
		(ADF_VF2PF_MSGTYPE_INIT << ADF_VF2PF_MSGTYPE_SHIFT));

	if (adf_iov_notify(accel_dev, msg, 0)) {
		dev_err(&GET_DEV(accel_dev),
			"Failed to send Init event to PF\n");
		return -EFAULT;
	}
	set_bit(ADF_STATUS_PF_RUNNING, &accel_dev->status);
	return 0;
}
EXPORT_SYMBOL_GPL(adf_vf2pf_init);

/**
 * adf_vf2pf_shutdown() - send shutdown msg to PF
 * @accel_dev:  Pointer to acceleration VF device.
 *
 * Function sends a shutdown messge from the VF to a PF
 *
 * Return: void
 */
void adf_vf2pf_shutdown(struct adf_accel_dev *accel_dev)
{
	u32 msg = (ADF_VF2PF_MSGORIGIN_SYSTEM |
	    (ADF_VF2PF_MSGTYPE_SHUTDOWN << ADF_VF2PF_MSGTYPE_SHIFT));

	if (test_bit(ADF_STATUS_PF_RUNNING, &accel_dev->status))
		if (adf_iov_notify(accel_dev, msg, 0))
			dev_err(&GET_DEV(accel_dev),
				"Failed to send Shutdown event to PF\n");
}
EXPORT_SYMBOL_GPL(adf_vf2pf_shutdown);

static int adf_iov_block_get_bc(struct adf_accel_dev *accel_dev, u8 msg_type,
				u8 msg_index, u8 *data, int get_crc)
{
	u8 blk_type;
	u32 msg;
	unsigned long timeout = msecs_to_jiffies(ADF_IOV_MSG_RESP_TIMEOUT);
	int response_received = 0;
	int retry_count = 0;

	msg = ADF_VF2PF_MSGORIGIN_SYSTEM;
	if (get_crc)
		msg |= 1 << ADF_VF2PF_BLOCK_REQ_CRC_SHIFT;

	if (msg_type <= ADF_VF2PF_MAX_SMALL_MESSAGE_TYPE) {
		if (msg_index >=
			ADF_VF2PF_SMALL_PAYLOAD_SIZE + ADF_VF2PF_BLOCK_DATA) {
			dev_err(&GET_DEV(accel_dev),
				"Invalid byte index %d for message type %d\n",
				msg_index, msg_type);
			return -EINVAL;
		}
		msg |= ADF_VF2PF_MSGTYPE_GET_SMALL_BLOCK_REQ <<
			ADF_VF2PF_MSGTYPE_SHIFT;
		blk_type = msg_type;
		msg |= blk_type << ADF_VF2PF_BLOCK_REQ_TYPE_SHIFT;
		msg |= msg_index << ADF_VF2PF_SMALL_BLOCK_BYTE_NUM_SHIFT;
	} else if (msg_type <= ADF_VF2PF_MAX_MEDIUM_MESSAGE_TYPE) {
		if (msg_index >=
			ADF_VF2PF_MEDIUM_PAYLOAD_SIZE + ADF_VF2PF_BLOCK_DATA) {
			dev_err(&GET_DEV(accel_dev),
				"Invalid byte index %d for message type %d\n",
				msg_index, msg_type);
			return -EINVAL;
		}
		msg |= ADF_VF2PF_MSGTYPE_GET_MEDIUM_BLOCK_REQ <<
			ADF_VF2PF_MSGTYPE_SHIFT;
		blk_type = msg_type - ADF_VF2PF_MIN_MEDIUM_MESSAGE_TYPE;
		msg |= blk_type << ADF_VF2PF_BLOCK_REQ_TYPE_SHIFT;
		msg |= msg_index << ADF_VF2PF_MEDIUM_BLOCK_BYTE_NUM_SHIFT;
	} else if (msg_type <= ADF_VF2PF_MAX_LARGE_MESSAGE_TYPE) {
		if (msg_index >=
			ADF_VF2PF_LARGE_PAYLOAD_SIZE + ADF_VF2PF_BLOCK_DATA) {
			dev_err(&GET_DEV(accel_dev),
				"Invalid byte index %d for message type %d\n",
				msg_index, msg_type);
			return -EINVAL;
		}
		msg |= ADF_VF2PF_MSGTYPE_GET_LARGE_BLOCK_REQ <<
			ADF_VF2PF_MSGTYPE_SHIFT;
		blk_type = msg_type - ADF_VF2PF_MIN_LARGE_MESSAGE_TYPE;
		msg |= blk_type << ADF_VF2PF_BLOCK_REQ_TYPE_SHIFT;
		msg |= msg_index << ADF_VF2PF_LARGE_BLOCK_BYTE_NUM_SHIFT;
	} else {
		dev_err(&GET_DEV(accel_dev),
			"Invalid message type %d\n", msg_type);
	}

	do {
		/* Send request from VF to PF */
		if (retry_count)
			accel_dev->vf.pfvf_counters.retry++;
		if (adf_iov_putmsg(accel_dev, msg, 0)) {
			dev_err(&GET_DEV(accel_dev),
				"Failed to send block request to PF\n");
			return -EIO;
		}

		/* Wait for response */
		if (!wait_for_completion_timeout
				(&accel_dev->vf.iov_msg_completion, timeout))
			dev_err(&GET_DEV(accel_dev),
				"IOV response message timeout\n");
		else
			response_received = 1;
	} while (!response_received &&
		 ++retry_count < ADF_IOV_MSG_RESP_RETRIES);

	if (!response_received)
		accel_dev->vf.pfvf_counters.rx_timeout++;
	else
		accel_dev->vf.pfvf_counters.rx_rsp++;

	if (!response_received)
		return -EIO;

	if (accel_dev->vf.pf2vf_block_resp_type !=
			(get_crc ? ADF_PF2VF_BLOCK_RESP_TYPE_CRC :
				ADF_PF2VF_BLOCK_RESP_TYPE_DATA)) {
		dev_err(&GET_DEV(accel_dev),
			"%sBlock response type %d, data %d, msg %d, index %d\n",
			get_crc ? "CRC " : "",
			accel_dev->vf.pf2vf_block_resp_type,
			accel_dev->vf.pf2vf_block_byte,
			msg_type, msg_index);
		return -EIO;
	}
	*data = accel_dev->vf.pf2vf_block_byte;
	return 0;
}

static int adf_iov_block_get_byte(struct adf_accel_dev *accel_dev, u8 msg_type,
				  u8 msg_index, u8 *data)
{
	return adf_iov_block_get_bc(accel_dev, msg_type, msg_index, data, 0);
}

static int adf_iov_block_get_crc(struct adf_accel_dev *accel_dev, u8 msg_type,
				 u8 msg_index, u8 *crc)
{
	return adf_iov_block_get_bc(accel_dev, msg_type, msg_index - 1, crc, 1);
}

int adf_iov_block_get(struct adf_accel_dev *accel_dev, u8 msg_type,
		      u8 *block_version, u8 *buffer, u8 *length)
{
	u8 buf_size = *length;
	u8 payload_len;
	u8 remote_crc;
	u8 local_crc;
	u8 buf_index;
	int ret;

	if (msg_type > ADF_VF2PF_MAX_LARGE_MESSAGE_TYPE) {
		dev_err(&GET_DEV(accel_dev),
			"Invalid message type %d\n", msg_type);
		return -EINVAL;
	}

	ret = adf_iov_block_get_byte(accel_dev, msg_type,
				     ADF_VF2PF_BLOCK_VERSION_BYTE,
				     block_version);
	if (ret)
		return ret;
	ret = adf_iov_block_get_byte(accel_dev, msg_type,
				     ADF_VF2PF_BLOCK_LEN_BYTE, length);
	if (ret)
		return ret;

	payload_len = *length;

	if (buf_size < payload_len) {
		dev_err(&GET_DEV(accel_dev),
			"Truncating block type %d response from %d to %d bytes\n",
			msg_type, payload_len, buf_size);
		payload_len = buf_size;
	}

	/* Get the data */
	for (buf_index = 0; buf_index < payload_len; buf_index++) {
		ret = adf_iov_block_get_byte(accel_dev, msg_type,
					     buf_index + ADF_VF2PF_BLOCK_DATA,
					     buffer + buf_index);
		if (ret)
			return ret;
	}

	ret = adf_iov_block_get_crc(accel_dev, msg_type,
				    payload_len + ADF_VF2PF_BLOCK_DATA,
				    &remote_crc);
	if (ret)
		return ret;
	local_crc = adf_pfvf_crc(ADF_CRC8_INIT_VALUE, block_version, 1);
	local_crc = adf_pfvf_crc(local_crc, length, 1);
	local_crc = adf_pfvf_crc(local_crc, buffer, payload_len);
	if (local_crc != remote_crc) {
		dev_err(&GET_DEV(accel_dev),
			"CRC error on msg type %d. Local %02X, remote %02X\n",
			msg_type, local_crc, remote_crc);
		accel_dev->vf.pfvf_counters.crc_err++;
		return -EIO;
	}

	accel_dev->vf.pfvf_counters.blk_rx++;
	*length = payload_len;
	return 0;
}
