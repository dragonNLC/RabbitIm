#include "Tool.h"

CTool::CTool(QObject *parent) :
    QObject(parent)
{
}

CTool::~CTool()
{
}


//设置日志的回调函数
void Log(void*, int, const char* fmt, va_list vl)
{
    static char buf[1024];
    vsprintf(buf, fmt, vl);
    qDebug(buf);
}

int CTool::SetFFmpegLog()
{
    //在程序初始化时设置ffmpeg日志的回调函数
    av_log_set_callback(Log);
    return 0;
}

AVPixelFormat CTool::QVideoFrameFormatToFFMpegPixFormat(const QVideoFrame::PixelFormat format)
{
    if(QVideoFrame::Format_RGB32 == format)
        return AV_PIX_FMT_RGB32;
    else if(QVideoFrame::Format_BGR24 == format)
        return AV_PIX_FMT_BGR24;
    else if(QVideoFrame::Format_RGB24 == format)
        return AV_PIX_FMT_RGB24;
    else if(QVideoFrame::Format_YUYV == format)
        return AV_PIX_FMT_YUYV422;
    else if(QVideoFrame::Format_UYVY == format)
        return AV_PIX_FMT_UYVY422;
    else if(QVideoFrame::Format_NV21 == format)
        return AV_PIX_FMT_NV21;
    else
        return AV_PIX_FMT_NONE;
}

AVPixelFormat CTool::QXmppVideoFrameFormatToFFMpegPixFormat(const QXmppVideoFrame::PixelFormat format)
{
    if(QXmppVideoFrame::Format_RGB32 == format)
        return AV_PIX_FMT_RGB32;
    else if(QXmppVideoFrame::Format_RGB24 == format)
        return AV_PIX_FMT_RGB24;
    else if(QXmppVideoFrame::Format_YUYV == format)
        return AV_PIX_FMT_YUYV422;
    else if(QXmppVideoFrame::Format_UYVY == format)
        return AV_PIX_FMT_UYVY422;
    else if(QXmppVideoFrame::Format_YUV420P == format)
        return AV_PIX_FMT_YUV420P;
    else
        return AV_PIX_FMT_NONE;
}

//如果转换成功，则调用者使用完 pOutFrame 后，需要调用 avpicture_free(pOutFrame) 释放内存
//成功返回0，不成功返回非0
int CTool::ConvertFormat(/*[in]*/ const QVideoFrame &inFrame,
                         /*[out]*/AVPicture &outFrame,
                         /*[in]*/ int nWidth,
                         /*[in]*/ int nHeight,
                         /*[in]*/ AVPixelFormat pixelFormat)
{
    int nRet = 0;

    AVPicture pic;
    nRet = avpicture_fill(&pic, inFrame.bits(),
                   QVideoFrameFormatToFFMpegPixFormat(inFrame.pixelFormat()),
                   inFrame.width(),
                   inFrame.height());
    if(nRet < 0)
    {
        qDebug(("avpicture_fill fail:%x"), nRet);
        return nRet;
    }

    nRet = ConvertFormat(pic, inFrame.width(), inFrame.height(),
                         QVideoFrameFormatToFFMpegPixFormat(inFrame.pixelFormat()),
                         outFrame, nWidth, nHeight, pixelFormat);

    return nRet;
}

int CTool::ConvertFormat(/*[in]*/ const QXmppVideoFrame &inFrame,
                         /*[out]*/AVPicture &outFrame,
                         /*[in]*/ int nWidth,
                         /*[in]*/ int nHeight,
                         /*[in]*/ AVPixelFormat pixelFormat)
{
    int nRet = 0;

    AVPicture pic;
    nRet = avpicture_fill(&pic, inFrame.bits(),
                   QXmppVideoFrameFormatToFFMpegPixFormat(inFrame.pixelFormat()),
                   inFrame.width(),
                   inFrame.height());
    if(nRet < 0)
    {
        qDebug(("avpicture_fill fail:%x"), nRet);
        return nRet;
    }

    nRet = ConvertFormat(pic, inFrame.width(), inFrame.height(),
                         QXmppVideoFrameFormatToFFMpegPixFormat(inFrame.pixelFormat()),
                         outFrame, nWidth, nHeight,
                         pixelFormat);

    return nRet;
}

