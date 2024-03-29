/*
* libtcod 1.6.0
* Copyright (c) 2008,2009,2010,2012,2013 Jice & Mingos
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * The name of Jice or Mingos may not be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY JICE AND MINGOS ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL JICE OR MINGOS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "libtcod.h"
#include "libtcod_int.h"

typedef struct {
	TCOD_list_t buffer; /* list<int> */
	uintptr ibuffer; /* byte buffer. bytes are send into buffer 4 by 4 (32 bits OS) or 8 by 8(64 bits OS) */
	int isize; /* number of bytes in ibuffer */
	int bsize; /* number of bytes in buffer */
	int offset; /* current reading position */
} zip_data_t;

TCOD_zip_t TCOD_zip_new() {
	zip_data_t *ret=(zip_data_t *)calloc(sizeof(zip_data_t),1);
	return (TCOD_zip_t)ret;
}

void TCOD_zip_delete(TCOD_zip_t pzip) {
	zip_data_t *zip=(zip_data_t *)pzip;
	if ( zip->buffer != NULL ) TCOD_list_delete(zip->buffer);
	free(zip);
}


/* output interface */
void TCOD_zip_put_char(TCOD_zip_t pzip, char val) {
	zip_data_t *zip=(zip_data_t *)pzip;
	uintptr iv=(uintptr)(uint8)val;
	/* store one byte in ibuffer */
	switch (zip->isize) {
		case 0 : zip->ibuffer|=iv; break;
		case 1 : zip->ibuffer|=(iv<<8); break;
		case 2 : zip->ibuffer|=(iv<<16); break;
		case 3 : zip->ibuffer|=(iv<<24); break;
#ifdef TCOD_64BITS
		/* for 64 bits OS */
		case 4 : zip->ibuffer|=(iv<<32); break;
		case 5 : zip->ibuffer|=(iv<<40); break;
		case 6 : zip->ibuffer|=(iv<<48); break;
		case 7 : zip->ibuffer|=(iv<<56); break;
#endif
	}
	zip->isize++;
	zip->bsize++;
	if (zip->isize == sizeof(uintptr) ) {
		/* ibuffer full. send it to buffer */
		if (!zip->buffer) zip->buffer=TCOD_list_new();
		TCOD_list_push(zip->buffer,(void *)zip->ibuffer);
		zip->isize=zip->ibuffer=0;
	}
}

void TCOD_zip_put_int(TCOD_zip_t pzip, int val) {
#ifndef TCOD_64BITS
	zip_data_t *zip=(zip_data_t *)pzip;
	if ( zip->isize == 0 ) {
		/* the buffer is padded. read 4 bytes */
		if (!zip->buffer) zip->buffer=TCOD_list_new();
		TCOD_list_push(zip->buffer,(void *)val);
		zip->bsize += sizeof(uintptr);
	} else {
#endif
		/* the buffer is not padded. read 4x1 byte */
		TCOD_zip_put_char(pzip,(char)(val&0xFF));
		TCOD_zip_put_char(pzip,(char)((val&0xFF00)>>8));
		TCOD_zip_put_char(pzip,(char)((val&0xFF0000)>>16));
		TCOD_zip_put_char(pzip,(char)((val&0xFF000000)>>24));
#ifndef TCOD_64BITS
	}
#endif
}

void TCOD_zip_put_float(TCOD_zip_t pzip, float val) {
	TCOD_zip_put_int(pzip,* (int *)(&val) );
}

void TCOD_zip_put_string(TCOD_zip_t pzip, const char *val) {
	if (val == NULL) TCOD_zip_put_int(pzip,-1);
	else {
		int l=strlen(val),i;
		TCOD_zip_put_int(pzip,l);
		for (i=0; i <= l; i++) TCOD_zip_put_char(pzip,val[i]);
	}
}

void TCOD_zip_put_data(TCOD_zip_t pzip, int nbBytes, const void *data) {
	if (data == NULL) TCOD_zip_put_int(pzip,-1);
	else {
		char *val=(char *)data;
		int i;
		TCOD_zip_put_int(pzip,nbBytes);
		for (i=0; i< nbBytes; i++) TCOD_zip_put_char(pzip,val[i]);
	}
}

void TCOD_zip_put_color(TCOD_zip_t zip, const TCOD_color_t val) {
	TCOD_zip_put_char(zip,val.r);
	TCOD_zip_put_char(zip,val.g);
	TCOD_zip_put_char(zip,val.b);
}

void TCOD_zip_put_image(TCOD_zip_t zip, const TCOD_image_t val) {
	int w,h,x,y;
	TCOD_image_get_size(val, &w,&h);
	TCOD_zip_put_int(zip,w);
	TCOD_zip_put_int(zip,h);
	for (y=0; y < h; y++) {
		for (x=0; x < w; x++ ) {
			TCOD_zip_put_color(zip,TCOD_image_get_pixel(val,x,y));
		}
	}
}

