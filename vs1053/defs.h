#pragma once

#define HOST        SPI2_HOST

#define PIN_CLK     GPIO_NUM_12
#define PIN_MOSI    GPIO_NUM_14
#define PIN_MISO    GPIO_NUM_27
#define PIN_CS_SDI  GPIO_NUM_26
#define PIN_CS_SD   GPIO_NUM_25
#define PIN_CS_SCI  GPIO_NUM_33
#define PIN_RESET   GPIO_NUM_32
#define PIN_DREQ    GPIO_NUM_35

#define SCI_FREQ         250000
#define SDI_FREQ        8000000

#define CMD_WRITE          0x02
#define CMD_READ           0x03

#define REG_MODE           0x00
#define REG_STATUS         0x01
#define REG_CLOCKF         0x03
#define REG_HDAT0          0x08
#define REG_HDAT1          0x09
#define REG_VOL            0x0B

#define MODE_SDINEW      0x0800
#define MODE_RESET       0x0004

#define FS_NUM                1
#define MAX_OPEN_FILES        1
