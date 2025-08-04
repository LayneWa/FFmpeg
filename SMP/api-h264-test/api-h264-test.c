/*
 * Copyright (c) 2015 Ludmila Glinskih
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * H264 codec test.
 */

#include "libavutil/adler32.h"
#include "libavutil/mem.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libavutil/timestamp.h"
#include "libavcodec/evc.h"
#include "libavcodec/bsf.h"

int video_decode_example(const char *input_filename)
{
    const AVCodec *codec = NULL;
    AVCodecContext *ctx= NULL;
    AVCodecParameters *origin_par = NULL;
    AVFrame *fr = NULL;
    uint8_t *byte_buffer = NULL;
    AVPacket *pkt;
    AVFormatContext *fmt_ctx = NULL;
    int number_of_written_bytes;
    int video_stream;
    int byte_buffer_size;
    int i = 0;
    int result;

    result = avformat_open_input(&fmt_ctx, input_filename, NULL, NULL);
    if (result < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open file\n");
        return result;
    }

    result = avformat_find_stream_info(fmt_ctx, NULL);
    if (result < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't get stream info\n");
        return result;
    }

    //video_stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    video_stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream < 0) {
      av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
      return -1;
    }

    origin_par = fmt_ctx->streams[video_stream]->codecpar;

    codec = avcodec_find_decoder(origin_par->codec_id);
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "Can't find decoder\n");
        return -1;
    }

    ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate decoder context\n");
        return AVERROR(ENOMEM);
    }

    result = avcodec_parameters_to_context(ctx, origin_par);
    if (result) {
        av_log(NULL, AV_LOG_ERROR, "Can't copy decoder context\n");
        return result;
    }

    result = avcodec_open2(ctx, codec, NULL);
    if (result < 0) {
        av_log(ctx, AV_LOG_ERROR, "Can't open decoder\n");
        return result;
    }

    fr = av_frame_alloc();
    if (!fr) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate frame\n");
        return AVERROR(ENOMEM);
    }

    pkt = av_packet_alloc();
    if (!pkt) {
        av_log(NULL, AV_LOG_ERROR, "Cannot allocate packet\n");
        return AVERROR(ENOMEM);
    }

    byte_buffer_size = av_image_get_buffer_size(ctx->pix_fmt, ctx->width, ctx->height, 16);
    printf("byte_buffer_size =%d\n", byte_buffer_size);
    byte_buffer = (uint8_t*)av_malloc(byte_buffer_size);
    if (!byte_buffer) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate buffer\n");
        return AVERROR(ENOMEM);
    }

    printf("#tb %d: %d/%d\n", video_stream, fmt_ctx->streams[video_stream]->time_base.num, fmt_ctx->streams[video_stream]->time_base.den);
    i = 0;

    result = 0;
    while (result >= 0) {
        result = av_read_frame(fmt_ctx, pkt);
        if (result >= 0 && pkt->stream_index != video_stream) {
            av_packet_unref(pkt);
            continue;
        }
        printf("pos ,size = %d,%d\n", pkt->pos, pkt->size);
        if (result < 0)
            result = avcodec_send_packet(ctx, NULL);
        else {
            if (pkt->pts == AV_NOPTS_VALUE)
                pkt->pts = pkt->dts = i;
            result = avcodec_send_packet(ctx, pkt);
        }
        av_packet_unref(pkt);

        if (result < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error submitting a packet for decoding\n");
            return result;
        }

        while (result >= 0) {
            result = avcodec_receive_frame(ctx, fr);
            if (result == AVERROR_EOF)
                goto finish;
            else if (result == AVERROR(EAGAIN)) {
                result = 0;
                break;
            } else if (result < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error decoding frame\n");
                return result;
            }
            printf("%d\n", fr->pict_type);
            number_of_written_bytes = av_image_copy_to_buffer(byte_buffer, byte_buffer_size,
                                    (const uint8_t* const *)fr->data, (const int*) fr->linesize,
                                    ctx->pix_fmt, ctx->width, ctx->height, 1);
            if (number_of_written_bytes < 0) {
                av_log(NULL, AV_LOG_ERROR, "Can't copy image to buffer\n");
                av_frame_unref(fr);
                return number_of_written_bytes;
            }
            printf("%d, %s, %s, %8" PRId64", %8d, 0x%08" PRIx32"\n", video_stream,
                   av_ts2str(fr->pts), av_ts2str(fr->pkt_dts), fr->duration,
                   number_of_written_bytes, av_adler32_update(0, (const uint8_t*)byte_buffer, number_of_written_bytes));

            av_frame_unref(fr);
        }
        i++;
    }

