#include <stdio.h>
extern "C"
{
#include <libavformat/avformat.h>
}
 
/* 打印编码器支持该采样率并查找指定采样率下标 */
static int find_sample_rate_index(const AVCodec* codec, int sample_rate)
{
        const int* p = codec->supported_samplerates;
        int sample_rate_index = -1; //支持的分辨率下标
        int count = 0;
        while (*p != 0) {// 0作为退出条件，比如libfdk-aacenc.c的aac_sample_rates
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
        header[2] |= (channelConfig & 0x04) >> 2;           //channel configuration:channel_config               高1bit
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
 
 
int main() {
        AVFormatContext* ifmt_ctx = NULL;
        AVPacket pkt;
        int ret;
    unsigned int i;
        int videoindex = -1, audioindex = -1;
        const char* in_filename = "test.mp4";
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
 
        //这里遍历音频编码器打印支持的采样率，并找到当前音频采样率所在的下表，用于后面添加adts头
    //本程序并没有使用，只是测试，如果为了程序健壮性可以采用此方式
        const AVCodec* codec = nullptr;
        codec  = avcodec_find_encoder(ifmt_ctx->streams[audioindex]->codecpar->codec_id);
        int sample_rate_index = find_sample_rate_index(codec, ifmt_ctx->streams[audioindex]->codecpar->sample_rate);
        printf("分辨率数组下表：%d\n", sample_rate_index);
 
        while (av_read_frame(ifmt_ctx, &pkt) >= 0) {
                if (pkt.stream_index == videoindex) {
 
                        av_bsf_send_packet(bsf_ctx, &pkt);
 
                        while (true)
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
                                ifmt_ctx->streams[audioindex]->codecpar->channels, 
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