/*
 * Copyright 2020 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_flexspi.h"
#include "app.h"
#include "fsl_debug_console.h"
#include "fsl_flexspi_edma.h"
#include "fsl_edma.h"
#if defined(FSL_FEATURE_SOC_DMAMUX_COUNT) && FSL_FEATURE_SOC_DMAMUX_COUNT
#include "fsl_dmamux.h"
#endif

#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "fsl_common.h"

/*****************************************/
/***************UART ****************/

#include "board.h"
#include "fsl_lpuart.h"
#include "fsl_iomuxc.h"
#include "pin_mux.h"
#include "clock_config.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define AUTOMATIC					1u
#define REGULAR						0u
#define INIT						1u
#define HISTORY						0u
#define CONTROL_MODE				REGULAR
#define SEQUENCE_AUTOMATIC			INIT
//#define HISTORY_SEQUENCE			HISTORY

#define LPUART_PC_BOARD            	LPUART1
#define LPUART_BOARD_ARM	   		LPUART4
#define LPUART_BOARD_ARM_BPS	    (9600U)
#define DEMO_LPUART_CLK_FREQ   	 	BOARD_DebugConsoleSrcFreq()
#define DEMO_LPUART_IRQn         	LPUART1_IRQn
#define DEMO_LPUART_IRQHandler   	LPUART1_IRQHandler

/*! @brief Ring buffer size (Unit: Byte). */
#define DEMO_RING_BUFFER_SIZE 	 	16
#define SIZE_BUFFER 		 	 	DEMO_RING_BUFFER_SIZE
/*! @brief Ring buffer to save received data. */

#define MAX_SIZE_ERROR_BUFFER       100
#define MAX_SIZE_INIT_BUFFER		50
#define NB_SERVO 					6
#define LIMIT_HIGH_GRIPPER			2000
#define LIMIT_LOW_GRIPPER			800
#define LIMIT_HIGH_WRIST			2000
#define LIMIT_LOW_WRIST				800
#define LIMIT_HIGH_ELBOW			2000
#define LIMIT_LOW_ELBOW				800
#define LIMIT_HIGH_SHOULDER			2000
#define LIMIT_LOW_SHOULDER			800
#define LIMIT_HIGH_BASE				2000
#define LIMIT_LOW_BASE				800
#define LIMIT_LOW_ROTATION			800
#define LIMIT_HIGH_ROTATION			2000
#define DISTANCE					50
#define DATA_STORE_SIZE				100


#define ERROR_SIG_HIGH				1u
#define ERROR_SIG_LOW				2u
#define ERROR_SIG_KEY				3u
#define NO_ERROR					4u

#define NO_STORE					0u
#define STORE_SPI					1u
#define READ_SPI					2u
typedef enum {
	BASE, SHOULDER, ELBOW, WRIST, GRIPPER, ROTATION
}SERVO_ID;

typedef enum {
	FLAG_INVALIDE_DATA,
	FLAG_VALIDE_DATA,
	FLAG_CONTROLE_DATA,
	NO_DATA,
	NO_DATA_IN_SPI
} flag_receive_data_t;
/*******************************************************************************
 * Prototypes
 ******************************************************************************/
extern status_t flexspi_nor_flash_erase_sector(FLEXSPI_Type *base, uint32_t address);
status_t flexspi_nor_flash_page_program(FLEXSPI_Type *base, uint32_t dstAddr, const uint32_t *src);
status_t flexspi_nor_read_data(FLEXSPI_Type *base, uint32_t startAddress, uint32_t *buffer, uint32_t length);
extern status_t flexspi_nor_get_vendor_id(FLEXSPI_Type *base, uint8_t *vendorId);
extern status_t flexspi_nor_enable_quad_mode(FLEXSPI_Type *base);
extern void flexspi_nor_flash_init(FLEXSPI_Type *base);

/**************************UART******************************/
flag_receive_data_t  mapper_for_ARM(uint8_t c, uint8_t *sequence, uint8_t *sizeSequence);
void convert_pos_toString(uint16_t pos, uint8_t *posBuffer, uint8_t* bufferSize);
void send_info_seq();
uint8_t store_buffer_SPI();
void read_buffer_SPI();
/*******************************************************************************
 * Variables
 ******************************************************************************/
/*Default flexspi+dma driver uses 32-bit data width configuration for transfer,
this requires data buffer address should be aligned to 32-bit. */
AT_NONCACHEABLE_SECTION_ALIGN(static uint8_t s_nor_program_buffer[256], 4);
AT_NONCACHEABLE_SECTION_ALIGN(static uint8_t s_nor_cmd_buffer[50], 4);
static uint8_t s_nor_read_buffer[256];
static uint8_t s_nor_read_cmd_buffer[50];


edma_handle_t dmaTxHandle;
edma_handle_t dmaRxHandle;

uint8_t demoRingBuffer[DEMO_RING_BUFFER_SIZE];
uint8_t sequenceBuffer[DEMO_RING_BUFFER_SIZE];
uint8_t dataStore[DATA_STORE_SIZE][DEMO_RING_BUFFER_SIZE];
uint8_t sizeSequence[DATA_STORE_SIZE];
volatile uint16_t txIndex; /* Index of the data to send out. */
volatile uint16_t rxIndex; /* Index of the memory to save new arrived data. */
uint16_t rxIndex_pred;
volatile uint16_t storeIndex;
uint16_t actual_pos[NB_SERVO];
uint16_t request_pos[NB_SERVO];
uint8_t error_signal 	   = NO_ERROR;
uint8_t store_flash_signal = NO_STORE;



