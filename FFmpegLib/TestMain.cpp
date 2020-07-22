#include "TestMain.h"


FFmpegLib fflib;

char file_out_path[] = "../out/result_file.flv";

int width_output = 0;//等于0 默认为输出宽度
int heigth_output = 0;//等于0 默认为输出高度
int fps_output = 0;//等于0 默认为输入设备的最大帧率 此值只能小于等于设备最大帧率

int out_sample_rate = 44100;//音频采样

AVFormatContext* formatContext_output;
AVCodecContext* encodecContext_output;

int count_frame = 30 * 10 * 1;

int main()
{
    /// <summary>
    /// 输入部分
    /// </summary>
    std::vector<TDeviceName> videoDevices;
    std::vector<TDeviceName> audioDevices;

    HRESULT resultv = fflib.FFmpegLib::GetAudioVideoInputDevices(videoDevices, CLSID_VideoInputDeviceCategory);
    HRESULT resulta = fflib.FFmpegLib::GetAudioVideoInputDevices(audioDevices, CLSID_AudioInputDeviceCategory);

    
    if (resultv == S_OK)
        for (TDeviceName& item : videoDevices)
        {
            char* vedioname= new char[128];
            snprintf(vedioname, 128, "video=%s", fflib.WCharToChar(item.FriendlyName));
            item.name = vedioname;
        }

    if (resulta == S_OK)
        for (TDeviceName& item : audioDevices)
        {
            char audioname[128] = { '\0' };
            snprintf(audioname, 128, "audio=%s", fflib.WCharToChar(item.FriendlyName));
            item.name = audioname;
        }

    ///输出部分
    formatContext_output = fflib.CreateFormatOutput("flv", file_out_path);

    std::thread thread_video([videoDevices]() {
        startCamera(videoDevices[1].name);
    });

    //std::thread thread_audio([audioDevices]() {
    //    startMicrophone(audioDevices[0].name);
    //});


    getchar();
}


void startCamera(const char* vdevice_in_url) {
    AVFormatContext* formatContext_vinput = fflib.OpenCamera(vdevice_in_url);

    AVStream* stream_in = formatContext_vinput->streams[0];

    if (width_output == 0)
        width_output = stream_in->codecpar->width;

    if (heigth_output == 0)
        heigth_output = stream_in->codecpar->height;

    int frame_rate = stream_in->avg_frame_rate.num / stream_in->avg_frame_rate.den;//每秒多少帧
    std::cout << "设备帧率:" << frame_rate << std::endl;

    AVCodecContext* codecContext_input = fflib.CreateDecodec(stream_in->codecpar);

    AVFrame* avFrame_in = av_frame_alloc();
    AVFrame* avFrame_out = av_frame_alloc();
    SwsContext* swsContext = fflib.CreateSwsContext(codecContext_input, avFrame_out, width_output, heigth_output);
    AVPacket* avpkt_in = av_packet_alloc();
    AVPacket* avpkt_out = av_packet_alloc();


    AVCodecContext* vEncodecContext = fflib.CreateVideoEncodec(AV_CODEC_ID_H264, width_output, heigth_output);

    AVStream* vStream_output = fflib.CreateStream(formatContext_output, vEncodecContext);

    fflib.OpenOutputIO(formatContext_output, file_out_path);

    //30帧 分钟

    //long long start = av_gettime();
    for (size_t pts = 0; pts < count_frame; pts++)
    {
        fflib.ReadFrame(formatContext_vinput, avpkt_in);
        int ret = fflib.DecodecFrame(codecContext_input, avpkt_in, avFrame_in);
        av_packet_unref(avpkt_in);
        if (ret == 0)
        {
            int  sws = fflib.StartSws(swsContext, avFrame_in, avFrame_out);

            avFrame_out->pts = vStream_output->time_base.den / frame_rate * pts;

            int result = fflib.EncodecFrame(formatContext_output, vEncodecContext, avpkt_out, avFrame_out);
            if (result) {
                //pts--;
                //std::cout << "编码失败:" << result << std::endl;
            }
        }
    }

    av_write_trailer(formatContext_output);

    av_frame_free(&avFrame_in);
    av_frame_free(&avFrame_out);
    sws_freeContext(swsContext);
    avcodec_free_context(&codecContext_input);
    avformat_close_input(&formatContext_vinput);

    //std::cout <<"size:"<< deFrame->linesize[0] << std::endl;
}

void startMicrophone(const char* adevice_in_url) {
    AVFormatContext* formatContext_input = fflib.OpenMicrophone(adevice_in_url);

    AVStream* stream_input = formatContext_input->streams[0];

    AVCodecContext* codecContext_input = fflib.CreateDecodec(stream_input->codecpar);

    AVCodecContext* encodecContext = fflib.CreateAudioEncodec(AV_CODEC_ID_AAC, out_sample_rate);

    AVStream* stream_output = fflib.CreateStream(formatContext_output, encodecContext);

    fflib.OpenOutputIO(formatContext_output, file_out_path);

    AVFrame* avFrame_in = av_frame_alloc();
    AVFrame* avFrame_out = av_frame_alloc();
    AVPacket* avpkt_in = av_packet_alloc();
    AVPacket* avpkt_out = av_packet_alloc();


    AVFilterContext* buffer_src_ctx = NULL;
    AVFilterContext* buffer_sink_ctx = NULL;
    char args[512];
    sprintf_s(args, sizeof(args),
        "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%I64x",
        stream_input->time_base.num,
        stream_input->time_base.den,
        stream_input->codecpar->sample_rate,
        av_get_sample_fmt_name((AVSampleFormat)stream_input->codecpar->format),
        av_get_default_channel_layout(codecContext_input->channels));

    fflib.initAudioFilter(args, out_sample_rate, buffer_src_ctx, buffer_sink_ctx);


    for (size_t pts = 0; pts < count_frame; pts++)
    {
        fflib.ReadFrame(formatContext_input, avpkt_in);
        int ret = fflib.DecodecFrame(codecContext_input, avpkt_in, avFrame_in);
        av_packet_unref(avpkt_in);
        if (ret == 0)
        {

            //转换数据格式
            int ret = av_buffersrc_add_frame_flags(buffer_src_ctx, avFrame_in, AV_BUFFERSRC_FLAG_PUSH);
            if (ret < 0)
            {
                //XError(ret);
            }

            ret = av_buffersink_get_frame_flags(buffer_sink_ctx, avFrame_out, AV_BUFFERSINK_FLAG_NO_REQUEST);
            if (ret < 0)
            {
                continue;
            }

            int frame_rate = out_sample_rate/ avFrame_out->nb_samples;

            avFrame_out->pts = stream_output->time_base.den / frame_rate * pts;

            //avFrame_out->pts = av_rescale_q(avFrame_out->nb_samples * pts, {1,44100}, stream_output->time_base);

            //std::cout << "音频pts:" << avFrame_out->pts << std::endl;

            int result = fflib.EncodecFrame(formatContext_output, encodecContext, avpkt_out, avFrame_out);
            if (result) {
                //pts--;
                //std::cout << "编码失败:" << result << std::endl;
            }
        }
    }

    av_write_trailer(formatContext_output);

    av_frame_free(&avFrame_in);
    av_frame_free(&avFrame_out);
    avcodec_free_context(&codecContext_input);
    avformat_close_input(&formatContext_input);
}