finish:
    av_packet_free(&pkt);
    av_frame_free(&fr);
    avformat_close_input(&fmt_ctx);
    avcodec_free_context(&ctx);
    av_freep(&byte_buffer);
    return 0;
}


int video_decode_1078(const char *input_filename)
{
    const AVCodec *codec = NULL;
    AVCodecContext *ctx= NULL;
    AVCodecParameters *origin_par = NULL;
    AVFrame *fr = NULL;
    uint8_t *byte_buffer = NULL;
    AVPacket *pkt;
    AVFormatContext *fmt_ctx = NULL;
    int number_of_written_bytes;
    int video_stream;
    int byte_buffer_size;
    int i = 0;
    int result;

    result = avformat_open_input(&fmt_ctx, input_filename, NULL, NULL);
    if (result < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open file\n");
        return result;
    }

    result = avformat_find_stream_info(fmt_ctx, NULL);
    if (result < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't get stream info\n");
        return result;
    }

    //video_stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    video_stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream < 0) {
      av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
      return -1;
    }

    origin_par = fmt_ctx->streams[video_stream]->codecpar;

    codec = avcodec_find_decoder(origin_par->codec_id);
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "Can't find decoder\n");
        return -1;
    }

    ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate decoder context\n");
        return AVERROR(ENOMEM);
    }

    result = avcodec_parameters_to_context(ctx, origin_par);
    if (result) {
        av_log(NULL, AV_LOG_ERROR, "Can't copy decoder context\n");
        return result;
    }

    result = avcodec_open2(ctx, codec, NULL);
    if (result < 0) {
        av_log(ctx, AV_LOG_ERROR, "Can't open decoder\n");
        return result;
    }

    fr = av_frame_alloc();
    if (!fr) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate frame\n");
        return AVERROR(ENOMEM);
    }

    pkt = av_packet_alloc();
    if (!pkt) {
        av_log(NULL, AV_LOG_ERROR, "Cannot allocate packet\n");
        return AVERROR(ENOMEM);
    }

    byte_buffer_size = av_image_get_buffer_size(ctx->pix_fmt, ctx->width, ctx->height, 16);
    printf("byte_buffer_size =%d\n", byte_buffer_size);
    byte_buffer = (uint8_t*)av_malloc(byte_buffer_size);
    if (!byte_buffer) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate buffer\n");
        return AVERROR(ENOMEM);
    }

    printf("#tb %d: %d/%d\n", video_stream, fmt_ctx->streams[video_stream]->time_base.num, fmt_ctx->streams[video_stream]->time_base.den);
    i = 0;
	FILE *fp = fopen("file.h264", "wb");
	printf("fd=%d\n",fp );

    result = 0;
    while (result >= 0) {
        result = av_read_frame(fmt_ctx, pkt);
        if (result >= 0 && pkt->stream_index != video_stream) {
            av_packet_unref(pkt);
            continue;
        }
        printf("pos ,size = %d,%d\n", pkt->pos, pkt->size);
        //fwrite(pkt->data,1,  pkt->size, fp);
        if (result < 0)
            result = avcodec_send_packet(ctx, NULL);
        else {
            if (pkt->pts == AV_NOPTS_VALUE)
                pkt->pts = pkt->dts = i;
            result = avcodec_send_packet(ctx, pkt);
        }
        av_packet_unref(pkt);

        if (result < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error submitting a packet for decoding\n");
            return result;
        }

        while (result >= 0) {
            result = avcodec_receive_frame(ctx, fr);
            if (result == AVERROR_EOF)
                goto finish;
            else if (result == AVERROR(EAGAIN)) {
                result = 0;
                break;
            } else if (result < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error decoding frame\n");
                return result;
            }
            printf("%d\n", fr->pict_type);
            number_of_written_bytes = av_image_copy_to_buffer(byte_buffer, byte_buffer_size,
                                    (const uint8_t* const *)fr->data, (const int*) fr->linesize,
                                    ctx->pix_fmt, ctx->width, ctx->height, 1);
            if (number_of_written_bytes < 0) {
                av_log(NULL, AV_LOG_ERROR, "Can't copy image to buffer\n");
                av_frame_unref(fr);
                return number_of_written_bytes;
            }
            printf("%d, %s, %s, %8" PRId64", %8d, 0x%08" PRIx32"\n", video_stream,
                   av_ts2str(fr->pts), av_ts2str(fr->pkt_dts), fr->duration,
                   number_of_written_bytes, av_adler32_update(0, (const uint8_t*)byte_buffer, number_of_written_bytes));

            av_frame_unref(fr);
        }
        i++;
    }

