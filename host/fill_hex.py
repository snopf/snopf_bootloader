#!/usr/bin/env python3

# Copyright (c) 2018 Hajo Kortmann
# License: GNU GPL v2 (see License.txt)

# We fill the data up to this adress with zeros
BOOTLOADER_START = 0x1680

def intel_checksum(line):
    cs = sum([int(line[i*2:i*2+2], 16)
              for i in range(len(line)//2)])
    cs %= 256
    cs *= -1
    cs &= 0xff
    cs = '{:02x}'.format(cs).upper()
    return cs

def get_datatype(line):
    return line[7:9]

def get_datalen(line):
    return int(line[1:3], 16)

def get_addr(line):
    return int(line[3:7], 16)

def zero_line(addr, count=0x10):
    l = ':' + '{:02x}'.format(count) + '{:04x}'.format(addr) + '00'
    l += ''.join(['FF' for i in range(count)])
    l += intel_checksum(l[1:]) + '\r\n'
    return l
    
def fill_hex_to_end(lines):
    last_line = lines[-1]
    
    datalines = [l for l in lines if get_datatype(l) == '00']
    
    max_addr = 0
    for l in datalines:
        max_addr = max(max_addr, get_addr(l) + get_datalen(l))

    offset = BOOTLOADER_START - max_addr
    
    lsize = 0x10
    n, r = divmod(offset, lsize)
    
    new_lines = [zero_line(max_addr + lsize*i, lsize) for i in range(n)]
    
    if r > 0:
        new_lines.append(zero_line(max_addr + lsize * n, r))
        
    lines = lines[:-1] + new_lines
    lines.append(last_line)
    
    return lines
