// Copyright (c) 2018 Hajo Kortmann
// License: GNU GPL v2 (see License.txt)

#include "usbdrv.h"
#include "io_stuff.h"

#include <string.h>
#include <avr/interrupt.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <util/crc16.h>

/* 
* Program space distribution:
* 0x0000 - 0x1680  Main Application section
* 0x1680 - 0x1fc0  Bootloader section
* 0x1fc0 - 0x2000  reserved for the main app address storage
*/

// Bootloader position (byte addressing!)
#define BOOTLOADER_ADDR     0x1680
// opcode of RJMP for AVR
#define OP_RJMP             0xC000U
// The jump command to the bootloader consists of the RJMP opcode
// plus the relative address (in words, thus byte address / 2). We subtract 
// 1 from the absolute address to get the relative address (considering the
// current program counter value)
static uint16_t boot_jump = OP_RJMP + BOOTLOADER_ADDR / 2 - 1;

// Reset address of AVR
#define MCU_RESET           0x0000
// Flash page size in bytes for tiny85
#define PAGESIZE       64
// Number of pages used by main application
#define NUM_PAGES_APP   BOOTLOADER_ADDR / PAGESIZE 

// USB host message request types
#define USB_RQ_STATUS       0
#define USB_RQ_FLASH_PAGE   1
#define USB_RQ_CRC          2

#define USB_STATUS_RX_PAGE      0
#define USB_STATUS_RX_CRC       1
#define USB_STATUS_RX_COMPLETE  2
#define USB_STATUS_TX_ERR       0xFE
#define USB_STATUS_CRC_ERROR    0xFF
static volatile uint8_t usb_status_flag = USB_STATUS_RX_PAGE;

USB_PUBLIC uint8_t usbFunctionSetup(uint8_t data[8])
{
    usbMsgPtr = (uint8_t*)&usb_status_flag;
    usbRequest_t *rq = (void*) data;
    
    if (rq->bRequest == USB_RQ_STATUS) {
        return 1;
    } else if (rq->bRequest == USB_RQ_FLASH_PAGE) {
        if (usb_status_flag == USB_STATUS_RX_PAGE) {
            return USB_NO_MSG;
        }
        usb_status_flag = USB_STATUS_TX_ERR;
        return 1;
    } else if (rq->bRequest == USB_RQ_CRC) {
        if (usb_status_flag == USB_STATUS_RX_CRC) {
            return USB_NO_MSG;
        }
        usb_status_flag = USB_STATUS_TX_ERR;
        return 1;        
    }
    
    return 0;
}

static volatile uint16_t page_buffer[PAGESIZE/2];
static volatile uint16_t page_crc;

// Processes the data sent to us by the host
usbMsgLen_t usbFunctionWrite(uint8_t* data, uint8_t len)
{
    static uint8_t recv_bytes = 0;
    if (usb_status_flag == USB_STATUS_RX_PAGE) {
        uint8_t *buf = ((uint8_t*)page_buffer) + recv_bytes;
        memcpy(buf, data, len);
        recv_bytes += len;
        if (recv_bytes == PAGESIZE) {
            usb_status_flag = USB_STATUS_RX_CRC;
            recv_bytes = 0;
            return 1;
        }
        return 0;
    }
    if (usb_status_flag == USB_STATUS_RX_CRC) {
        page_crc = *((uint16_t*)data);
        usb_status_flag = USB_STATUS_RX_COMPLETE;
        return 1;
    }
    return 0;
}

static void fill_boot_page_tmp_buffer(uint16_t addr)
{
    // boot_page_fill writes one word
    for (uint8_t i = 0; i < PAGESIZE / 2; i ++) {
         boot_page_fill(addr, page_buffer[i]);
         addr += 2;
    }
}

// Check the CRC of the written page to the received CRC
static uint8_t check_crc(uint16_t addr)
{
    uint16_t crc = 0xffff;
    for (uint8_t i = 0; i < PAGESIZE; i++) {
        crc = _crc16_update(crc, pgm_read_byte_near(addr+i));
    }
    return crc == page_crc;
}

