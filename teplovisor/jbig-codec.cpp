// jtest.cpp : Defines the entry point for the console application.
//
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <jbig-codec.h>

void l18_dump(unsigned char* p){
	printf("L18: ");
	int i = 0;
	int len = 18;
	while ( len-- ){
		i++;
		printf("%02x", (unsigned int) *p);
		p++;
		if ( i % 4 == 0 ){
			printf(" ");
		}
	}
	printf("\n");
}


jbig85Codec::jbig85Codec(){
	m_dec_buffer = 0;
}
jbig85Codec::~jbig85Codec(){
	if ( m_dec_buffer ){
		free(m_dec_buffer);
	}
}
void jbig85Codec::init(unsigned long w, unsigned long h){
	m_h = h;
	m_w = w;
	m_dec_buf_len = m_w*3/8;
	if ( m_dec_buffer ){
		free(m_dec_buffer);
	}
	m_dec_buffer = (unsigned char*) malloc(m_dec_buf_len);
}
int jbig85Codec::encode(unsigned char* picture, unsigned char* frame, size_t frame_len){
	struct jbg85_enc_state enc;
	jbg85_enc_init(&enc, m_w, m_h, enc_cb, this);

	m_pdata = frame;
	m_p = frame;
	m_mem_size = frame_len;
	m_data_size = 0;
	m_err = 0;
	m_skip_header = 1;
	size_t prev = m_w/8;
	size_t prevprev = prev*2;
	for(size_t i=0;i<m_h;i++){
		jbg85_enc_lineout(&enc, picture, picture-prev, picture-prevprev);
		picture += prev;
	}
	if ( m_err ){
		return m_err;
	}
/*	if ( m_data_size >= 18 ){
		const unsigned char buf[] = {
			0xff, 0x02, 0xff, 0x02, 
			0xff, 0x02, 0xff, 0x02, 
			0xff, 0x02, 0xff, 0x02, 
			0xff, 0x02, 0xff, 0x02, 
			0xff, 0x02 };
		if ( memcmp(buf, frame + m_data_size - 18, 18) == 0 ){
			printf("CUT\n");
			return m_data_size - 18;
		}
		l18_dump(frame + m_data_size - 18);
	}*/
	return m_data_size;
}
int jbig85Codec::decode(unsigned char* frame, size_t frame_len, unsigned char* picture){
	{
		unsigned short* p = (unsigned short*) frame;
		bool isEmpty = true;
		for(size_t i=0;i<frame_len/2;i++){
			if ( p[i] != 0x02FF ){
				isEmpty = false;
				break;
			}
		}
		if ( isEmpty ){
			return 0;
		}
	}
	struct jbg85_dec_state dec;
	jbg85_dec_init(&dec, m_dec_buffer, m_dec_buf_len, dec_cb, this);

	m_pdata = picture;
	m_p = picture;
	m_mem_size = m_w*m_h/8;
	m_data_size = 0;
	m_err = 0;
	size_t total_cnt;
	size_t cnt;
	{
		//restore header
		struct jbg85_enc_state enc;
		jbg85_enc_init(&enc, m_w, m_h, enc_cb, this);
		/* prepare BIH */
		unsigned char   buf[20];
		buf[0]  = 0;   /* DL = initial layer to be transmitted */
		buf[1]  = 0;   /* D  = number of differential layers */
		buf[2]  = 1;   /* P  = number of bit planes */
		buf[3]  = 0;
		buf[4]  =  enc.x0 >> 24;
		buf[5]  = (enc.x0 >> 16) & 0xff;
		buf[6]  = (enc.x0 >>  8) & 0xff;
		buf[7]  =  enc.x0        & 0xff;
		buf[8]  =  enc.y0 >> 24;
		buf[9]  = (enc.y0 >> 16) & 0xff;
		buf[10] = (enc.y0 >>  8) & 0xff;
		buf[11] =  enc.y0 & 0xff;
		buf[12] =  enc.l0 >> 24;
		buf[13] = (enc.l0 >> 16) & 0xff;
		buf[14] = (enc.l0 >>  8) & 0xff;
		buf[15] =  enc.l0 & 0xff;
		buf[16] = enc.mx;
		buf[17] = 0;   /* MY = maximum vertical offset allowed for AT pixel */
		buf[18] = 0;   /* order: HITOLO = SEQ = ILEAVE = SMID = 0 */
		buf[19] = enc.options & (JBG_LRLTWO | JBG_VLENGTH | JBG_TPBON);
		jbg85_dec_in(&dec, buf, 20, &cnt);
	}
	jbg85_dec_in(&dec, frame, frame_len, &cnt);
	total_cnt = cnt;
/*	if ( 0 ) {
		const unsigned char buf[] = {
			0xff, 0x02, 0xff, 0x02, 
			0xff, 0x02, 0xff, 0x02, 
			0xff, 0x02, 0xff, 0x02, 
			0xff, 0x02, 0xff, 0x02, 
			0xff, 0x02 };
		jbg85_dec_in(&dec, (unsigned char*) buf, sizeof(buf), &cnt);
	}*/

	if ( JBG_EOK != jbg85_dec_end(&dec) ){
		m_err = -1;
	}

	if ( m_err ){
		return m_err;
	}
	return (int) total_cnt;
}
void jbig85Codec::on_enc_data(unsigned char *start, size_t len){
	if ( m_data_size + len > m_mem_size ){
		m_err = -2;
		return;
	}
	if ( m_skip_header ){
		m_skip_header = 0;
		return;
	}
	memcpy(m_p, start, len);
	m_p += len;
	m_data_size += len;
}
void jbig85Codec::enc_cb(unsigned char *start, size_t len, void *pUser){
	jbig85Codec* pThis = (jbig85Codec*) pUser;
	pThis->on_enc_data(start, len);
}

void jbig85Codec::on_dec_data(unsigned char *start, size_t len){
	if ( m_data_size + len > m_mem_size ){
		m_err = -2;
		return;
	}
	memcpy(m_p, start, len);
	m_p += len;
	m_data_size += len;
}
int jbig85Codec::dec_cb(const struct jbg85_dec_state *s, unsigned char *start, size_t len, unsigned long y, void *pUser){
	jbig85Codec* pThis = (jbig85Codec*) pUser;
	pThis->on_dec_data(start, len);
	return 0;
}
