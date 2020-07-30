#pragma once
#include <cstdint>
#include <iostream>
extern "C"
{
#include <libavformat/avformat.h>
}

class ZFrame
{
public:
	ZFrame();
	ZFrame(AVFrame* d, int type);
	~ZFrame();
	void Drop();
	AVFrame* avFrame;
	int codec_type;
};

