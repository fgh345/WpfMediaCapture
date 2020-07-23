#include "FFMpegLib.h"

using namespace std;

bool isPre = false;//输出准备完成

FFmpegLib::FFmpegLib()
{
	avdevice_register_all();
	//avformat_network_init();
}

FFmpegLib::~FFmpegLib()
{
}

AVCodecContext* FFmpegLib::CreateEncodec(int cid)
{
	/// 初始化编码器
	AVCodec* codec = avcodec_find_encoder((AVCodecID)cid);
	if (!codec)
	{
		cout << "avcodec_find_encoder  failed!" << endl;
		return NULL;
	}
	//音频编码器上下文
	AVCodecContext* c = avcodec_alloc_context3(codec);
	if (!c)
	{
		cout << "avcodec_alloc_context3  failed!" << endl;
		return NULL;
	}
	c->codec_tag = 0;//表示不用封装器做编码
	c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	//c->thread_count = XGetCpuNum();
	c->time_base = { 1,AV_TIME_BASE };//必须设置 但是又不是最终结果
	return c;
}

AVCodecContext* FFmpegLib::CreateVideoEncodec(int cid,int width_output, int heigth_output, int frame_rate) {
	AVCodecContext* codecContext_output = CreateEncodec(cid);

	codecContext_output->pix_fmt = AV_PIX_FMT_YUV420P;//像素格式
	codecContext_output->width = width_output;//视频宽
	codecContext_output->height = heigth_output;//视频高
	codecContext_output->gop_size = 30;// 设置每gop_size个帧一个关键帧
	codecContext_output->qmin = 1;//决定像素块大小 qmin越大  画面块状越明显
	codecContext_output->qmax = 20;
	//codecContext_output->time_base = {1,frame_rate };
	codecContext_output->framerate = { frame_rate,1};
	codecContext_output->max_b_frames = 0;//设置b帧是p帧的倍数

	AVDictionary* param = 0;
	if (codecContext_output->codec_id == AV_CODEC_ID_H264) {
		av_dict_set(&param, "preset", "superfast", 0);//superfast slow
		av_dict_set(&param, "tune", "zerolatency", 0);
		av_dict_set(&param, "profile", "main", 0);
	}

	if (avcodec_open2(codecContext_output, codecContext_output->codec, &param))
		printf("开启视频编码器失败!");

	av_dict_free(&param);

	return codecContext_output;
}

AVCodecContext* FFmpegLib::CreateAudioEncodec(int cid,int sample_rate) {
	AVCodecContext* codecContext_output = CreateEncodec(cid);

	codecContext_output->sample_fmt = AV_SAMPLE_FMT_FLTP;
	codecContext_output->sample_rate = sample_rate;
	codecContext_output->channel_layout = AV_CH_LAYOUT_STEREO;
	codecContext_output->channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
	//codecContext_output->time_base = { 1,sample_rate };
	//codecContext_output->bit_rate = out_audio_bit_rate;


	if (avcodec_open2(codecContext_output, codecContext_output->codec, NULL))
		printf("开启音频编码器失败!");

	return codecContext_output;
}

AVFormatContext* FFmpegLib::CreateFormatOutput(const char* format_name ,const char* file_out_path) {

	AVFormatContext* formatContext_output;

	int ret = avformat_alloc_output_context2(&formatContext_output, NULL, format_name, file_out_path);

	if (ret) {
		cout << "CreateFormat Failed!" << endl;
	}

	return formatContext_output;
}

