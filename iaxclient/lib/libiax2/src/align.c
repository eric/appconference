/*
 * align.c: Copyright Sun Microsystems, by Stephen Uhler
 *
 * Some RISC processors can only dereference integers on 4 byte boundries.
 * This function allows an integer to be dereferenced at an arbitrary
 * character boundary.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: Redistribution of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * Redistribution in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * Neither the name of Sun Microsystems, Inc. or the names of
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission. This software
 * is provided "AS IS," without a warranty of any kind.  ALL EXPRESS OR
 * IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED. SUN MIDROSYSTEMS, INC.
 * ("SUN") AND ITS LICENSORS SHALL NOT BE LIABLE FOR ANY DAMAGES SUFFERED
 * BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING THIS
 * SOFTWARE OR ITS DERIVATIVES. IN NO EVENT WILL SUN OR ITS LICENSORS BE
 * LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA, OR FOR DIRECT, INDIRECT,
 * SPECIAL, CONSEQUENTIAL, INCIDENTAL OR PUNITIVE DAMAGES, HOWEVER CAUSED
 * AND REGARDLESS OF THE THEORY OF LIABILITY, ARISING OUT OF THE USE OF
 * OR INABILITY TO USE THIS SOFTWARE, EVEN IF SUN HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES. You acknowledge that this software is not
 * designed, licensed or intended for use in the design, construction,
 * operation or maintenance of any nuclear facility.
 */

#include "align.h"

unsigned int get_align32(unsigned int *src) {
    unsigned int left=0, right=0;
    int addr = (int) src;
    int offset = addr&3;
    addr &= ~3;
    left = ((unsigned int *)addr)[0];
    if (offset) right = ((int *)addr)[1];
    switch(offset) {
        case 0:
           return left;
        case 1:
           return (left<<8) | (right>>24);
        case 2:
           return (left<<16) | (right>>16);
        case 3:
           return (left<<24) | (right>>8);
    }
    return 0;	/* prevent gcc warning */
}

/* 
 * Access a short on an arbitrary byte boundary
 */

unsigned short get_align16(unsigned short *src) {
    if (((int) src) &1) { /* 12 34 -> 23 */
		src = (unsigned short *)(((int) src) - 1);
		return (src[0] << 8) | (src[1] >>8);
	} else {
		return *src;
	}
}

/*
 * Put a 32 bit quantity into a non-aligned int
 * dst is a possible un-aligned int*
 */

unsigned int set_align32(unsigned int *dst, unsigned int src) {
    int addr = ((int)dst);
    int offset = addr&3;
    unsigned int *dst32 = (int *)(addr &= ~3);
    switch(offset) {
	case 0:
	   dst32[0] = src;
	   break;
	case 1:		/* 1234 5678 -> 1abc d678 */
	   dst32[0] = (dst32[0]&0xFF000000) | (src>>8);
	   dst32[1] = (dst32[1]&0x00FFFFFF) | (src<<24);
	   break;
	case 2:		/* 1234 5678 -> 12ab cd78 */
	   dst32[0] = (dst32[0]&0xFFFF0000) | (src>>16);
	   dst32[1] = (dst32[1]&0x0000FFFF) | (src<<16);
	   break;
	case 3:		/* 1234 5678 -> 123a bcd8 */
	   dst32[0] = (dst32[0]&0xFFFFFF00) | (src>>24);
	   dst32[1] = (dst32[1]&0x000000FF) | (src<<8);
	   break;
    }
    return src;
}
	   
#ifdef ALIGN_MAIN

#include <stdio.h>
int main(int argc, char **argv) {
   int i;
   int result;
   char data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb,
0xc, 0xd};
   fprintf(stderr,"Align test\n");

   for(i=0;i<10;i++) {
       fprintf(stderr, "converting: %d=", i);
       /* result = *((int *)(data + i)); */
       result = align32(data+i);
       fprintf(stderr, "0x%x\n", result);
   }

   return 0;
}
#endif