// write the current flash page buffer to the mcu's flash
static void write_page(uint8_t num_page)
{
    cli();
    uint16_t page_addr = BOOTLOADER_ADDR - num_page * PAGESIZE;
        
    boot_page_erase(page_addr);
    fill_boot_page_tmp_buffer(page_addr);
    boot_page_write(page_addr);
    
    boot_spm_busy_wait();
    
    if (!(check_crc(page_addr))) {
        usb_status_flag = USB_STATUS_CRC_ERROR;
    } 
    else {
        usb_status_flag = USB_STATUS_RX_PAGE;
    }
    
    sei();
}

// insert the jump to the address of the bootloader section at 0x0000
static void insert_bootloader(void)
{
    page_buffer[0] = boot_jump;
}

// insert bootloader isrs after 0x0000
static void insert_bootloader_isr(void)
{
    // this needs to be done whenever the bootloader has been activated
    // to activate the interrupts used by the USB driver
    boot_page_erase(0x0000);
    static const uint8_t isr_vector_num = 15;
    uint16_t cmd;
    for (uint8_t i = 0; i < (isr_vector_num * 2); i += 2) {
        // we use RJMP so we'll have to add the bootloaders start address
        // as an offset
        cmd = pgm_read_word(BOOTLOADER_ADDR + i) + BOOTLOADER_ADDR / 2;
        boot_page_fill(i, cmd);
    }
    boot_page_write(0x0000);
    boot_spm_busy_wait();
}

static void wait_for_next_page(void)
{
    while (usb_status_flag != USB_STATUS_RX_COMPLETE) {
        usbPoll();
        _delay_ms(10);
    }
}

// Address for main application jump storage (we'll use the last page)
#define MAIN_APP_ADDR_SEC   0x1fc0
// Pointer to the main application
void (*main_app_func)(void);

static void start_main_app(void) __attribute__((noreturn));
static void start_main_app(void)
{
    main_app_func = (void*)pgm_read_word(MAIN_APP_ADDR_SEC);
    (*main_app_func)();
    while(1);
}

// extract the main app start address from the received page
static void extract_main_app_func(void)
{
    main_app_func = (void*)(page_buffer[0] - OP_RJMP - 1);
    boot_page_fill(MAIN_APP_ADDR_SEC, *main_app_func);
    // Empty the rest of the buffer
    for(uint8_t i = 2; i < PAGESIZE/2; i += 2) {
        boot_page_fill(MAIN_APP_ADDR_SEC + i, 0);
    }
    boot_page_erase(MAIN_APP_ADDR_SEC);
    boot_page_write(MAIN_APP_ADDR_SEC);
    boot_spm_busy_wait();
}

int main(void)
{
    io_init();
    // Button needs to be pressed for 10 sec at startup to activate bootloader
    // instead of main app
    uint8_t time;
    for (time = 100; time > 0; time --)
    {
        _delay_ms(100);
        if (!(IO_BUTTON_PRESSED)) {
            break;
        }
    }
    
    if (time > 0) {
        // The button hasn't been pressed long enough for a firmware update
        // so we'll just start the main application
        start_main_app();
    }
    
    // The firmware update process starts from here
    
    // Signal that the bootloader is running
    IO_LED_ON;
    
    // write the bootloaders isr jmps into the mcu's isr vector table
    insert_bootloader_isr();
    
    // start usb
    usbInit();
    usbDeviceDisconnect();
    _delay_ms(500);
    usbDeviceConnect();

    sei();
    
    for  (uint8_t i = 1; i < NUM_PAGES_APP; i++)
    {
        wait_for_next_page();
        write_page(i);
    }
    // Wait for the last page (which is the one containing the isr table
    // of the main app)
    wait_for_next_page();
    extract_main_app_func();
    insert_bootloader();
    write_page(NUM_PAGES_APP);
    
    cli();
    // signal that we are done    
    for (uint8_t i = 0; i < 5; i++) {
        IO_LED_OFF;
        _delay_ms(500);
        IO_LED_ON;
        _delay_ms(500);
    }

    return 0;
}
