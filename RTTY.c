#include "RTTY.h"
#include "main.h"

#define NSS_LOW()  HAL_GPIO_WritePin(NSS_GPIO_Port, NSS_Pin, GPIO_PIN_RESET)
#define NSS_HIGH() HAL_GPIO_WritePin(NSS_GPIO_Port, NSS_Pin, GPIO_PIN_SET)
#define RESET_LOW()  HAL_GPIO_WritePin(RESET_GPIO_Port, RESET_Pin, GPIO_PIN_RESET)
#define RESET_HIGH() HAL_GPIO_WritePin(RESET_GPIO_Port, RESET_Pin, GPIO_PIN_SET)

extern SPI_HandleTypeDef hspi1;

#define CPU_FREQ_MHZ 60

#define BIT_RATE 50
#define BIT_TIME_US 20150

#define FREQ_CENTER 433000170UL    // Center Frequency in Hz (z.B. 433.000170 MHz)
#define FREQ_SHIFT 425              // Shift in Hz (±85 Hz = 170 Hz total)

static void HAL_Delay_Microseconds(volatile uint32_t us) {
    if (!(DWT->CTRL & 1)) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
    uint32_t cycles = (CPU_FREQ_MHZ * us);
    uint32_t start = DWT->CYCCNT;
    while (DWT->CYCCNT - start < cycles);
}

static void SPI_WriteRegister(uint8_t addr, uint8_t val) {
    uint8_t buf[2] = { addr | 0x80, val };
    NSS_LOW();
    HAL_SPI_Transmit(&hspi1, buf, 2, HAL_MAX_DELAY);
    NSS_HIGH();
}

static void SPI_ResetModule(void) {
    RESET_LOW();
    HAL_Delay(1);
    RESET_HIGH();
    HAL_Delay(10);
}

static void SX1278_SetFrequency(uint32_t freq_hz) {
    uint32_t frf = (uint32_t)(freq_hz / 61.03515625); // 32 MHz / 2^19 = 61.035 Hz step
    SPI_WriteRegister(0x06, (frf >> 16) & 0xFF);
    SPI_WriteRegister(0x07, (frf >> 8) & 0xFF);
    SPI_WriteRegister(0x08, frf & 0xFF);
}

static void SX1278_InitFSK(void) {
    SPI_ResetModule();
    SPI_WriteRegister(0x01, 0x01);      // Standby, FSK mode
    SPI_WriteRegister(0x02, 0x2C);      // Bitrate MSB (45.45 baud)
    SPI_WriteRegister(0x03, 0xA0);      // Bitrate LSB
    SPI_WriteRegister(0x04, 0x00);      // Fdev MSB = 0 (we switch freq manually)
    SPI_WriteRegister(0x05, 0x00);      // Fdev LSB
    SPI_WriteRegister(0x09, 0xF8);		// 10 mW TX Power
    SPI_WriteRegister(0x30, 0x00);      // Packet config off
    SX1278_SetFrequency(FREQ_CENTER);   // Set center freq
    SPI_WriteRegister(0x01, 0x03);      // TX mode
	SPI_WriteRegister(0x44, 0x80); 		// fast frequency switching
}

void RTTY_Init() {
	SX1278_InitFSK();
}

void RTTY_TXByte(char c)
{
  int i;
  RTTY_TXBit(0); // Start bit

  for (i=0;i<7;i++) {
    if (c & 1) RTTY_TXBit(1);
    else RTTY_TXBit(0);
    c = c >> 1;
  }

  RTTY_TXBit(1); // Stop bit
  RTTY_TXBit(1); // Stop bit
}

void RTTY_TXString(char * string)
{
  char c;

  c = *string++;

  while ( c != '\0')
  {
    RTTY_TXByte(c);
    c = *string++;
  }
}

void RTTY_TXBit(int bit)
{
  if (bit)
  {
	SX1278_SetFrequency(FREQ_CENTER + FREQ_SHIFT);
  }
  else
  {
	SX1278_SetFrequency(FREQ_CENTER - FREQ_SHIFT);
  }

  HAL_Delay_Microseconds(18800); // 50 baud
  //HAL_Delay_Microseconds(9400); // 100 baud
  //HAL_Delay_Microseconds(4700); // 200 baud
  //HAL_Delay_Microseconds(3133); // 300 baud
}
