/**
 * @file vendor_data.c
 *
 * @brief Implementation of vendor-specific data handling
 *
 * Copyright (c) 2013-2018 Microchip Technology Inc. and its subsidiaries.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Subject to your compliance with these terms, you may use Microchip
 * software and any derivatives exclusively with Microchip products.
 * It is your responsibility to comply with third party license terms applicable
 * to your use of third party software (including open source software) that
 * may accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
 * INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
 * AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
 * LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
 * LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
 * SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
 * POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
 * ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
 * RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * \asf_license_stop
 *
 */

/*
 * Copyright (c) 2014-2018 Microchip Technology Inc. and its subsidiaries.
 *
 * Licensed under Atmel's Limited License Agreement --> EULA.txt
 */

#ifdef VENDOR_DATA

/* === INCLUDES ============================================================ */

#include <stdio.h>

#include "rf4ce.h"
#include "zrc.h"
#include "vendor_data.h"
#include "app_config.h"
#ifdef FLASH_SUPPORT
#include "pal_flash.h"
#endif
#ifdef BOOT_FLASH
#include "pal_boot_flash.h"
#endif
#ifdef TFA_BAT_MON
#include "tfa.h"
#endif
#include "conf_board.h"

/* === MACROS ============================================================== */

/* Flash start address of the firmware image .*/
#define IMAGE_START_ADDR            ((uint32_t)60 * 1024)

/* Maximum size of the firmware image .*/
#define IMAGE_SIZE                  ((uint32_t)60 * 1024)

#define FW_VERSION_MAJOR (0x02)
#define FW_VERSION_MINOR (0x00)
#define FW_VERSION_REV   (0x00)

/* === EXTERNALS =========================================================== */

FLASH_EXTERN(uint16_t VendorIdentifier);
static uint16_t get_batmon_voltage(void);

#ifdef RF4CE_TARGET
extern void vendor_data_confirm(nwk_enum_t Status, uint8_t PairingRef,
		profile_id_t ProfileId,
		uint8_t Handle
		);

#else /* RF4CE_TARGET */
extern void nlme_rx_enable_confirm(nwk_enum_t Status);
void vendor_data_confirm(nwk_enum_t Status, uint8_t PairingRef,
		profile_id_t ProfileId,
		uint8_t Handle
		);
void vendor_data_ind(uint8_t PairingRef, uint16_t VendorId,
		uint8_t nsduLength, uint8_t *nsdu, uint8_t RxLinkQuality,
		uint8_t RxFlags);

#endif /* RF4CE_TARGET */

/* === IMPLEMENTATION ====================================================== */

#ifndef RF4CE_TARGET
void vendor_data_ind(uint8_t PairingRef, uint16_t VendorId,
		uint8_t nsduLength, uint8_t *nsdu, uint8_t RxLinkQuality,
		uint8_t RxFlags)
{
	/* Check if vendor id matches.
	 * Handle here only vendor data from same vendor */
	uint16_t v_id = PGM_READ_WORD(&VendorIdentifier);
	uint8_t nsduHandle = 1;
	if ((VendorId == v_id) && (RxFlags & RX_FLAG_WITH_SEC)) {
		switch (nsdu[0]) { /* vendor-specific command id */
		case BATTERY_STATUS_REQ:
		{
			uint16_t voltage = get_batmon_voltage();
			nsdu[0] = BATTERY_STATUS_RESP;
			nsdu[1] = (uint8_t)voltage; /* LSB */
			nsdu[2] = (uint8_t)(voltage >> 8); /* MSB */
			nsduLength = 3;
		}
		break;

		case ALIVE_REQ: /* Alive request */
			vendor_app_alive_req();
			/* Send alive response */
			nsdu[0] = ALIVE_RESP;
			nsduLength = 1;
			break;

		case FW_VERSION_REQ:
		{
			/* Send alive response */
			nsdu[0] = FW_VERSION_RESP;
			nsdu[1] = FW_VERSION_MAJOR; /* major version number */
			nsdu[2] = FW_VERSION_MINOR; /* minor version number */
			nsdu[3] = FW_VERSION_REV; /* revision version number */
			nsduLength = 4;
		}
		break;

		case RX_ON_REQ:
		{
			uint32_t duration = 0;

			memcpy(&duration, &nsdu[1], 3);
			if (!nlme_rx_enable_request(duration,
					(FUNC_PTR)nlme_rx_enable_confirm
					)) {
				/*
				 * RX enable could not been added to the queue.
				 * Therefore do not send response message.
				 */
				return;
			}

			/* Send response */
			nsdu[0] = RX_ON_RESP;
			nsduLength = 1;
		}
		break;

		default:
		{
			/* Send response */
			nsdu[0] = FW_DATA_RESP;
			nsdu[1] = VD_NOT_SUPPORTED_ATTRIBUTE;
			nsduLength = 2;
		}
		break;
		}

		/* Transmit response message */

		nlde_data_request(PairingRef, PROFILE_ID_ZRC, VendorId,
				nsduLength, nsdu,
				TXO_UNICAST | TXO_DST_ADDR_NET | TXO_ACK_REQ | TXO_SEC_REQ | TXO_MULTI_CH | TXO_CH_NOT_SPEC | TXO_VEND_SPEC,
				nsduHandle,
				(FUNC_PTR)vendor_data_confirm
				);
		/* Keep compiler happy */
		RxLinkQuality = RxLinkQuality;
		RxFlags = RxFlags;
	}
}

#endif  /* #ifndef RF4CE_TARGET */

#ifndef RF4CE_TARGET
void vendor_data_confirm(nwk_enum_t Status, uint8_t PairingRef,
		profile_id_t ProfileId,
		uint8_t Handle
		)
{
#ifdef FLASH_SUPPORT
	if (Handle == 2) {
		/* This is the confirm for SWAP_RSP
		 * so initiate flash swap for activiting the new image */
		flash_swap(IMAGE_START_ADDR, IMAGE_SIZE);
	}
#endif
	Status = Status;
	PairingRef = PairingRef;
	Handle = Handle;
	ProfileId = ProfileId;
}

#endif
static uint16_t get_batmon_voltage(void)
{
	uint16_t voltage;
	/* GET_BATTERY_VOLTAGE - will be board specific */
	/* #if (!defined SAMD20) || (!defined SAMD21) || (!defined SAMR21) */
	voltage = GET_BATTERY_VOLTAGE();

	/*#else
	 * voltage = 0;
	 #endif*/

	return voltage;
}

#endif  /* #ifdef VENDOR_DATA */