void TCOD_zip_put_console(TCOD_zip_t zip, const TCOD_console_t val) {
	int w,h,x,y;
	w=TCOD_console_get_width(val);
	h=TCOD_console_get_height(val);
	TCOD_zip_put_int(zip,w);
	TCOD_zip_put_int(zip,h);
	for (y=0; y < h; y++) {
		for (x=0; x < w; x++ ) {
			TCOD_zip_put_char(zip,TCOD_console_get_char(val,x,y));
			TCOD_zip_put_color(zip,TCOD_console_get_char_foreground(val,x,y));
			TCOD_zip_put_color(zip,TCOD_console_get_char_background(val,x,y));
		}
	}
}

int TCOD_zip_save_to_file(TCOD_zip_t pzip, const char *filename) {
	zip_data_t *zip=(zip_data_t *)pzip;
	gzFile f=gzopen(filename,"wb");
	int l=zip->bsize;
	void *buf;
	if (!f) return 0;
	gzwrite(f,&l,sizeof(int));
	if (l==0) {
		gzclose(f);
		return 0;
	}
	if ( zip->isize > 0 ) {
		/* send remaining bytes from ibuffer to buffer */
		if (!zip->buffer) zip->buffer=TCOD_list_new();
		TCOD_list_push(zip->buffer,(void *)zip->ibuffer);
		zip->isize=zip->ibuffer=0;
	}
	buf=(void *)TCOD_list_begin(zip->buffer);
	l=gzwrite(f,buf,l);
	gzclose(f);
	return l;
}


/* input interface */
int TCOD_zip_load_from_file(TCOD_zip_t pzip, const char *filename) {
	zip_data_t *zip=(zip_data_t *)pzip;
	gzFile f=gzopen(filename,"rb");
	int l,lread;
	void *buf;
	int wordsize=sizeof(uintptr);
	if (!f) return 0;
	gzread(f,&l,sizeof(int));
	if (l==0) {
		gzclose(f);
		return 0;
	}
	if ( zip->buffer) {
		TCOD_list_delete(zip->buffer);
		memset(zip,0,sizeof(zip_data_t));
	}
	zip->buffer=TCOD_list_allocate((l+wordsize-1)/wordsize);
	TCOD_list_set_size(zip->buffer,(l+wordsize-1)/wordsize);
	buf=(void *)TCOD_list_begin(zip->buffer);
	lread=gzread(f,buf,l);
	gzclose(f);
	return lread == 0 ? l : lread;
}

char TCOD_zip_get_char(TCOD_zip_t pzip) {
	zip_data_t *zip=(zip_data_t *)pzip;
	char c=0;
	if ( zip->isize == 0 ) {
		/* ibuffer is empty. get 4 or 8 new bytes from buffer */
		zip->ibuffer=(uintptr)TCOD_list_get(zip->buffer,zip->offset);
		zip->offset++;
		zip->isize=sizeof(uintptr);
	}
	/* read one byte from ibuffer */
#ifdef TCOD_64BITS
	switch(zip->isize) {
		case 8: c= zip->ibuffer&0xFFL; break;
		case 7: c= (zip->ibuffer&0xFF00L)>>8; break;
		case 6: c= (zip->ibuffer&0xFF0000L)>>16; break;
		case 5: c= (zip->ibuffer&0xFF000000L)>>24; break;
		case 4: c= (zip->ibuffer&0xFF00000000L)>>32; break;
		case 3: c= (zip->ibuffer&0xFF0000000000L)>>40; break;
		case 2: c= (zip->ibuffer&0xFF000000000000L)>>48; break;
		case 1: c= (zip->ibuffer&0xFF00000000000000L)>>56; break;
	}
#else
	switch(zip->isize) {
		case 4: c= (char)(zip->ibuffer&0xFF); break;
		case 3: c= (char)((zip->ibuffer&0xFF00)>>8); break;
		case 2: c= (char)((zip->ibuffer&0xFF0000)>>16); break;
		case 1: c= (char)((zip->ibuffer&0xFF000000)>>24); break;
	}
#endif
	zip->isize--;
	return c;
}

int TCOD_zip_get_int(TCOD_zip_t pzip) {
#ifndef TCOD_64BITS
	zip_data_t *zip=(zip_data_t *)pzip;
	if ( zip->isize == 0 ) {
		/* buffer is padded. read 4 bytes */
		int i=(int)TCOD_list_get(zip->buffer,zip->offset);
		zip->offset++;
		return i;
	} else {
#endif
		/* buffer is not padded. read 4x 1 byte */
		uint32 i1=(uint32)(uint8)TCOD_zip_get_char(pzip);
		uint32 i2=(uint32)(uint8)TCOD_zip_get_char(pzip);
		uint32 i3=(uint32)(uint8)TCOD_zip_get_char(pzip);
		uint32 i4=(uint32)(uint8)TCOD_zip_get_char(pzip);

		return i1 | (i2<<8) | (i3<<16) | (i4<<24);
#ifndef TCOD_64BITS
	}
#endif
}

float TCOD_zip_get_float(TCOD_zip_t pzip) {
	int i=TCOD_zip_get_int(pzip);
	return *(float *)(&i);
}

