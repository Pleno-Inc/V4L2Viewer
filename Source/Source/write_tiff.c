#include "write_tiff.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// tiff types: short = 3, int = 4
// Tags: ( 2-byte tag ) ( 2-byte type ) ( 4-byte count ) ( 4-byte data )
//    0100 0003 0000 0001 0064 0000
//       |        |    |         |
// tag --+        |    |         |
// short int -----+    |         |
// one value ----------+         |
// value of 100 -----------------+
//
#define TIFF_HDR_NUM_ENTRY 8
#define TIFF_HDR_SIZE 10+TIFF_HDR_NUM_ENTRY*12 
const uint8_t tiff_header[TIFF_HDR_SIZE] = {
	// I     I     42    
	  0x49, 0x49, 0x2a, 0x00,
	// ( offset to tags, 0 )  
	  0x08, 0x00, 0x00, 0x00, 
	// ( num tags )  
	  0x08, 0x00, 
	// ( newsubfiletype, 0 full-image )
	  0xfe, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// ( image width )
	  0x00, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	// ( image height )
	  0x01, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	// ( bits per sample )
	  0x02, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	// ( Photometric Interpretation, 2 = RGB )
	  0x06, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 
	// ( Strip offsets, 8 )
	  0x11, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 
	// ( samples per pixel, 3 - RGB)
	  0x15, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
	// ( Strip byte count )
	  0x17, 0x01, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
};


static uint8_t* write_header(uint8_t * rgb, uint32_t width, uint32_t height, uint16_t bpp)
{
	uint32_t ulTemp=0;
	uint16_t sTemp=0;
	memcpy(rgb, tiff_header, TIFF_HDR_SIZE);

	sTemp = TIFF_HDR_NUM_ENTRY;
	memcpy(rgb + 8, &sTemp, 2);

	memcpy(rgb + 10 + 1*12 + 8, &width, 4);
	memcpy(rgb + 10 + 2*12 + 8, &height, 4);
	memcpy(rgb + 10 + 3*12 + 8, &bpp, 2);

	// strip byte count
	ulTemp = width * height * (bpp / 8) * 3;
	memcpy(rgb + 10 + 7*12 + 8, &ulTemp, 4);

	//strip offset
	sTemp = TIFF_HDR_SIZE;
	memcpy(rgb + 10 + 5*12 + 8, &sTemp, 2);

	return rgb + TIFF_HDR_SIZE;
};


int write_tiff(const char* outputpath, uint8_t* imgbytes, const uint32_t width, const uint32_t height, const int bpp)
{
    FILE* ofile = NULL;

    printf("_alain_ %d\n", __LINE__);
    uint8_t  header[TIFF_HDR_SIZE];
    memcpy((uint8_t* )&header, &tiff_header, sizeof(header));

    printf("_alain_ %d\n", __LINE__);
    (void )write_header(&header, width, height, bpp);

    printf("_alain_ %d\n", __LINE__);
    ofile = fopen(outputpath, "w");
    if (! ofile) {
        return errno;
    }

    size_t nbytes = sizeof(header);
    printf("_alain_ %d, %d\n", __LINE__, (int )nbytes);
    if (fwrite(&header, 1, nbytes, ofile) != nbytes) {
        fclose(ofile);
        return errno;
    }

    nbytes = width*height * (bpp / 8);
    printf("_alain_ %d, %d\n", __LINE__, (int )nbytes);
    if (fwrite(imgbytes, 1, nbytes, ofile) != nbytes) {
        fclose(ofile);
        return errno;
    }

    printf("_alain_ %d\n", __LINE__);
    fclose(ofile);
    printf("_alain_ %d\n", __LINE__);
}
