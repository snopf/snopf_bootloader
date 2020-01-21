#!/usr/bin/env python3

# Copyright (c) 2018 Hajo Kortmann
# License: GNU GPL v2 (see License.txt)

from fill_hex import fill_hex_to_end
import sys
import subprocess
import usb.core
import time
import struct
import numpy as np

VENDOR_ID = 0x16c0
PRODUCT_ID = 0x27d8
SERIAL_ID = 'snopf.com:boot'

PAGE_SIZE = 64

class Updater(object):
    
    def __init__(self, device):
        self.device = device
        self.get_pages()
        
    def start_update(self):
        for i, page in enumerate(self.pages):
            print('writing page ' + str(i+1) + ' of ' + str(self.num_pages))
            self.write_page(page)
            
        print('done')
        
    def get_pages(self):
        with open('tmp.bin', 'rb') as f:
            data = f.read()
        if len(data) % PAGE_SIZE != 0:
            raise Exception(
                'Binary must be multiple of page size %d' % PAGE_SIZE)
        
        self.num_pages = len(data) // PAGE_SIZE
        self.pages = [data[i*PAGE_SIZE:i*PAGE_SIZE+PAGE_SIZE]
                      for i in range(self.num_pages)]
        # The updater starts with the first page below the bootloader
        # section, thus we'll have to reverse
        self.pages.reverse()
    
    def update_crc16(self, crc, data):
        crc ^= data
        for i in range(8):
            if (crc & 1):
                crc = (crc >> 1) ^ 0xa001
            else:
                crc = (crc >> 1)
        return crc

    def crc(self, page):
        crc = np.uint16(0xffff)
        for i in range(PAGE_SIZE):
            #crc = self.update_crc16(crc, np.uint8(struct.unpack('B', page[i])[0]))
            crc = self.update_crc16(crc, np.uint8(page[i]))
        return struct.pack('<H', crc)
    
    def get_status(self):
        status = self.device.ctrl_transfer(0xC0, 0, 0, 0, 1)[0]
        if status == 0xFE:
            raise Exception('TX ERROR')
        if status == 0xFF:
            raise Exception('CRC ERROR')
        return status
    
    def check_status(self, wait_status):
        time.sleep(0.1)
        status = self.get_status()
        while status != wait_status:
            print('device status: ' + str(status))
            time.sleep(0.1)
            status = self.get_status()
    
    def write_page(self, page):
        self.check_status(0)           
        sent_bytes = self.device.ctrl_transfer(0x40, 1, 0, 0, page)
        print('Sent %d bytes' % sent_bytes)
        print('page submitted..')
        self.check_status(1)
        self.device.ctrl_transfer(0x40, 2, 0, 0, self.crc(page))
        print('crc submitted..')
        
def create_bin_file_from_hex(fname):
    with open(fname) as f:
        lines = f.readlines()
    lines = fill_hex_to_end(lines)
    with open('tmp.hex', 'w') as f:
        f.writelines(lines)
    subprocess.call(['objcopy', '-I', 'ihex', 'tmp.hex',
                     '-O', 'binary', 'tmp.bin'])

if __name__ == '__main__':
    create_bin_file_from_hex(sys.argv[1])
    
    device = usb.core.find(idVendor=VENDOR_ID, idProduct=PRODUCT_ID,
                           serial_number=SERIAL_ID)
    
    updater = Updater(device)
    updater.start_update()