/*******************************************************************************
 * Code
 ******************************************************************************/
void DEMO_LPUART_IRQHandler(void)
{
    uint8_t data;
    uint16_t tmprxIndex = rxIndex;
    uint16_t tmptxIndex = txIndex;


    /* If new data arrived. */
    if ((kLPUART_RxDataRegFullFlag)&LPUART_GetStatusFlags(LPUART_PC_BOARD))
   {
        data = LPUART_ReadByte(LPUART_PC_BOARD);

        /* If ring buffer is not full0, add data to ring buffer. */
        /*if (((tmprxIndex + 1) % DEMO_RING_BUFFER_SIZE) != tmptxIndex)*/
        if ((rxIndex - storeIndex) != DEMO_RING_BUFFER_SIZE)
        {
            demoRingBuffer[(rxIndex % DEMO_RING_BUFFER_SIZE)] = data;
            rxIndex++;
            //rxIndex %= DEMO_RING_BUFFER_SIZE;
            //mapper_for_ARM(data, sequenceBuffer, &sizeARMSequence);
        }
    }
    SDK_ISR_EXIT_BARRIER;
}

/* For write DMA handler depanding on FLEXSPI_TX_DMA_CHANNEL. */
extern void DMA0_DriverIRQHandler(void);
/* For read DMA handler depanding on FLEXSPI_RX_DMA_CHANNEL. */
extern void DMA1_DriverIRQHandler(void);
flexspi_device_config_t deviceconfig = {
    .flexspiRootClk       = 133000000,
    .flashSize            = FLASH_SIZE,
    .CSIntervalUnit       = kFLEXSPI_CsIntervalUnit1SckCycle,
    .CSInterval           = 2,
    .CSHoldTime           = 3,
    .CSSetupTime          = 3,
    .dataValidTime        = 0,
    .columnspace          = 0,
    .enableWordAddress    = 0,
    .AWRSeqIndex          = 0,
    .AWRSeqNumber         = 0,
    .ARDSeqIndex          = NOR_CMD_LUT_SEQ_IDX_READ_FAST_QUAD,
    .ARDSeqNumber         = 1,
    .AHBWriteWaitUnit     = kFLEXSPI_AhbWriteWaitUnit2AhbCycle,
    .AHBWriteWaitInterval = 0,
};

const uint32_t customLUT[CUSTOM_LUT_LENGTH] = {
    /* Normal read mode -SDR */
    /* Normal read mode -SDR */
    [4 * NOR_CMD_LUT_SEQ_IDX_READ_NORMAL] =
        FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x03, kFLEXSPI_Command_RADDR_SDR, kFLEXSPI_1PAD, 0x18),
    [4 * NOR_CMD_LUT_SEQ_IDX_READ_NORMAL + 1] =
        FLEXSPI_LUT_SEQ(kFLEXSPI_Command_READ_SDR, kFLEXSPI_1PAD, 0x04, kFLEXSPI_Command_STOP, kFLEXSPI_1PAD, 0),

    /* Fast read mode - SDR */
    [4 * NOR_CMD_LUT_SEQ_IDX_READ_FAST] =
        FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x0B, kFLEXSPI_Command_RADDR_SDR, kFLEXSPI_1PAD, 0x18),
    [4 * NOR_CMD_LUT_SEQ_IDX_READ_FAST + 1] = FLEXSPI_LUT_SEQ(
        kFLEXSPI_Command_DUMMY_SDR, kFLEXSPI_1PAD, 0x08, kFLEXSPI_Command_READ_SDR, kFLEXSPI_1PAD, 0x04),

    /* Fast read quad mode - SDR */
    [4 * NOR_CMD_LUT_SEQ_IDX_READ_FAST_QUAD] =
        FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0xEB, kFLEXSPI_Command_RADDR_SDR, kFLEXSPI_4PAD, 0x18),
    [4 * NOR_CMD_LUT_SEQ_IDX_READ_FAST_QUAD + 1] = FLEXSPI_LUT_SEQ(
        kFLEXSPI_Command_DUMMY_SDR, kFLEXSPI_4PAD, 0x06, kFLEXSPI_Command_READ_SDR, kFLEXSPI_4PAD, 0x04),

    /* Read extend parameters */
    [4 * NOR_CMD_LUT_SEQ_IDX_READSTATUS] =
        FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x81, kFLEXSPI_Command_READ_SDR, kFLEXSPI_1PAD, 0x04),

    /* Write Enable */
    [4 * NOR_CMD_LUT_SEQ_IDX_WRITEENABLE] =
        FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x06, kFLEXSPI_Command_STOP, kFLEXSPI_1PAD, 0),

    /* Erase Sector  */
    [4 * NOR_CMD_LUT_SEQ_IDX_ERASESECTOR] =
        FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x20, kFLEXSPI_Command_RADDR_SDR, kFLEXSPI_1PAD, 0x18),

    /* Page Program - single mode */
    [4 * NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_SINGLE] =
        FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x02, kFLEXSPI_Command_RADDR_SDR, kFLEXSPI_1PAD, 0x18),
    [4 * NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_SINGLE + 1] =
        FLEXSPI_LUT_SEQ(kFLEXSPI_Command_WRITE_SDR, kFLEXSPI_1PAD, 0x04, kFLEXSPI_Command_STOP, kFLEXSPI_1PAD, 0),

    /* Page Program - quad mode */
    [4 * NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_QUAD] =
        FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x32, kFLEXSPI_Command_RADDR_SDR, kFLEXSPI_1PAD, 0x18),
    [4 * NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_QUAD + 1] =
        FLEXSPI_LUT_SEQ(kFLEXSPI_Command_WRITE_SDR, kFLEXSPI_4PAD, 0x04, kFLEXSPI_Command_STOP, kFLEXSPI_1PAD, 0),

    /* Read ID */
    [4 * NOR_CMD_LUT_SEQ_IDX_READID] =
        FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x9F, kFLEXSPI_Command_READ_SDR, kFLEXSPI_1PAD, 0x04),

    /* Enable Quad mode */
    [4 * NOR_CMD_LUT_SEQ_IDX_WRITESTATUSREG] =
        FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x31, kFLEXSPI_Command_WRITE_SDR, kFLEXSPI_1PAD, 0x04),

    /* Read status register */
    [4 * NOR_CMD_LUT_SEQ_IDX_READSTATUSREG] =
        FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0x05, kFLEXSPI_Command_READ_SDR, kFLEXSPI_1PAD, 0x04),

    /* Erase whole chip */
    [4 * NOR_CMD_LUT_SEQ_IDX_ERASECHIP] =
        FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR, kFLEXSPI_1PAD, 0xC7, kFLEXSPI_Command_STOP, kFLEXSPI_1PAD, 0),
};

