extern unsigned short crc_ccitt_table[256];

#define update_crc(crc,c) crc= (crc>> 8)^crc_ccitt_table[(crc^(c))&0xff];
