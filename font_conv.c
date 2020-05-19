
	/*"A",33*/
   /* 
	0x00   0000 0000
	0x40,  0100 0000
	0x07,  0000 0111
	0xC0,  1100 0000
	0x39,  0011 1001
	0x00,  0000 0000
	0x0F,  0000 1111
	0x00,  0000 0000
	0x01,  0000 0001
	0xC0,  1100 0000
	0x00,  0000 0000
	0x40,  0100 0000
*/

#include <stdio.h>
#include <stdlib.h>
#include "ssd1331.h"

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c %c%c%c%c %c%c%c%c %c%c%c%c"

#define BYTE_TO_BINARY(byte)  \
(byte & 0x8000 ? '1' : '0'), \
(byte & 0x4000 ? '1' : '0'), \
(byte & 0x2000 ? '1' : '0'), \
(byte & 0x1000 ? '1' : '0'), \
(byte & 0x0800 ? '1' : '0'), \
(byte & 0x0400 ? '1' : '0'), \
(byte & 0x0200 ? '1' : '0'), \
(byte & 0x0100 ? '1' : '0'), \
(byte & 0x0080 ? '1' : '0'), \
(byte & 0x0040 ? '1' : '0'), \
(byte & 0x0020 ? '1' : '0'), \
(byte & 0x0010 ? '1' : '0'), \
(byte & 0x0008 ? '1' : '0'), \
(byte & 0x0004 ? '1' : '0'), \
(byte & 0x0002 ? '1' : '0'), \
(byte & 0x0001 ? '1' : '0')

static void SSD1331_char53(unsigned char x, unsigned char y, char acsii, char size, char mode, unsigned short hwColor) {
    unsigned char i, j, y0=y;
    int temp;
    unsigned char ch = acsii - ' ';
    for(i = 0;i<1;i++) {

		temp = Font0503[ch];
		printf("\n----------------------------------------------------------------\n");
		printf("'%c' => %02X hex => %d dec => %c%c%c%c %c%c%c%c %c%c%c%c %c%c%c%c bin \n", acsii, temp, temp, BYTE_TO_BINARY(temp));
		printf("----------------------------------------------------------------\n");

		for(j =0;j<15;j++)
        {
            if(temp & 0x8000) fprintf (stderr, "(%c%c%c%c %c%c%c%c %c%c%c%c %c%c%c%c & %c%c%c%c %c%c%c%c %c%c%c%c %c%c%c%c) - (%08X & %08X) - x=[%d], y=[%d] -> TRUE \n", BYTE_TO_BINARY(temp), BYTE_TO_BINARY(0x8000), temp, 0x8000, x, y);
            else fprintf (stderr, "(%c%c%c%c %c%c%c%c %c%c%c%c %c%c%c%c & %c%c%c%c %c%c%c%c %c%c%c%c %c%c%c%c) - (%08X & %08X) - x=[%d], y=[%d] -> FALSE \n", BYTE_TO_BINARY(temp), BYTE_TO_BINARY(0x8000), temp, 0x8000, x, y);
            temp <<=1;
            y++;
            if((j+1) % 5==0)
            {
				printf("\n");
                y = y0;
                x ++;
                continue;
            }						
        }
    }
}

void SSD1331_string53(unsigned char x, unsigned char y, const char *pString, unsigned char Size, unsigned char Mode, unsigned short hwColor) {
    while (*pString != '\0') {      

        SSD1331_char53(x, y, *pString, Size, Mode, hwColor);
        x += 4;
        pString ++;
    }
}

int main(int argc, char **argv) 
{
	SSD1331_string53(0,0, "HELLO", 2, 1, 1);
	return 0;
}