void BOARD_InitPins_UART4(void) {
  CLOCK_EnableClock(kCLOCK_Iomuxc);

  IOMUXC_SetPinMux(
		  IOMUXC_GPIO_AD_02_LPUART4_TXD,
		  0U);
  IOMUXC_SetPinMux(
		  IOMUXC_GPIO_AD_01_LPUART4_RXD,
		  0U);

  IOMUXC_SetPinConfig(
		  IOMUXC_GPIO_AD_02_LPUART4_TXD,
		  0x10A0U);

  IOMUXC_SetPinConfig(
		  IOMUXC_GPIO_AD_01_LPUART4_RXD,
		  0x10A0U);
}

// cmd of type #1P1500\r
void initSeq_ARM(void) {
	uint8_t i;
	char temp;
	uint8_t initBuffer[SIZE_BUFFER];

	initBuffer[0] = '#';
	for(i = 0; i < NB_SERVO + 1; i++) {
		temp = i + '0';
		initBuffer[1] = temp; /**convertir en char*/
		initBuffer[2] = 'P';
		initBuffer[3] = '1';
		initBuffer[4] = '5';
		initBuffer[5] = '0';
		initBuffer[6] = '0';
		initBuffer[7] = '\r';
		actual_pos[i] = 1500;
		uint8_t k  = 0;
		rxIndex   = 0;
		txIndex    = 0;
		storeIndex = 0;
		for (k = 0; k < 8 ; k++) {
			while (!(kLPUART_TxDataRegEmptyFlag & LPUART_GetStatusFlags(LPUART_BOARD_ARM)));
			LPUART_WriteByte(LPUART_BOARD_ARM, initBuffer[k]);

			while (!(kLPUART_TxDataRegEmptyFlag & LPUART_GetStatusFlags(LPUART_PC_BOARD)));
			LPUART_WriteByte(LPUART_PC_BOARD, initBuffer[k]);
		}
	}
}

