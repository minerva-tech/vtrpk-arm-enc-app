extern "C" {
//	#include <jbig.h>
	#include <jbig85.h>
};

class jbig85Codec{
    unsigned char* m_pdata;
    unsigned char* m_p;
    size_t m_mem_size;
    size_t m_data_size;
    int  m_err;
    int m_skip_header;

    unsigned char*  m_dec_buffer;
    size_t          m_dec_buf_len;
public:
    jbig85Codec();
    ~jbig85Codec();
    void init(unsigned long w, unsigned long h);
    int encode(unsigned char* picture, unsigned char* frame, size_t frame_len);
    int decode(unsigned char* frame, size_t frame_len, unsigned char* picture);
protected:
    unsigned long m_h;
    unsigned long m_w;

    void on_enc_data(unsigned char *start, size_t len);
    static void enc_cb(unsigned char *start, size_t len, void *pUser);

    void on_dec_data(unsigned char *start, size_t len);
    static int dec_cb(const struct jbg85_dec_state *s, unsigned char *start, size_t len, unsigned long y, void *pUser);

};