finish:
    av_packet_free(&pkt);
    av_frame_free(&fr);
    avformat_close_input(&fmt_ctx);
    avcodec_free_context(&ctx);
    av_freep(&byte_buffer);
	fclose(fp);
    return 0;
}



/* 打印编码器支持该采样率并查找指定采样率下标 */
static int find_sample_rate_index(const AVCodec* codec, int sample_rate)
{
        const int* p = codec->supported_samplerates;
        int sample_rate_index = -1; //支持的分辨率下标
        int count = 0;
        while (p != 0) {// 0作为退出条件，比如libfdk-aacenc.c的aac_sample_rates
                printf("%s 支持采样率: %dhz  对应下标：%d\n", codec->name, *p, count);

                if (*p == sample_rate)
                        sample_rate_index = count;
                p++;
                count++;
        }
        return sample_rate_index;
}


/// <summary>
/// 给aac音频数据添加adts头
/// </summary>
/// <param name="header">adts数组</param>
/// <param name="sample_rate">采样率</param>
/// <param name="channals">通道数</param>
/// <param name="prfile">音频编码器配置文件（FF_PROFILE_AAC_LOW  定义在 avcodec.h）</param>
/// <param name="len">音频包长度</param>
void addHeader(char header[], int sample_rate, int channals, int prfile, int len)
{
        uint8_t sampleIndex = 0;
        switch (sample_rate) {
        case 96000: sampleIndex = 0; break;
        case 88200: sampleIndex = 1; break;
        case 64000: sampleIndex = 2; break;
        case 48000: sampleIndex = 3; break;
        case 44100: sampleIndex = 4; break;
        case 32000: sampleIndex = 5; break;
        case 24000: sampleIndex = 6; break;
        case 22050: sampleIndex = 7; break;
        case 16000: sampleIndex = 8; break;
        case 12000: sampleIndex = 9; break;
        case 11025: sampleIndex = 10; break;
        case 8000: sampleIndex = 11; break;
        case 7350: sampleIndex = 12; break;
        default: sampleIndex = 4; break;
        }

        uint8_t audioType = 2;  //AAC LC

        uint8_t channelConfig = 2;      //双通道

        len += 7;
        //0,1是固定的
        header[0] = (uint8_t)0xff;         //syncword:0xfff                          高8bits
        header[1] = (uint8_t)0xf0;         //syncword:0xfff                          低4bits
        header[1] |= (0 << 3);    //MPEG Version:0 for MPEG-4,1 for MPEG-2  1bit
        header[1] |= (0 << 1);    //Layer:0                                 2bits
        header[1] |= 1;           //protection absent:1                     1bit
        //根据aac类型,采样率,通道数来配置
        header[2] = (audioType - 1) << 6;            //profile:audio_object_type - 1                      2bits
        header[2] |= (sampleIndex & 0x0f) << 2; //sampling frequency index:sampling_frequency_index  4bits
        header[2] |= (0 << 1);                             //private bit:0                                      1bit
        header[2] |= (channelConfig & 0x04) >> 2;           //channel configuration:channel_config

        //根据通道数+数据长度来配置
        header[3] = (channelConfig & 0x03) << 6;     //channel configuration:channel_config      低2bits
        header[3] |= (0 << 5);                      //original：0                               1bit
        header[3] |= (0 << 4);                      //home：0                                   1bit
        header[3] |= (0 << 3);                      //copyright id bit：0                       1bit
        header[3] |= (0 << 2);                      //copyright id start：0                     1bit
        header[3] |= ((len & 0x1800) >> 11);           //frame length：value   高2bits
        //根据数据长度来配置
        header[4] = (uint8_t)((len & 0x7f8) >> 3);     //frame length:value    中间8bits
        header[5] = (uint8_t)((len & 0x7) << 5);       //frame length:value    低3bits
        header[5] |= (uint8_t)0x1f;                    //buffer fullness:0x7ff 高5bits
        header[6] = (uint8_t)0xfc;
}