flag_receive_data_t  mapper_for_ARM(uint8_t c, uint8_t *sequence, uint8_t *sizeSequence) {
	uint8_t error_string[MAX_SIZE_ERROR_BUFFER] = "limit ";
	char id_servo;
	uint16_t position;
	uint8_t buffer_pos[16];
	flag_receive_data_t data_flag = FLAG_VALIDE_DATA;

	uint8_t id_sequence = 0;
	switch(c) {
	case 'q': /*BASE LEFT/ or HIGH*/

		request_pos[BASE] = (actual_pos[BASE] - DISTANCE);
		if (request_pos[BASE] > LIMIT_LOW_BASE) {
			actual_pos[BASE] = request_pos[BASE];
			position = actual_pos[BASE];
			id_servo = BASE + '0';
		} else {
			error_signal = ERROR_SIG_LOW;
			data_flag = FLAG_INVALIDE_DATA;
			return data_flag;
		}

		break;
	case 'd':
		request_pos[BASE] = (actual_pos[BASE] + DISTANCE);
		if (request_pos[BASE] < LIMIT_HIGH_BASE) {
			actual_pos[BASE] = request_pos[BASE];
			position = actual_pos[BASE];
			id_servo = BASE + '0';
		} else {
			error_signal = ERROR_SIG_HIGH;
			data_flag = FLAG_INVALIDE_DATA;
			return data_flag;
		}
		break;
	case 'z': /*SHOULDER LEFT/ or HIGH*/
		request_pos[SHOULDER] = (actual_pos[SHOULDER] + DISTANCE);
		if (request_pos[SHOULDER] < LIMIT_HIGH_SHOULDER) {
			actual_pos[SHOULDER] = request_pos[SHOULDER];
			position = actual_pos[SHOULDER];
			id_servo = SHOULDER + '0';
		} else {
			error_signal = ERROR_SIG_HIGH;
			data_flag = FLAG_INVALIDE_DATA;
			return data_flag;
		}
		break;
	case 's': /*SHOULDER LEFT/ or HIGH*/
		request_pos[SHOULDER] = (actual_pos[SHOULDER] - DISTANCE);
		if (request_pos[SHOULDER] > LIMIT_LOW_SHOULDER) {
			actual_pos[SHOULDER] = request_pos[SHOULDER];
			position = actual_pos[SHOULDER];
			id_servo = SHOULDER + '0';
		} else {
			error_signal = ERROR_SIG_LOW;
			data_flag = FLAG_INVALIDE_DATA;
			return data_flag;
		}
		break;
	case 'a':  /*WRIST OPEN */

		request_pos[GRIPPER] = actual_pos[GRIPPER] - DISTANCE;
		if (request_pos[GRIPPER] > LIMIT_LOW_GRIPPER) {
			actual_pos[GRIPPER] = request_pos[GRIPPER];
			position = actual_pos[GRIPPER];
			id_servo = GRIPPER + '0';
		} else {
			error_signal = ERROR_SIG_LOW;
			data_flag = FLAG_INVALIDE_DATA;
			return data_flag;
		}
		break;
	case 'e':		/*WRIST CLOSE */
		request_pos[GRIPPER] = actual_pos[GRIPPER] + DISTANCE;
		if (request_pos[GRIPPER] < LIMIT_HIGH_GRIPPER) {
			actual_pos[GRIPPER]  = request_pos[GRIPPER];
			position = actual_pos[GRIPPER];
			id_servo = GRIPPER + '0';
		} else {
			error_signal = ERROR_SIG_HIGH;
			data_flag = FLAG_INVALIDE_DATA;
			return data_flag;
		}
		break;
	case 'i':
		request_pos[WRIST] = actual_pos[WRIST] + DISTANCE;
		if (request_pos[WRIST] != LIMIT_HIGH_WRIST) {
			actual_pos[WRIST] = request_pos[WRIST];
			position = actual_pos[WRIST];
			id_servo = WRIST + '0';
		} else {
			error_signal = ERROR_SIG_HIGH;
			data_flag = FLAG_INVALIDE_DATA;
			return data_flag;
		}
		break;
	case 'k':
		request_pos[WRIST] = actual_pos[WRIST] - DISTANCE;
		if (request_pos[WRIST] != LIMIT_HIGH_WRIST) {
			actual_pos[WRIST] = request_pos[WRIST];
			position = actual_pos[WRIST];
			id_servo = WRIST + '0';
		} else {
			error_signal = ERROR_SIG_LOW;
			data_flag = FLAG_INVALIDE_DATA;
			return data_flag;
		}
		break;
	case 'r':
			request_pos[ROTATION] = (actual_pos[ROTATION] - DISTANCE);
			if (request_pos[ROTATION] > LIMIT_LOW_ROTATION) {
				actual_pos[ROTATION] = request_pos[ROTATION];
				position = actual_pos[ROTATION];
				id_servo = ROTATION + '0';
			} else {
				error_signal = ERROR_SIG_LOW;
				data_flag = FLAG_INVALIDE_DATA;
				return data_flag;
			}
			break;
	case 't':
			request_pos[ROTATION] = (actual_pos[ROTATION] + DISTANCE);
			if (request_pos[ROTATION] < LIMIT_HIGH_ROTATION) {
				actual_pos[ROTATION] = request_pos[ROTATION];
				position = actual_pos[ROTATION];
				id_servo = ROTATION + '0';
			} else {
				error_signal = ERROR_SIG_HIGH;
				data_flag = FLAG_INVALIDE_DATA;
				return data_flag;
			}
			break;
	case 'j':
		request_pos[ELBOW] = (actual_pos[ELBOW] - DISTANCE);
		if (request_pos[ELBOW] > LIMIT_LOW_ROTATION) {
			actual_pos[ELBOW] = request_pos[ELBOW];
			position = actual_pos[ELBOW];
			id_servo = ELBOW + '0';
		} else {
			error_signal = ERROR_SIG_LOW;
			data_flag = FLAG_INVALIDE_DATA;
			return data_flag;
		}
		break;
		break;
	case 'l':
		request_pos[ELBOW] = (actual_pos[ELBOW] + DISTANCE);
		if (request_pos[ELBOW] < LIMIT_HIGH_ROTATION) {
			actual_pos[ELBOW] = request_pos[ELBOW];
			position = actual_pos[ELBOW];
			id_servo = ELBOW + '0';
		} else {
			error_signal = ERROR_SIG_LOW;
			data_flag = FLAG_INVALIDE_DATA;
			return data_flag;
		}
		break;

	 case '\r':
#if defined(CONTROL_MODE) && (CONTROL_MODE == AUTOMATIC)
#if defined(SEQUENCE_AUTOMATIC) && (SEQUENCE_AUTOMATIC == INIT)
		if (store_flash_signal != STORE_SPI) {
			store_flash_signal = STORE_SPI;
			data_flag          = FLAG_CONTROLE_DATA;
			int index = 0;
			for (int i = 0; i < rxIndex; i++) {
				s_nor_cmd_buffer[i] = demoRingBuffer[i];
				demoRingBuffer[i]   = 0;
				index++;

			}
			// s_nor_cmd_buffer[index++] = '\r';
			 s_nor_cmd_buffer[index]   = rxIndex;
			 store_buffer_SPI();
			 storeIndex = 0;
			 txIndex    = 0;
			 rxIndex    = 0;
		} else {
			initSeq_ARM();
		}

#elif defined(HISTORY_SEQUENCE) && (HISTORY_SEQUENCE == HISTORY)
		data_flag = READ_SPI;
		data_flag = FLAG_CONTROLE_DATA;
#endif /* #if defined(SEQUENCE_AUTOMATIC) && (SEQUENCE_AUTOMATIC == INIT) */

#elif defined(CONTROL_MODE) && (CONTROL_MODE == REGULAR)
		initSeq_ARM();
#endif /* defined(CONTROL_MODE) && (CONTROL_MODE == REGULAR) */
		rxIndex = 0;
		txIndex = 0;
		storeIndex = 0;
	    data_flag = FLAG_CONTROLE_DATA;
	break;
#if defined(CONTROL_MODE) && (CONTROL_MODE == AUTOMATIC)
	 case ' ':

		 if (store_flash_signal == STORE_SPI) {	/* at least a sequence has been save */
			 read_buffer_SPI();
			 rxIndex    = 0;
			 storeIndex = 0;
			 txIndex    = 0;
			 uint8_t index = 0;
			 while((index < DEMO_RING_BUFFER_SIZE) && (s_nor_read_cmd_buffer[index]!= '\r')) {
				 demoRingBuffer[index] = s_nor_read_cmd_buffer[index];
				 index++;
			 }
			 index++;
			 rxIndex = s_nor_read_cmd_buffer[index] - 1;   /* - 1 to delete the enter key pressed to save the sequence into flash mem*/
			 data_flag = FLAG_CONTROLE_DATA;

		 } else {
			 data_flag = NO_DATA_IN_SPI;
		 }
		 break;
#else
#endif /* defined(CONTROL_MODE) && (CONTROL_MODE == AUTOMATIC) */


	default:
		error_signal = ERROR_SIG_KEY;
		data_flag = FLAG_INVALIDE_DATA;
		return data_flag;
	}
	for (int i = 0; i < 16; i++) {
		buffer_pos[i] = 0;
	}
	if (data_flag == FLAG_VALIDE_DATA) {
		sequence[id_sequence++] = '#';
		sequence[id_sequence++] = id_servo;
		sequence[id_sequence++] = 'P';
		uint8_t bufferSize;
		convert_pos_toString(position, buffer_pos, &bufferSize);
	    for(uint8_t i = 0; i < bufferSize; i++) {
	    	sequence[i + id_sequence] = buffer_pos[i];
	    }
	    id_sequence += bufferSize;
	    sequence[id_sequence] = '\r';
	    id_sequence ++;
	    *sizeSequence = id_sequence;
	}
    return data_flag;
}
/*!
 * @brief Convert decimal position (as 1800) into string buffer for send sequence to AL5D
 */
