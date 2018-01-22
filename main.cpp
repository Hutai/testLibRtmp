#include <stdio.h>
#include "librtmp/rtmp_sys.h"
#include "librtmp/log.h"

#pragma comment(lib, "librtmp.lib")
#pragma comment(lib, "WS2_32.lib")

#define BUFSIZE (1024*64)
#define PRINTERROR(res, comparevalue, outputstring) {if(res != comparevalue) printf(outputstring);}

#pragma pack(1)
typedef struct FINT16
{
	unsigned char Byte1;
	unsigned char Byte2;
}fint16;
typedef struct FINT24
{
	unsigned char Byte1;
	unsigned char Byte2;
	unsigned char Byte3;
}fint24;
typedef struct FINT32
{
	unsigned char Byte1;
	unsigned char Byte2;
	unsigned char Byte3;
	unsigned char Byte4;
}fint32;

/*FLV�ļ�ͷ*/
typedef struct FLVHEADER
{
	unsigned char F;//0x46
	unsigned char L;//0x4C
	unsigned char V;//0x56
	unsigned char type;//�汾
	unsigned char info;//����Ϣ |76543210| 0--��Ƶ 2--��Ƶ
	fint32 len;//�ļ�ͷ����9
}FlvHeader;
/*****************************/
/*ǰһ��tag�ĳ���(4�ֽ�)*/
/*****************************/
/*tagͷ*/
typedef struct TAGHEADER
{
	unsigned char type;//0x08--��Ƶ 0x09--��Ƶ 0x12--�ű�
	fint24 datalen;//�������ĳ���
	fint32 timestamp;//ʱ���
	fint24 streamsid;//����Ϣ
}TagHeader;
/*****************************/
/*tag������*/
/*****************************/
/*...........................*/
/*ǰһ��tag�ĳ���(4�ֽ�)*/
/*EOF*/

typedef struct VIDEODATAPRE
{
	unsigned char FrameTypeAndCodecid;//|֡����(4)|��������(4)|  ֡���ͣ�1--sps��pps��I֡��2--����֡  �������ͣ�7--AVC
	unsigned char AVCPacketType;//0--AVC��ͷ 1--AVC NALU
	fint24 CompositionTime;//0
}VideoData;
/*
AVPacketType == 0ʱ�������������01 sps[1] sps[2] sps[3] ff e1 sps_len(2Byte) spsdata 01 pps_len(2Byte) ppsdata
AVPacketType == 1ʱ�������������NALU_size(4Byte) NALUdata
*/

#pragma pack()

#define FINT16TOINT(x) ((x.Byte1<<8 & 0xff00) | (x.Byte2 & 0xff))
#define FINT24TOINT(x) ((x.Byte1<<16 & 0xff0000) | (x.Byte2<<8 & 0xff00) | (x.Byte3 & 0xff))
#define FINT32TOINT(x) ((x.Byte1<<24 & 0xff000000) | (x.Byte2<<16 & 0xff0000) | (x.Byte3<<8 & 0xff00) | (x.Byte4 & 0xff))

int main(int argc, char* argv[])
{
	WORD version;
	WSADATA wsaData;
	version = MAKEWORD(1, 1);
	WSAStartup(version, &wsaData);

	int res = 0;
	RTMP* rtmp =  RTMP_Alloc();
	RTMP_Init(rtmp);
	res = RTMP_SetupURL(rtmp, "rtmp://192.168.34.40/live/test");//����
	//res = RTMP_SetupURL(rtmp, "rtmp://live.hkstv.hk.lxdns.com/live//hks");//����
	PRINTERROR(res, 1, "RTMP_SetupURL error.\n");
	//if unable,the AMF command would be 'play' instead of 'publish'
	RTMP_EnableWrite(rtmp);//����Ҫ����д
	res = RTMP_Connect(rtmp, NULL);
	PRINTERROR(res, 1, "RTMP_Connect error.\n");
	res = RTMP_ConnectStream(rtmp,0);
	PRINTERROR(res, 1, "RTMP_ConnectStream error.\n");

	
	//����
	FILE *fp_push=fopen("save.flv","rb");
	FlvHeader flvheader;
	fread(&flvheader, sizeof(flvheader), 1, fp_push);
	int32_t preTagLen = 0;//ǰһ��Tag����
	fread(&preTagLen, 4, 1, fp_push);
	TagHeader tagHeader;
	uint32_t begintime=RTMP_GetTime(),nowtime,pretimetamp = 0;
	//res = RTMP_Write(rtmp,(char*)&flvheader,sizeof(flvheader));

	while (true)
	{
		fread(&tagHeader, sizeof(tagHeader), 1, fp_push);
		if(tagHeader.type != 0x09)
		{
			int num = FINT24TOINT(tagHeader.datalen);
			fseek(fp_push, FINT24TOINT(tagHeader.datalen)+4, SEEK_CUR);
			continue;
		}
		fseek(fp_push, -sizeof(tagHeader), SEEK_CUR);
		if((nowtime=RTMP_GetTime()-begintime)<pretimetamp)
		{
			printf("%d - %d\n", pretimetamp, nowtime);
			Sleep(pretimetamp-nowtime);
			continue;
		}
		
		char* pFileBuf=(char*)malloc(11+FINT24TOINT(tagHeader.datalen)+4);
		memset(pFileBuf,0,11+FINT24TOINT(tagHeader.datalen)+4);
		if(fread(pFileBuf,1,11+FINT24TOINT(tagHeader.datalen)+4,fp_push)!=11+FINT24TOINT(tagHeader.datalen)+4)
			break;

		if ((res = RTMP_Write(rtmp,pFileBuf,11+FINT24TOINT(tagHeader.datalen)+4)) <= 0)
		{
			printf("RTMP_Write end.\n");
			break;
		}
		pretimetamp = FINT24TOINT(tagHeader.timestamp);

		free(pFileBuf);
		pFileBuf=NULL;
	}
	//��������
	
	
	/*
	//����
	int nRead = 0, NRead = 0;
	int bufsize = 1024*1024;
	char* buf = (char*)malloc(bufsize);
	FILE* fp_save = fopen("save.flv", "wb");
	while(nRead=RTMP_Read(rtmp,buf,bufsize))
	{
		fwrite(buf,1,nRead,fp_save);
		NRead += nRead;
		RTMP_LogPrintf("Receive: %5dByte, Total: %5.2fkB\n",nRead,NRead*1.0/1024);
	}
	//��������
	*/

	RTMP_Close(rtmp);
	RTMP_Free(rtmp);
	WSACleanup();
	return 0;
}