int split() {
        AVFormatContext* ifmt_ctx = NULL;
        AVPacket pkt;
        int ret;
    	unsigned int i;
        int videoindex = -1, audioindex = -1;
        const char* in_filename = "input.mp4";
        const char* out_filename_v = "test1.h264";
        const char* out_filename_a = "test1.aac";

        if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
                printf("Could not open input file.");
                return -1;
        }

        if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
                printf("Failed to retrieve input stream information");
                return -1;
        }

        videoindex = -1;
        for (i = 0; i < ifmt_ctx->nb_streams; i++) { //nb_streams：视音频流的个数
                if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
                        videoindex = i;
                else if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
                        audioindex = i;
        }

        printf("\nInput Video===========================\n");
        av_dump_format(ifmt_ctx, 0, in_filename, 0);  // 打印信息
        printf("\n======================================\n");

        FILE* fp_audio = fopen(out_filename_a, "wb+");
        FILE* fp_video = fopen(out_filename_v, "wb+");

        AVBSFContext* bsf_ctx = NULL;
        const AVBitStreamFilter* pfilter = av_bsf_get_by_name("h264_mp4toannexb");
        if (pfilter == NULL) {
                printf("Get bsf failed!\n");
        }

        if ((ret = av_bsf_alloc(pfilter, &bsf_ctx)) != 0) {
                printf("Alloc bsf failed!\n");

        }

        ret = avcodec_parameters_copy(bsf_ctx->par_in, ifmt_ctx->streams[videoindex]->codecpar);
        if (ret < 0) {
                printf("Set Codec failed!\n");

        }
        ret = av_bsf_init(bsf_ctx);
        if (ret < 0) {
                printf("Init bsf failed!\n");

        }

    //这里遍历音频编码器打印支持的采样率，并找到当前音频采样率所在的下表，用于后面添
    //本程序并没有使用，只是测试，如果为了程序健壮性可以采用此方式
        const AVCodec* codec = avcodec_find_encoder(ifmt_ctx->streams[audioindex]->codecpar->codec_id);
        int sample_rate_index = find_sample_rate_index(codec, ifmt_ctx->streams[audioindex]->codecpar->sample_rate);
        printf("分辨率数组下表：%d\n", sample_rate_index);

        while (av_read_frame(ifmt_ctx, &pkt) >= 0) {
                if (pkt.stream_index == videoindex) {

                        av_bsf_send_packet(bsf_ctx, &pkt);

                        while (1)
                        {
                                ret = av_bsf_receive_packet(bsf_ctx, &pkt);
                                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                                        break;
                                else if (ret < 0) {
                                        printf("Receive Pkt failed!\n");
                                        break;
                                }

                                printf("Write Video Packet. size:%d\tpts:%ld\n", pkt.size, pkt.pts);

                                fwrite(pkt.data, 1, pkt.size, fp_video);
                        }

                }
                else if (pkt.stream_index == audioindex) {
                        printf("Write Audio Packet. size:%d\tpts:%ld\n", pkt.size, pkt.pts);
                        char adts[7] = { 0 };
                        addHeader(adts, ifmt_ctx->streams[audioindex]->codecpar->sample_rate,
                                ifmt_ctx->streams[audioindex]->codecpar->ch_layout.nb_channels,
                                ifmt_ctx->streams[audioindex]->codecpar->profile,
                                pkt.size);
                        fwrite(adts, 1, 7, fp_audio);
                        fwrite(pkt.data, 1, pkt.size, fp_audio);
                }
                av_packet_unref(&pkt);
        }

        av_bsf_free(&bsf_ctx);


        fclose(fp_video);
        fclose(fp_audio);

        avformat_close_input(&ifmt_ctx);
        return 0;


        if (ifmt_ctx)
                avformat_close_input(&ifmt_ctx);
        if (fp_audio)
                fclose(fp_audio);
        if (fp_video)
                fclose(fp_video);
        if (bsf_ctx)
                av_bsf_free(&bsf_ctx);
        return -1;
}

#define ERROR_STRING_SIZE 1024

#define ADTS_HEADER_LEN  7;

int adts_header(char * const p_adts_header, const int data_length,
                const int profile, const int samplerate,
                const int channels)
{}