void convert_pos_toString(uint16_t pos, uint8_t *posBuffer, uint8_t* bufferSize) {
	uint16_t temp = pos;
	char tempChar;
	uint8_t idBuffer = 0;
	uint8_t buffSize;
	uint16_t pivot = 1000;
	buffSize = ((pos >= 1000) ? 4 :
					(pos >= 100) ? 3 :
							(pos >= 10) ? 2 :
									(pos > 0) ? 1 : 0);
	*bufferSize = buffSize;
	while ((temp > 0) && (idBuffer < buffSize)) {
		if (temp >= pivot) {
			tempChar = (temp / pivot) + '0';
			posBuffer[idBuffer++] = tempChar;
			temp %= pivot;
			pivot /= 10;
			//idBuffer ++;
			if(temp == 0) {
				for(uint8_t i = idBuffer; i < buffSize; i++) {
					posBuffer[idBuffer++] = '0';
					//idBuffer++;
				}
			}
		} else { /* temp <= pivot */
			pivot /= 10;
			if (idBuffer != 0) {
				posBuffer[idBuffer++] = '0';
			}
		}
	}
}

void save_sequence_dataStore(uint8_t *data, uint8_t dataSize, uint8_t dataStore[DEMO_RING_BUFFER_SIZE][DEMO_RING_BUFFER_SIZE]/*, uint16_t *indexToStore*/) {
	uint16_t tmpstoreIndex = storeIndex;
	for(uint8_t i = 0; i < dataSize; i++) {
		dataStore[tmpstoreIndex][i] = data[i];
	}
	sizeSequence[tmpstoreIndex] = dataSize;
	storeIndex = storeIndex + 1;
	//*indexToStore = *indexToStore+ 1;
	//storeIndex++;
}
void send_info_seq() {

	uint8_t nb_line = 6;
	uint8_t offset  = 0;
	uint8_t *current_seq;
	uint8_t lenght;
	uint8_t sequence_init[nb_line][MAX_SIZE_INIT_BUFFER];
	uint8_t size[6];
	uint8_t* char_spec      = "\n";
	uint8_t* sequence_init1 = "		UART based C server :\r\n";
	uint8_t* sequence_init2 = " please control the robot arm using your keyboard \r\n";
	uint8_t* sequence_init3 = " (a;e) =>  Gripper open / close  \r\n";
	uint8_t* sequence_init4 = " (r;t) =>  Gripper rotation \r\n";
	uint8_t* sequence_init5 = " (q;d) =>  Base rotation \r\n";
	uint8_t* sequence_init6 = " (z;s) =>  Shoulder rotation\r\n";
	uint8_t* sequence_init7 = " (i;k) =>  Wrist rotation \r\n";
	uint8_t* sequence_init8 = " (j;l) =>  Elbow rotation \r\n";
	uint8_t* sequence_init9 = " ENTER =>  memorisation of a sequence \r\n";

	add_on_LPUART_Write_StringNonBlocking(LPUART_PC_BOARD, sequence_init1);
	add_on_LPUART_Write_StringNonBlocking(LPUART_PC_BOARD, sequence_init2);
	add_on_LPUART_Write_StringNonBlocking(LPUART_PC_BOARD, sequence_init3);
	add_on_LPUART_Write_StringNonBlocking(LPUART_PC_BOARD, sequence_init4);
	add_on_LPUART_Write_StringNonBlocking(LPUART_PC_BOARD, sequence_init5);
	add_on_LPUART_Write_StringNonBlocking(LPUART_PC_BOARD, sequence_init6);
	add_on_LPUART_Write_StringNonBlocking(LPUART_PC_BOARD, sequence_init7);
	add_on_LPUART_Write_StringNonBlocking(LPUART_PC_BOARD, sequence_init8);
	add_on_LPUART_Write_StringNonBlocking(LPUART_PC_BOARD, sequence_init8);
}