int FFmpegLib::EncodecFrame(AVFormatContext* formatContext_output,AVCodecContext* codecContext_output, AVPacket* avpkt_in, AVPacket* avpkt_out, AVFrame* avFrame_output, AVRational itime)
{
	//mutex().lock();

	int ret = avcodec_send_frame(codecContext_output, avFrame_output);
	if (ret) {
		XError(ret,"avcodec_send_frame");
		return ret;
	}

	ret = avcodec_receive_packet(codecContext_output, avpkt_out);
	if (ret) {
		XError(ret,"avcodec_receive_packet");
		av_packet_unref(avpkt_out);
		return ret;
	}

	if (codecContext_output->codec_type == AVMEDIA_TYPE_VIDEO) {
		avpkt_out->stream_index = 0;

		//AVRational itime = ictx->streams[packet.stream_index]->time_base;
		AVRational otime = formatContext_output->streams[avpkt_out->stream_index]->time_base;

		avpkt_out->pts = av_rescale_q_rnd(avpkt_out->pts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		avpkt_out->dts = av_rescale_q_rnd(avpkt_out->dts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		avpkt_out->duration = av_rescale_q_rnd(avpkt_out->duration, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		avpkt_out->pos = -1;


		printf("                                                      @@pts:%d,dts:%d,size:%d,duration:%d\n", avpkt_out->pts, avpkt_out->dts, avpkt_out->size, avpkt_out->duration);
	}else if (codecContext_output->codec_type== AVMEDIA_TYPE_AUDIO)
	{
		avpkt_out->stream_index = 1;

		AVRational otime = formatContext_output->streams[avpkt_out->stream_index]->time_base;

		avpkt_out->pts = av_rescale_q_rnd(avpkt_out->pts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		avpkt_out->dts = av_rescale_q_rnd(avpkt_out->dts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		avpkt_out->duration = av_rescale_q_rnd(avpkt_out->duration, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		avpkt_out->pos = -1;

		printf("##pts:%d,dts:%d,size:%d,duration:%d\n", avpkt_out->pts, avpkt_out->dts, avpkt_out->size, avpkt_out->duration);
	} 

	ret = av_interleaved_write_frame(formatContext_output, avpkt_out);
	if (ret)
	{
		XError(ret, "av_interleaved_write_frame");
	}

	return ret;
}

AVStream* FFmpegLib::CreateStream(AVFormatContext* formatContext, AVCodecContext* codecContext) {

	AVStream* stream = avformat_new_stream(formatContext, codecContext->codec);

	avcodec_parameters_from_context(stream->codecpar, codecContext);

	return stream;
}

void FFmpegLib::OpenOutputIO(AVFormatContext* formatContext, const char* url) {

	if (formatContext->nb_streams == 1)
	{
		while (true)
		{
			if (isPre)
				return;
		}
	}

	//Open output URL
	if (avio_open2(&formatContext->pb, url, AVIO_FLAG_READ_WRITE, NULL, NULL) < 0) {
		printf("Failed to open output file! \n");
	}

	//写入文件头
	avformat_write_header(formatContext, NULL);

	//打印输出流信息
	av_dump_format(formatContext, 0, url, 1);

	for (int i = 0; i < formatContext->nb_streams; i++)
	{
		cout << "stream_timebase-" << i << ":" << formatContext->streams[i]->time_base.den << endl;
	}
	isPre = true;
}

AVFormatContext* FFmpegLib::OpenCamera(const char* vdevice_in_url)
{

	//char szVedioSize[20] = { '\0' };
	//snprintf(szVedioSize, 20, "%dx%d", 1920, 1080);

	AVDictionary* options = NULL;
	av_dict_set_int(&options, "rtbufsize", 3041280 * 20, 0);
	//av_dict_set(&options, "video_size", szVedioSize, 0);
	//av_dict_set(&options, "pixel_format", av_get_pix_fmt_name(AV_PIX_FMT_YUYV422), 0);
	//av_dict_set(&options, "framerate", "30", 0);

	AVFormatContext* formatContext_input = avformat_alloc_context();

	AVInputFormat* fmt = av_find_input_format("dshow");

	int ret = avformat_open_input(&formatContext_input, vdevice_in_url, fmt, &options);

	if (ret)
	{
		printf("avformat_open_input error \n");
	}

	av_dict_free(&options);

	avformat_find_stream_info(formatContext_input, NULL);

	//打印输入流信息
	av_dump_format(formatContext_input, 0, vdevice_in_url, 0);

	return formatContext_input;
}

AVFormatContext* FFmpegLib::OpenMicrophone(const char* adevice_in_url)
{
	AVDictionary* options = NULL;
	av_dict_set_int(&options, "audio_buffer_size", 20, 0);

	AVFormatContext* formatContext_input = avformat_alloc_context();

	AVInputFormat* fmt = av_find_input_format("dshow");

	avformat_open_input(&formatContext_input, adevice_in_url, fmt, &options);

	av_dict_free(&options);

	avformat_find_stream_info(formatContext_input, NULL);

	//打印输入流信息
	av_dump_format(formatContext_input, 0, adevice_in_url, 0);

	return formatContext_input;
}

AVCodecContext* FFmpegLib::CreateDecodec(AVCodecParameters* codecpar)
{
	//处理解码器
	AVCodec* codec_input = avcodec_find_decoder(codecpar->codec_id);
	AVCodecContext* codecContext_input = avcodec_alloc_context3(codec_input);
	avcodec_parameters_to_context(codecContext_input, codecpar);

	if (avcodec_open2(codecContext_input, codec_input, NULL) != 0)
		printf("开启解码器失败!");
	return codecContext_input;
}

AVPacket* FFmpegLib::ReadFrame(AVFormatContext* formatContext_input, AVPacket* avpkt_in) {

	while (true)
	{
		if (av_read_frame(formatContext_input, avpkt_in) == 0)
		{
			if (avpkt_in->size > 0)
			{
				avpkt_in->pts = av_gettime();
				return avpkt_in;
			}
		}
	}
}

int FFmpegLib::DecodecFrame(AVCodecContext* codecContext_input, AVPacket* avpkt_in, AVFrame* avFrame_in)
{
	if (avcodec_send_packet(codecContext_input, avpkt_in) == 0) {
		av_frame_unref(avFrame_in);
		return avcodec_receive_frame(codecContext_input, avFrame_in);
	}
	return  -1;
}

SwsContext* FFmpegLib::CreateSwsContext(AVCodecContext* codecContext_input, AVFrame* avFrame, int width_output, int heigth_output) {

	if (width_output == 0 || heigth_output == 0)
	{
		width_output = codecContext_input->width;
		heigth_output = codecContext_input->height;
	}

	SwsContext* swsContext = sws_getCachedContext(NULL, codecContext_input->width, codecContext_input->height, codecContext_input->pix_fmt,
		width_output, heigth_output, AV_PIX_FMT_YUV420P,
		SWS_BICUBIC, 0, 0, 0);




	avFrame->format = AV_PIX_FMT_YUV420P;
	avFrame->width = width_output;
	avFrame->height = heigth_output;
	avFrame->pts = 0;

	int ret = av_frame_get_buffer(avFrame, 0);
	if (ret)
	{
		cout << "av_frame_get_buffer error!" << endl;
		return nullptr;
	}

	return swsContext;
}

int FFmpegLib::StartSws(SwsContext* swsContext, AVFrame* avFrame_in, AVFrame* avFrame_out)
{
	//转换像素数据格式
	return sws_scale(swsContext,
		avFrame_in->data,
		avFrame_in->linesize,
		0,
		avFrame_in->height,
		avFrame_out->data,
		avFrame_out->linesize);
}

void FFmpegLib::initAudioFilter(char* args, int sample_rate, AVFilterContext*& buffer_src_ctx, AVFilterContext*& buffer_sink_ctx)
{
	//配置过滤器 重新采用音频
	
	const AVFilter* abuffersrc = avfilter_get_by_name("abuffer");
	const AVFilter* abuffersink = avfilter_get_by_name("abuffersink");
	AVFilterInOut* outputs = avfilter_inout_alloc();
	AVFilterInOut* inputs = avfilter_inout_alloc();
	AVFilterGraph* filter_graph = avfilter_graph_alloc();
	filter_graph->nb_threads = 1;

	int ret = avfilter_graph_create_filter(&buffer_src_ctx, abuffersrc, "in",
		args, NULL, filter_graph);
	if (ret)
	{
		printf("avfilter_graph_create_filter error \n");
	}

	ret = avfilter_graph_create_filter(&buffer_sink_ctx, abuffersink, "out",
		NULL, NULL, filter_graph);
	if (ret)
	{
		printf("avfilter_graph_create_filter error \n");
	}

	static const enum AVSampleFormat out_sample_fmts[] = { AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_NONE };
	static const int64_t out_channel_layouts[] = { AV_CH_LAYOUT_STEREO, -1 };
	static const int out_sample_rates[] = { sample_rate , -1 };

	ret = av_opt_set_int_list(buffer_sink_ctx, "sample_fmts", out_sample_fmts, -1,
		AV_OPT_SEARCH_CHILDREN);
	ret = av_opt_set_int_list(buffer_sink_ctx, "channel_layouts", out_channel_layouts, -1,
		AV_OPT_SEARCH_CHILDREN);
	ret = av_opt_set_int_list(buffer_sink_ctx, "sample_rates", out_sample_rates, -1,
		AV_OPT_SEARCH_CHILDREN);

	outputs->name = av_strdup("in");
	outputs->filter_ctx = buffer_src_ctx;;
	outputs->pad_idx = 0;
	outputs->next = NULL;

	inputs->name = av_strdup("out");
	inputs->filter_ctx = buffer_sink_ctx;
	inputs->pad_idx = 0;
	inputs->next = NULL;

	ret = avfilter_graph_parse_ptr(filter_graph, "anull", &inputs, &outputs, nullptr);
	if (ret < 0)
	{
		printf("avfilter_graph_parse_ptr error \n");
	}

	ret = avfilter_graph_config(filter_graph, NULL);
	if (ret)
	{
		printf("avfilter_graph_config error \n");
	}

	av_buffersink_set_frame_size(buffer_sink_ctx, 1024);

}

void FFmpegLib::XError(int errNum,const char tit[])
{
	char buf[1024] = { 0 };
	av_strerror(errNum, buf, sizeof(buf));
	std::cout << tit << ":" << buf << std::endl;
}

char* FFmpegLib::WCharToChar(WCHAR* s)
{
	int w_nlen = WideCharToMultiByte(CP_ACP, 0, s, -1, NULL, 0, NULL, FALSE);
	char* ret = new char[w_nlen];
	memset(ret, 0, w_nlen);
	WideCharToMultiByte(CP_ACP, 0, s, -1, ret, w_nlen, NULL, FALSE);
	return ret;

}

//REFGUID guidValue ： CLSID_AudioInputDeviceCategory（麦克风）；  CLSID_AudioRendererCategory（扬声器）； //CLSID_VideoInputDeviceCategory（摄像头）
HRESULT FFmpegLib::GetAudioVideoInputDevices(std::vector<TDeviceName>& vectorDevices, REFGUID guidValue)
{
	TDeviceName name;
	HRESULT hr;

	// 初始化
	vectorDevices.clear();

	// 初始化COM
	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	if (FAILED(hr))
	{
		return hr;
	}

	// 创建系统设备枚举器实例
	ICreateDevEnum* pSysDevEnum = NULL;
	hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&pSysDevEnum);
	if (FAILED(hr))
	{
		CoUninitialize();
		return hr;
	}

	// 获取设备类枚举器

	IEnumMoniker* pEnumCat = NULL;
	hr = pSysDevEnum->CreateClassEnumerator(guidValue, &pEnumCat, 0);
	if (hr == S_OK)
	{
		// 枚举设备名称
		IMoniker* pMoniker = NULL;
		ULONG cFetched;
		while (pEnumCat->Next(1, &pMoniker, &cFetched) == S_OK)
		{
			IPropertyBag* pPropBag;
			hr = pMoniker->BindToStorage(NULL, NULL, IID_IPropertyBag, (void**)&pPropBag);
			if (SUCCEEDED(hr))
			{
				// 获取设备友好名
				VARIANT varName;
				VariantInit(&varName);
				hr = pPropBag->Read(L"FriendlyName", &varName, NULL);
				if (SUCCEEDED(hr))
				{
					StringCchCopy(name.FriendlyName, MAX_FRIENDLY_NAME_LENGTH, varName.bstrVal);
					// 获取设备Moniker名
					LPOLESTR pOleDisplayName = reinterpret_cast<LPOLESTR>(CoTaskMemAlloc(MAX_MONIKER_NAME_LENGTH * 2));
					if (pOleDisplayName != NULL)
					{
						hr = pMoniker->GetDisplayName(NULL, NULL, &pOleDisplayName);

						if (SUCCEEDED(hr))
						{
							StringCchCopy(name.MonikerName, MAX_MONIKER_NAME_LENGTH, pOleDisplayName);
							vectorDevices.push_back(name);
						}

						CoTaskMemFree(pOleDisplayName);
					}
				}
				VariantClear(&varName);
				pPropBag->Release();
			}

			pMoniker->Release();
		} // End for While

		pEnumCat->Release();
	}

	pSysDevEnum->Release();
	CoUninitialize();
	return hr;
}