int main_1078()
{
    char *in_filename = "input.mp4";
    char *h264_filename = "testm.h264";
    char *aac_filename = "testm.aac";
    FILE *aac_fd = NULL;
    FILE *h264_fd = NULL;

    h264_fd = fopen(h264_filename, "wb");
    if(!h264_fd) {
        printf("fopen %s failed\n", h264_filename);
        return -1;
    }

    aac_fd = fopen(aac_filename, "wb");
    if(!aac_fd) {
        printf("fopen %s failed\n", aac_filename);
        return -1;
    }

    AVFormatContext *ifmt_ctx = NULL;
    int video_index = -1;
    int audio_index = -1;
    AVPacket *pkt = NULL;
    int ret = 0;
    char errors[ERROR_STRING_SIZE+1];  // 主要是用来缓存解析FFmpeg api返回值的错误string

    ifmt_ctx = avformat_alloc_context();
    if(!ifmt_ctx) {
        printf("avformat_alloc_context failed\n");
        //        fclose(aac_fd);
        return -1;
    }

    ret = avformat_open_input(&ifmt_ctx, in_filename, NULL, NULL);
    if(ret < 0) {
        av_strerror(ret, errors, ERROR_STRING_SIZE);
        avformat_close_input(&ifmt_ctx);
        return -1;
    }

    video_index = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if(video_index == -1) {
        avformat_close_input(&ifmt_ctx);
        return -1;
    }

    audio_index = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if(audio_index == -1) {
        avformat_close_input(&ifmt_ctx);
        return -1;
    }

    // h264_mp4toannexb
    const AVBitStreamFilter *bsfilter = av_bsf_get_by_name("h264_mp4toannexb");
    if(!bsfilter) {
        avformat_close_input(&ifmt_ctx);
        return -1;
    }

    AVBSFContext *bsf_ctx = NULL;
    ret = av_bsf_alloc(bsfilter, &bsf_ctx);
    if(ret < 0) {
        av_strerror(ret, errors, ERROR_STRING_SIZE);
        avformat_close_input(&ifmt_ctx);
        return -1;
    }
    ret = avcodec_parameters_copy(bsf_ctx->par_in, ifmt_ctx->streams[video_index]->codecpar);
    if(ret < 0) {
        av_strerror(ret, errors, ERROR_STRING_SIZE);
        avformat_close_input(&ifmt_ctx);
        av_bsf_free(&bsf_ctx);
        return -1;
    }
    ret = av_bsf_init(bsf_ctx);
    if(ret < 0) {
        av_strerror(ret, errors, ERROR_STRING_SIZE);
        avformat_close_input(&ifmt_ctx);
        av_bsf_free(&bsf_ctx);
        return -1;
    }

    pkt = av_packet_alloc();
    av_init_packet(pkt);
    while (1) {
        ret = av_read_frame(ifmt_ctx, pkt);     //
        if(ret < 0 ) {
            av_strerror(ret, errors, ERROR_STRING_SIZE);
            printf("av_read_frame failed:%s\n", errors);
            break;
        }
        if(pkt->stream_index == video_index) {
            // 处理视频
            ret = av_bsf_send_packet(bsf_ctx, pkt);
            if(ret < 0) {
                av_strerror(ret, errors, ERROR_STRING_SIZE);
                av_packet_unref(pkt);
                continue;
            }
            while (1) {
                ret = av_bsf_receive_packet(bsf_ctx, pkt);
                if(ret != 0) {
                    break;
                }
                size_t size = fwrite(pkt->data, 1, pkt->size, h264_fd);
                if(size != pkt->size)
                {
                    av_log(NULL, AV_LOG_DEBUG, "h264 warning, length of writed data isn't equal pkt->size(%d, %d)\n",
                           size,
                           pkt->size);
                }
                av_packet_unref(pkt);
            }
        } else if(pkt->stream_index == audio_index) {
            // 处理音频
            char adts_header_buf[7] = {0};
            adts_header(adts_header_buf, pkt->size,
                        ifmt_ctx->streams[audio_index]->codecpar->profile,
                        ifmt_ctx->streams[audio_index]->codecpar->sample_rate,
                        ifmt_ctx->streams[audio_index]->codecpar->ch_layout.nb_channels);
            fwrite(adts_header_buf, 1, 7, aac_fd);
            size_t size = fwrite(pkt->data, 1, pkt->size, aac_fd);
            if(size != pkt->size)
            {
                av_log(NULL, AV_LOG_DEBUG, "aac warning, length of writed data isn't equal pkt->size(%d, %d)\n",
                       size,
                       pkt->size);
            }
            av_packet_unref(pkt);
        } else {
            av_packet_unref(pkt);       // 释放buffer
        }
    }
    printf("while finish\n");
failed:
    if(h264_fd) {
        fclose(h264_fd);
    }
    if(aac_fd) {
        fclose(aac_fd);
    }
    if(pkt)
        av_packet_free(&pkt);
    if(ifmt_ctx)
        avformat_close_input(&ifmt_ctx);

    return 0;
}


int main(int argc, char **argv)
{
    if (argc < 2)
    {
        av_log(NULL, AV_LOG_ERROR, "Incorrect input\n");
        return 1;
    }
	split();
    //if (video_decode_1078(argv[1]) != 0)
   //     return 1;

    return 0;
}