/*!
 * Error Handler
 *
 */
void error_management(/*uint16_t *rxIndex, uint16_t* storeIndex,
					  uint16_t *txIndex*/) {
	   if (error_signal != NO_ERROR) {
	  /* *storeIndex = 0;
  	   *txIndex    = 0;
  	   *rxIndex    = 0;
  	   */
		   storeIndex = 0;
		   txIndex    = 0;
		   rxIndex    = 0;
	   }
	   uint8_t *error_high_string =  "limit high reached\r\n";
	   uint8_t *error_low_string  =  "limit low reached\r\n";
	   uint8_t * indication_string = "(a;e), (q;d), (z;s), (i;k), \r\n";
	   uint8_t  *error_inv_string  = "invalide key please try the following combinaison: \r\n";
	   switch (error_signal) {
	   case ERROR_SIG_HIGH:
		   add_on_LPUART_Write_StringNonBlocking(LPUART_PC_BOARD, error_high_string);
		   break;
	   case ERROR_SIG_LOW:
		   add_on_LPUART_Write_StringNonBlocking(LPUART_PC_BOARD, error_low_string);
		   break;
	   case ERROR_SIG_KEY:
		   add_on_LPUART_Write_StringNonBlocking(LPUART_PC_BOARD, error_inv_string);
		   add_on_LPUART_Write_StringNonBlocking(LPUART_PC_BOARD, indication_string);
		   break;
	   default: /* Last time error was detected and user did not type a correct info yet so e still wait */
		   break;
	   }
	   /*flag_receive_data_pred = flag_receive_data;*/
}
/*!
 * @brief consumer function : translate the command (in case of accurate one) into sequence for AL5D
 */
uint8_t consumer_fn(uint8_t cmd/*, uint16_t *rxIndex, uint16_t* storeIndex, uint16_t *txIndex*/) {
	uint8_t consumer_flag;
	flag_receive_data_t flag_receive_data = NO_DATA;
	uint8_t sizeARMSequence;
	error_signal = NO_ERROR;
	uint16_t tmprxIndex    = rxIndex;
	uint16_t tmpstoreIndex = storeIndex;
	uint16_t tmptxIndex    = txIndex;

	flag_receive_data_t flag_receive_data_pred;
	uint16_t iter;
    if (tmprxIndex > tmpstoreIndex) { /* Donne reçus et pas encore stockées*/
    	flag_receive_data = mapper_for_ARM(cmd, sequenceBuffer, &sizeARMSequence);                   /* Pour une donne on construit la sequence */
    		if (flag_receive_data == FLAG_VALIDE_DATA) {
    			save_sequence_dataStore(sequenceBuffer, sizeARMSequence, dataStore/*, &storeIndex*/);		  /* Pour une donne reçus on stocke la sequence */
    			flag_receive_data_pred = flag_receive_data;
    		} else if (flag_receive_data == NO_DATA_IN_SPI) {
    			/* FLAG_CONTROL_DATA => storing buffer into flash memory  */
    			uint8_t *no_data = "no data in spi yet please inser data with enter key"; /* Could be insert in error management but it is not realy an error */
    			add_on_LPUART_Write_StringNonBlocking(LPUART_PC_BOARD, no_data);
    			rxIndex    = 0;
    			storeIndex = 0;
    			txIndex    = 0;
    	}
    }
    /*tmprxIndex_pred   = g_HostHidKeyboard.add_on_rxIndex;*/
    if (flag_receive_data == FLAG_VALIDE_DATA) {
        if (tmptxIndex < storeIndex) { 													/* Donne stockés et pas encore envoyés*/
        	/* Transmit to robotic arm */
        	while (!(kLPUART_TxDataRegEmptyFlag & LPUART_GetStatusFlags(LPUART_BOARD_ARM)));
        	LPUART_WriteBlocking(LPUART_BOARD_ARM,dataStore[tmptxIndex] , sizeSequence[tmptxIndex]);/*sizeof(dataStore[tmptxIndex]) / sizeof(dataStore[tmptxIndex][0])*/
        	/* Transmit to debug session  */
        	uint8_t new_line = '\n';
        	while (!(kLPUART_TxDataRegEmptyFlag & LPUART_GetStatusFlags(LPUART_PC_BOARD)));
        	LPUART_WriteBlocking(LPUART_PC_BOARD,&new_line , 1);
        	while (!(kLPUART_TxDataRegEmptyFlag & LPUART_GetStatusFlags(LPUART_PC_BOARD)));
            LPUART_WriteBlocking(LPUART_PC_BOARD,dataStore[tmptxIndex] , sizeSequence[tmptxIndex]);
            txIndex = txIndex + 1;
        }
    } else if (flag_receive_data == FLAG_INVALIDE_DATA) {  /* Incorrect value of data */
    		error_management(/*rxIndex, storeIndex, txIndex*/);
       }
	return flag_receive_data;
}
/*!
 * @brief main function
 *
 */