TCOD_color_t TCOD_zip_get_color(TCOD_zip_t pzip) {
	TCOD_color_t col;
	col.r=TCOD_zip_get_char(pzip);
	col.g=TCOD_zip_get_char(pzip);
	col.b=TCOD_zip_get_char(pzip);
	return col;
}

const char *TCOD_zip_get_string(TCOD_zip_t pzip) {
	zip_data_t *zip=(zip_data_t *)pzip;
	int l=TCOD_zip_get_int(pzip);
	const char *ret=(const char *)TCOD_list_begin(zip->buffer);
	int boffset; /* offset in bytes */
	if ( l == -1 ) return NULL;
	boffset=zip->offset*sizeof(uintptr)-zip->isize; /* current offset */
	ret += boffset; /* the string address in buffer */
	boffset += l+1; /* new offset */
	/* update ibuffer */
	zip->offset = (boffset+sizeof(uintptr)-1)/sizeof(uintptr);
	zip->isize = boffset%sizeof(uintptr);
	if ( zip->isize != 0 ) {
		zip->isize=sizeof(uintptr)-zip->isize;
		zip->ibuffer=(uintptr)TCOD_list_get(zip->buffer,zip->offset-1);
	}
	return ret;
}

int TCOD_zip_get_data(TCOD_zip_t pzip, int nbBytes, void *data) {
	zip_data_t *zip=(zip_data_t *)pzip;
	int l=TCOD_zip_get_int(pzip),i;
	const char *in=(const char *)TCOD_list_begin(zip->buffer);
	char *out=(char *)data;
	int boffset; /* offset in bytes */
	if ( l == -1 ) return 0;
	boffset=zip->offset*sizeof(uintptr)-zip->isize; /* current offset */
	in += boffset; /* the data address in buffer */
	/* copy it to data */
	for (i=0; i < MIN(l,nbBytes); i++ ) {
		*(out++)=*(in++);
		boffset++;
	}
	/* update ibuffer */
	zip->offset = (boffset+sizeof(uintptr)-1)/sizeof(uintptr);
	zip->isize = boffset%sizeof(uintptr);
	if ( zip->isize != 0 ) {
		zip->isize=sizeof(uintptr)-zip->isize;
		zip->ibuffer=(uintptr)TCOD_list_get(zip->buffer,zip->offset-1);
	}
	return l;
}

TCOD_image_t TCOD_zip_get_image(TCOD_zip_t pzip) {
	TCOD_image_t ret;
	int w,h,x,y;
	w=TCOD_zip_get_int(pzip);
	h=TCOD_zip_get_int(pzip);
	ret=TCOD_image_new(w,h);
	for (y=0; y < h; y++) {
		for (x=0; x < w; x++ ) {
			TCOD_image_put_pixel(ret, x,y,TCOD_zip_get_color(pzip));
		}
	}
	return ret;
}

TCOD_console_t TCOD_zip_get_console(TCOD_zip_t pzip) {
	TCOD_console_t ret;
	int w,h,x,y;
	w=TCOD_zip_get_int(pzip);
	h=TCOD_zip_get_int(pzip);
	ret=TCOD_console_new(w,h);
	for (y=0; y < h; y++) {
		for (x=0; x < w; x++ ) {
			TCOD_console_set_char(ret, x,y,TCOD_zip_get_char(pzip));
			TCOD_console_set_char_foreground(ret, x,y,TCOD_zip_get_color(pzip));
			TCOD_console_set_char_background(ret, x,y,TCOD_zip_get_color(pzip), TCOD_BKGND_SET);
		}
	}
	return ret;
}

uint32 TCOD_zip_get_current_bytes(TCOD_zip_t pzip) {
	zip_data_t *zip=(zip_data_t *)pzip;
	if (!zip->buffer) zip->buffer=TCOD_list_new();
	return TCOD_list_size(zip->buffer)*sizeof(uintptr)+zip->isize;
}

uint32 TCOD_zip_get_remaining_bytes(TCOD_zip_t pzip) {
	zip_data_t *zip=(zip_data_t *)pzip;
	if (!zip->buffer) zip->buffer=TCOD_list_new();
	return (TCOD_list_size(zip->buffer) - zip->offset) * sizeof(uintptr) + zip->isize;
}

void TCOD_zip_skip_bytes(TCOD_zip_t pzip, uint32 nbBytes) {
	zip_data_t *zip=(zip_data_t *)pzip;
	uint32 boffset=zip->offset*sizeof(uintptr)-zip->isize+ nbBytes; /* new offset */
	TCOD_IFNOT(boffset <= TCOD_list_size(zip->buffer)*sizeof(uintptr)) return;
	zip->offset = (boffset+sizeof(uintptr)-1)/sizeof(uintptr);
	zip->isize = boffset%sizeof(uintptr);
	if ( zip->isize != 0 ) {
		zip->isize=sizeof(uintptr)-zip->isize;
		zip->ibuffer=(uintptr)TCOD_list_get(zip->buffer,zip->offset-1);
	}
}