int CTool::ConvertFormat(/*[in]*/ const AVPicture &inFrame,
                         /*[in]*/ int nInWidth,
                         /*[in]*/ int nInHeight,
                         /*[in]*/ AVPixelFormat inPixelFormat,
                         /*[out]*/AVPicture &outFrame,
                         /*[in]*/ int nOutWidth,
                         /*[in]*/ int nOutHeight,
                         /*[in]*/ AVPixelFormat outPixelFormat)
{
    int nRet = 0;
    struct SwsContext* pSwsCtx = NULL;

    //分配输出空间
    nRet = avpicture_alloc(&outFrame, outPixelFormat, nOutWidth, nOutHeight);
    if(nRet)
    {
        qDebug(("avpicture_alloc fail:%x"), nRet);
        return nRet;
    }

    if(inPixelFormat == outPixelFormat
            && nInWidth == nOutWidth
            && nInHeight == nOutHeight)
    {
        av_picture_copy(&outFrame, &inFrame, inPixelFormat, nInWidth, nInHeight);
        return 0;
    }

    //设置图像转换上下文
    pSwsCtx = sws_getCachedContext (NULL,
                                    nInWidth,                //源宽度
                                    nInHeight,               //源高度
                                    inPixelFormat,  //源格式
                                    nOutWidth,                         //目标宽度
                                    nOutHeight,                        //目标高度
                                    outPixelFormat,                    //目的格式
                                    SWS_FAST_BILINEAR,              //转换算法
                                    NULL, NULL, NULL);
    if(NULL == pSwsCtx)
    {
        qDebug("sws_getContext false");
        avpicture_free(&outFrame);
        return -3;
    }

    //进行图片转换
    nRet = sws_scale(pSwsCtx,
                     inFrame.data, inFrame.linesize,
                     0, nInHeight,
                     outFrame.data, outFrame.linesize);
    if(nRet < 0)
    {
        qDebug("sws_scale fail:%x", nRet);
        avpicture_free(&outFrame);
    }
    else
    {
        //qDebug("sws_scale height:%d", nRet);
        nRet = 0;
    }

    sws_freeContext(pSwsCtx);
    return nRet;
}


cv::Mat CTool::ImageRotate(cv::Mat & src, const CvPoint &_center, double angle)
{
    CvPoint2D32f center;
    center.x = float(_center.x);
    center.y = float(_center.y);

    //计算二维旋转的仿射变换矩阵
    cv::Mat M = cv::getRotationMatrix2D(center, angle, 1);

    // rotate
    cv::Mat dst;
    cv::warpAffine(src, dst, M, cvSize(src.cols, src.rows), cv::INTER_LINEAR);
    return dst;
}

void CTool::YUV420spRotate90(uchar *des, uchar *src,int width,int height)
{
    int wh = width * height;
    //旋转Y
    int k = 0;
    for(int i = 0; i < width; i++) {
        for(int j = 0; j < height; j++)
        {
            des[k] = src[width * j + i];
            k++;
        }
    }

    for(int i = 0; i < width; i += 2) {
        for(int j = 0; j < height / 2; j++)
        {
            des[k] = src[wh+ width * j + i];
            des[k+1] = src[wh + width * j + i + 1];
            k+=2;
        }
    }
}

//以Y轴做镜像
void YUV420spMirrorY(uchar *dst, uchar *src, int width, int height)
{
    int wh = width * height;
    //镜像Y
    int k = 0;

    for(int j = 0; j < height; j++) {
        int nPos = (j + 1) * width;
        for(int i = 0; i < width; i++)
        {
            dst[k] = src[nPos - i];
            k++;
        }
    }

    for(int j = 0; j < height / 2; j ++) {
        int nPos = wh + (j + 1) * width;
        for(int i = 0; i < width; i += 2)
        {
            dst[k] = src[nPos - i];
            dst[k+1] = src[nPos - i - 1];
            k+=2;
        }
    }
}

//以X轴做镜像
void YUV420spMirrorX(uchar *dst, uchar *src, int width, int height)
{
    int wh = width * height;
    //镜像Y
    int k = 0;

    for(int j = 0; j < height; j++) {
        int nPos = (height - j) * width;
        for(int i = 0; i < width; i++)
        {
            dst[k] = src[nPos + i];
            k++;
        }
    }

    for(int j = 0; j < height / 2; j ++) {
        int nPos = wh + (height / 2 - j) * width;
        for(int i = 0; i < width; i += 2)
        {
            dst[k] = src[nPos + i];
            dst[k+1] = src[nPos + i + 1];
            k+=2;
        }
    }
}

void CTool::YUV420spMirror(uchar *dst, uchar *src, int width, int height, int mode)
{
    switch (mode) {
    case 0:
        return YUV420spMirrorY(dst, src, width, height);
        break;
    case 1:
        return YUV420spMirrorX(dst, src, width, height);
    default:
        break;
    }
}

