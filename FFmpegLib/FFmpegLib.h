#pragma once

//extern "C"
//{
//#include <libswscale/swscale.h>
//#include <libavcodec/avcodec.h>
//#include <libavformat/avformat.h>
//#include "libavdevice/avdevice.h"
//#include "libavutil/time.h"
//#include "libavfilter/buffersink.h"
//#include "libavfilter/buffersrc.h"
//#include "libavutil/imgutils.h"
//}
//
//#pragma comment(lib,"swscale.lib")
//#pragma comment(lib,"avformat.lib")
//#pragma comment(lib,"avcodec.lib")
//#pragma comment(lib,"avutil.lib")
//#pragma comment(lib,"avdevice.lib")
//#pragma comment(lib,"avfilter.lib")

#include <iostream>
#include <windows.h>
#include <vector>
#include <dshow.h>
#include <mutex>


extern "C"
{
#include "libavutil/time.h"
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include "libavdevice/avdevice.h"
}

#pragma comment(lib,"avformat.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib,"swresample.lib")
#pragma comment(lib, "Strmiids.lib")
#pragma comment(lib,"avdevice.lib")
#pragma comment(lib,"avfilter.lib")

const int MAX_FRIENDLY_NAME_LENGTH = 128;
const int MAX_MONIKER_NAME_LENGTH = 256;

typedef struct _TDeviceName
{
	WCHAR FriendlyName[MAX_FRIENDLY_NAME_LENGTH];
	WCHAR MonikerName[MAX_MONIKER_NAME_LENGTH];
	char* name;
} TDeviceName;

class FFmpegLib
{
public:
	FFmpegLib();
	~FFmpegLib();

	char* WCharToChar(WCHAR* s);
	HRESULT GetAudioVideoInputDevices(std::vector<TDeviceName>& vectorDevices, REFGUID guidValue);
	AVCodecContext* CreateVideoEncodec(int cid, int width_output, int heigth_output, int frame_rate);
	AVCodecContext* CreateAudioEncodec(int cid, int out_sample_rate);
	int EncodecFrame(AVFormatContext* formatContext_output, AVCodecContext* codecContext_output, AVPacket* avpkt_in, AVPacket* avpkt_out, AVFrame* avFrame_output,AVRational itime);
	//int EncodecVideoFrame(AVFormatContext* formatContext_output, AVCodecContext* codecContext_output, AVPacket* avpkt_out, AVFrame* avFrame_output, size_t pts);

	AVFormatContext* CreateFormatOutput(const char* format_name, const char* file_out_path);
	AVStream* CreateStream(AVFormatContext* formatContext, AVCodecContext* codecContext);
	void OpenOutputIO(AVFormatContext* formatContext, const char* url);

	AVFormatContext* OpenCamera(const char* vdevice_in_url);
	AVFormatContext* OpenMicrophone(const char* adevice_in_url);
	AVCodecContext* CreateDecodec(AVCodecParameters* codecpar);
	AVPacket* ReadFrame(AVFormatContext* formatContext_input, AVPacket* avpkt_in);
	int DecodecFrame(AVCodecContext* codecContext_input, AVPacket* avpkt_in, AVFrame* avFrame_in);
	SwsContext* CreateSwsContext(AVCodecContext* codecContext_input, AVFrame* avFrame, int width_output = 0, int heigth_output = 0);
	int StartSws(SwsContext* swsContext, AVFrame* avFrame_in, AVFrame* avFrame_out);
	void initAudioFilter(char* args, int sample_rate, AVFilterContext*& buffer_src_ctx, AVFilterContext*& buffer_sink_ctx);

private:
	void XError(int errNum, const char tit[]);
	AVCodecContext* CreateEncodec(int cid);
	
};