#ifndef _PANDORA_CLIENT_H
#define _PANDORA_CLIENT_H

#ifdef __cplusplus
extern "C"
{
#endif

#define PANDORA_CAMERA_UNIT (5)

typedef struct _PandoraPicHeader_s{
	char SOP[2];
	unsigned char pic_id;
	unsigned char type;
	unsigned int width;
	unsigned int height;
	unsigned timestamp;
	unsigned len;
	unsigned int totalLen;
	unsigned int position;
}PandoraPicHeader;

typedef struct _PandoraPic{
	PandoraPicHeader header;
	void* yuv;
}PandoraPic;

#define PANDORA_CLIENT_HEADER_SIZE (28)

typedef int (*CallBack)(void* handle , int cmd , void* param , void* userp);

void* PandoraClientNew(const char* ip , int port , CallBack callback , void* userp);
void PandoraCLientDestroy(void* handle);



#ifdef __cplusplus
}
#endif

#endif