uint8_t control_arm_AL5D(/*uint16_t *rxIndex , uint16_t* storeIndex, uint16_t *txIndex*/) {
	uint8_t cmd;
	uint8_t main_flag;
	uint8_t new_data;

	if ((new_data) || (rxIndex > storeIndex)) {
		cmd = demoRingBuffer[((storeIndex) % DEMO_RING_BUFFER_SIZE)];
		main_flag = consumer_fn(cmd/*, rxIndex, storeIndex, txIndex*/);
	}
	return main_flag;
}

uint8_t store_buffer_SPI() {
    /* Disable I cache to avoid cache pre-fatch instruction with branch prediction from flash
       and application operate flash synchronously in multi-tasks. */
	uint8_t flag_nor_transfert = -1;
	status_t status;
#if defined(__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
    volatile bool ICacheEnableFlag = false;
    /* Disable I cache. */
    if (SCB_CCR_IC_Msk == (SCB_CCR_IC_Msk & SCB->CCR))
    {
        SCB_DisableICache();
        ICacheEnableFlag = true;
    }
#endif /* __ICACHE_PRESENT */

    status = flexspi_nor_flash_erase_sector(EXAMPLE_FLEXSPI, EXAMPLE_SECTOR * SECTOR_SIZE);

#if defined(__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
    if (ICacheEnableFlag)
    {
        /* Enable I cache. */
        SCB_EnableICache();
        ICacheEnableFlag = false;
    }
#endif /* __ICACHE_PRESENT */

    if (status != kStatus_Success)
    {
        PRINTF("Erase sector failure !\r\n");
        return flag_nor_transfert;
    }

#if defined(CACHE_MAINTAIN) && CACHE_MAINTAIN
    DCACHE_InvalidateByRange(EXAMPLE_FLEXSPI_AMBA_BASE + EXAMPLE_SECTOR * SECTOR_SIZE, FLASH_PAGE_SIZE);
#endif

    memset(s_nor_program_buffer, 0xFFU, sizeof(s_nor_program_buffer));
    memcpy(s_nor_read_buffer, (void *)(EXAMPLE_FLEXSPI_AMBA_BASE + EXAMPLE_SECTOR * SECTOR_SIZE),
           sizeof(s_nor_read_buffer));

    if (memcmp(s_nor_program_buffer, s_nor_read_buffer, sizeof(s_nor_program_buffer)))
    {
        PRINTF("\r\n Erase data -  read out data value incorrect !\r\n ");
        return flag_nor_transfert;
    }
    else
    {
        PRINTF("\r\nErase data - successfully. \r\n");
    }
#if defined(__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
    /* Disable I cache. */
    if (SCB_CCR_IC_Msk == (SCB_CCR_IC_Msk & SCB->CCR))
    {
        SCB_DisableICache();
        ICacheEnableFlag = true;
    }
#endif /* __ICACHE_PRESENT */

    status =
        flexspi_nor_flash_page_program(EXAMPLE_FLEXSPI, EXAMPLE_SECTOR * SECTOR_SIZE, (void *)s_nor_cmd_buffer);

#if defined(__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
    if (ICacheEnableFlag)
    {
        /* Enable I cache. */
        SCB_EnableICache();
        ICacheEnableFlag = false;
    }
#endif /* __ICACHE_PRESENT */

    if (status != kStatus_Success)
    {
        PRINTF("Page program failure !\r\n");
        return flag_nor_transfert;
    }

#if defined(CACHE_MAINTAIN) && CACHE_MAINTAIN
    DCACHE_InvalidateByRange(EXAMPLE_FLEXSPI_AMBA_BASE + EXAMPLE_SECTOR * SECTOR_SIZE, FLASH_PAGE_SIZE);
#endif

    memcpy(s_nor_read_cmd_buffer, (void *)(EXAMPLE_FLEXSPI_AMBA_BASE + EXAMPLE_SECTOR * SECTOR_SIZE),
           sizeof(s_nor_read_cmd_buffer));

    if (memcmp(s_nor_read_cmd_buffer, s_nor_cmd_buffer, sizeof(s_nor_cmd_buffer)) != 0)
    {
        PRINTF("Program data -  read out data value incorrect !\r\n ");
        return flag_nor_transfert;
    }
    else
    {
        PRINTF("Program data - successfully. \r\n");
    }
    flag_nor_transfert = 1;

}
void read_buffer_SPI() {

	#if defined(CACHE_MAINTAIN) && CACHE_MAINTAIN
    DCACHE_InvalidateByRange(EXAMPLE_FLEXSPI_AMBA_BASE + EXAMPLE_SECTOR * SECTOR_SIZE, FLASH_PAGE_SIZE);
#endif

    memcpy(s_nor_read_cmd_buffer, (void *)(EXAMPLE_FLEXSPI_AMBA_BASE + EXAMPLE_SECTOR * SECTOR_SIZE),
           sizeof(s_nor_read_cmd_buffer));

}
/*!
 * @brief Main function
 */
