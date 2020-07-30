#include "ZFrame.h"

ZFrame::ZFrame()
{
	avFrame = nullptr;
	codec_type = -1;
}

ZFrame::ZFrame(AVFrame* d,int type)
{
	this->avFrame = d;
	codec_type = type;
}

ZFrame::~ZFrame()
{
	//printf("~ZFrame÷¥––¡À!!!!!!!!!!!!!!!!!!!!!!! \n");
}

void ZFrame::Drop()
{
	av_frame_free(&avFrame);
	codec_type = 0;
}