int main(void)
{
    uint32_t i = 0;
    status_t status;
    uint8_t vendorID = 0;
    edma_config_t userConfig;

    BOARD_ConfigMPU();
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();

#if defined(ENABLE_RAM_VECTOR_TABLE)
    /* For write DMA handler depanding on FLEXSPI_TX_DMA_CHANNEL. */
    InstallIRQHandler(DMA0_IRQn, (uint32_t)DMA0_DriverIRQHandler);
    /* For read DMA handler depanding on FLEXSPI_RX_DMA_CHANNEL. */
    InstallIRQHandler(DMA1_IRQn, (uint32_t)DMA1_DriverIRQHandler);
#endif

    PRINTF("\r\nFLEXSPI edma example started!\r\n");

#if defined(FSL_FEATURE_SOC_DMAMUX_COUNT) && FSL_FEATURE_SOC_DMAMUX_COUNT
    /* DMAMUX init */
    DMAMUX_Init(EXAMPLE_FLEXSPI_DMA_MUX);

    DMAMUX_SetSource(EXAMPLE_FLEXSPI_DMA_MUX, FLEXSPI_TX_DMA_CHANNEL, FLEXSPI_TX_DMA_REQUEST_SOURCE);
    DMAMUX_EnableChannel(EXAMPLE_FLEXSPI_DMA_MUX, FLEXSPI_TX_DMA_CHANNEL);

    DMAMUX_SetSource(EXAMPLE_FLEXSPI_DMA_MUX, FLEXSPI_RX_DMA_CHANNEL, FLEXSPI_RX_DMA_REQUEST_SOURCE);
    DMAMUX_EnableChannel(EXAMPLE_FLEXSPI_DMA_MUX, FLEXSPI_RX_DMA_CHANNEL);
#endif

    /* EDMA init */
    /*
     * userConfig.enableRoundRobinArbitration = false;
     * userConfig.enableHaltOnError = true;
     * userConfig.enableContinuousLinkMode = false;
     * userConfig.enableDebugMode = false;
     */
    EDMA_GetDefaultConfig(&userConfig);
    EDMA_Init(EXAMPLE_FLEXSPI_DMA, &userConfig);

    /* Create the EDMA channel handles */
    EDMA_CreateHandle(&dmaTxHandle, EXAMPLE_FLEXSPI_DMA, FLEXSPI_TX_DMA_CHANNEL);
    EDMA_CreateHandle(&dmaRxHandle, EXAMPLE_FLEXSPI_DMA, FLEXSPI_RX_DMA_CHANNEL);
#if defined(FSL_FEATURE_EDMA_HAS_CHANNEL_MUX) && FSL_FEATURE_EDMA_HAS_CHANNEL_MUX
    EDMA_SetChannelMux(EXAMPLE_FLEXSPI_DMA, FLEXSPI_TX_DMA_CHANNEL, FLEXSPI_TX_DMA_REQUEST_SOURCE);
    EDMA_SetChannelMux(EXAMPLE_FLEXSPI_DMA, FLEXSPI_RX_DMA_CHANNEL, FLEXSPI_RX_DMA_REQUEST_SOURCE);
#endif

    /* FLEXSPI init */
    flexspi_nor_flash_init(EXAMPLE_FLEXSPI);

    /* Enter quad mode. */
    status = flexspi_nor_enable_quad_mode(EXAMPLE_FLEXSPI);
    if (status != kStatus_Success)
    {
        return status;
    }

    /* Get vendor ID. */
    status = flexspi_nor_get_vendor_id(EXAMPLE_FLEXSPI, &vendorID);
    if (status != kStatus_Success)
    {
        return status;
    }
    PRINTF("Vendor ID: 0x%x\r\n", vendorID);


    /******************************************************************************************UART  */
    lpuart_config_t config;
    flag_receive_data_t flag_receive_data_pred = 1;
    BOARD_InitPins_UART4();
    LPUART_GetDefaultConfig(&config);
    config.baudRate_Bps = BOARD_DEBUG_UART_BAUDRATE;
    config.enableTx     = true;
    config.enableRx     = true;
    //LPUART_Init(LPUART_PC_BOARD, &config, DEMO_LPUART_CLK_FREQ);
    config.baudRate_Bps = BOARD_DEBUG_UART_BAUDRATE;
    config.enableTx     = true;
    config.enableRx     = true;
    LPUART_Init(LPUART_BOARD_ARM, &config , DEMO_LPUART_CLK_FREQ);
    LPUART_EnableInterrupts(LPUART_PC_BOARD, kLPUART_RxDataRegFullInterruptEnable);
    EnableIRQ(DEMO_LPUART_IRQn);
    initSeq_ARM();
    send_info_seq();
    rxIndex    = 0;
    storeIndex = 0;
    txIndex    = 0;
    while (1)
    {
    	uint8_t flag = control_arm_AL5D(/*&rxIndex , &storeIndex, &txIndex*/);
    }

}
