// RTSP Server

#include "xop/RtspServer.h"
#include "net/Timer.h"
#include <thread>
#include <memory>
#include <iostream>
#include <string>
#include "JPEGSource.h"
#include <mutex>

#include <opencv2/opencv.hpp>

#define LOGV(x) std::cout << "V [" << __FILE__ << "+L" << __LINE__ << "] " << x << std::endl
#define LOGI(x) std::cout << "I [" << __FILE__ << "+L" << __LINE__ << "] " << x << std::endl
#define LOGE(x) std::cout << "E*[" << __FILE__ << "+L" << __LINE__ << "] " << x << std::endl
#define LOGD(x) ;

typedef struct {
    // algo<->rtsp
    std::mutex mtx;
    bool update;
    int width;
    int height;
    cv::Mat frame;
} ShareObject;

void SendFrameThread(ShareObject *p_share, xop::RtspServer* rtsp_server, xop::MediaSessionId session_id, int& clients);

void ProcFrameThread(ShareObject *p_share);

int main(int argc, char **argv)
{    
    int clients = 0;
    std::string ip = "0.0.0.0";
    std::string rtsp_url = "rtsp://127.0.0.1:554/live";

    std::shared_ptr<xop::EventLoop> event_loop(new xop::EventLoop());  
    std::shared_ptr<xop::RtspServer> server = xop::RtspServer::Create(event_loop.get());
    if (!server->Start(ip, 554)) {
        return -1;
    }
    
#ifdef AUTH_CONFIG
    server->SetAuthConfig("-_-", "admin", "12345");
#endif
     
    xop::MediaSession *session = xop::MediaSession::CreateNew("live"); // url: rtsp://ip/live
    session->AddSource(xop::channel_0, xop::JPEGSource::CreateNew()); 
    //session->AddSource(xop::channel_1, xop::AACSource::CreateNew(44100,2));
    // session->startMulticast(); /* 开启组播(ip,端口随机生成), 默认使用 RTP_OVER_UDP, RTP_OVER_RTSP */

    session->AddNotifyConnectedCallback([] (xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port){
        printf("RTSP client connect, ip=%s, port=%hu \n", peer_ip.c_str(), peer_port);
    });
   
    session->AddNotifyDisconnectedCallback([](xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port) {
        printf("RTSP client disconnect, ip=%s, port=%hu \n", peer_ip.c_str(), peer_port);
    });

    std::cout << "URL: " << rtsp_url << std::endl;
        
    xop::MediaSessionId session_id = server->AddSession(session); 
    //server->removeMeidaSession(session_id); /* 取消会话, 接口线程安全 */
    
    ShareObject share_obj;
    share_obj.height = 1080;
    share_obj.width = 1920;
    share_obj.update = false;

    std::thread thread_send(SendFrameThread, &share_obj, server.get(), session_id, std::ref(clients));
    thread_send.detach();

    std::thread thread_proc(ProcFrameThread, &share_obj);
    thread_proc.detach();

    while(1) {
        xop::Timer::Sleep(100);
    }

    getchar();
    return 0;
}

class JPEGFile
{
public:
    JPEGFile(int buf_size=500000);
    ~JPEGFile();

    bool Open(const char *path);
    bool Open(const cv::Mat &img);
    void Close();

    bool IsOpened() const
    { return (m_file != NULL); }

    int ReadFrame(char* in_buf, int in_buf_size, bool* end);
    //int ReadFrameMat(const cv::Mat &img, char* in_buf, int in_buf_size, bool* end);
private:
    FILE *m_file = NULL;
    unsigned char *m_buf = NULL;
    int  m_buf_size = 0;
    int  m_bytes_used = 0;
    int  m_count = 0;
};

void SendFrameThread(ShareObject *p_share, xop::RtspServer* rtsp_server, xop::MediaSessionId session_id, int& clients)
{
    int buf_size = 2000000;
    std::unique_ptr<uint8_t> frame_buf(new uint8_t[buf_size]);

    int height = p_share->height;
    int width = p_share->width;

    cv::Mat image = cv::Mat::zeros(height, width, CV_8UC3);
    cv::putText(image, "RK Vision Demo (RK3588) \n         ver.20241005", cv::Point(800, 500), cv::FONT_HERSHEY_PLAIN, 2.0, CV_RGB(255, 0, 0));
    
    JPEGFile JPEG_file;
    JPEG_file.Open(image);

    while(1)
    {
        if(clients >= 0) /* 会话有客户端在线, 发送音视频数据 */
        {
            {
                if (p_share->mtx.try_lock()) {
                    if(p_share->update == true && !p_share->frame.empty()) {
                        p_share->update = false;
                        image = p_share->frame;
                        JPEG_file.Close();
                        JPEG_file.Open(image);
                        //image.release();
                    }
                    p_share->mtx.unlock();

                }

                bool end_of_frame = false;
                int frame_size = JPEG_file.ReadFrame((char*)frame_buf.get(), buf_size, &end_of_frame);
                if (frame_size > 0) {
                    xop::AVFrame videoFrame = {0};
                    videoFrame.type = 0; 
                    videoFrame.size = frame_size;
                    videoFrame.timestamp = xop::JPEGSource::GetTimestamp();
                    videoFrame.buffer.reset(new uint8_t[videoFrame.size]);    
                    memcpy(videoFrame.buffer.get(), frame_buf.get(), videoFrame.size);
                    rtsp_server->PushFrame(session_id, xop::channel_0, videoFrame);
                } else {
                    break;
                }

                /*
                //获取一帧 H264, 打包
                xop::AVFrame videoFrame = {0};
                videoFrame.type = 0; // 建议确定帧类型。I帧(xop::VIDEO_FRAME_I) P帧(xop::VIDEO_FRAME_P)
                videoFrame.size = video frame size;  // 视频帧大小 
                videoFrame.timestamp = xop::H264Source::GetTimestamp(); // 时间戳, 建议使用编码器提供的时间戳
                videoFrame.buffer.reset(new uint8_t[videoFrame.size]);                    
                memcpy(videoFrame.buffer.get(), video frame data, videoFrame.size);                    
                   
                rtsp_server->PushFrame(session_id, xop::channel_0, videoFrame); //送到服务器进行转发, 接口线程安全
                */
            }
                    
            {                
                /*
                //获取一帧 AAC, 打包
                xop::AVFrame audioFrame = {0};
                audioFrame.type = xop::AUDIO_FRAME;
                audioFrame.size = audio frame size;  /* 音频帧大小 
                audioFrame.timestamp = xop::AACSource::GetTimestamp(44100); // 时间戳
                audioFrame.buffer.reset(new uint8_t[audioFrame.size]);                    
                memcpy(audioFrame.buffer.get(), audio frame data, audioFrame.size);

                rtsp_server->PushFrame(session_id, xop::channel_1, audioFrame); // 送到服务器进行转发, 接口线程安全
                */
            }        
        }

        xop::Timer::Sleep(10); /* 实际使用需要根据帧率计算延时! */
    }

    JPEG_file.Close();

}


JPEGFile::JPEGFile(int buf_size)
    : m_buf_size(buf_size)
{
    m_buf = new unsigned char[m_buf_size];
}

JPEGFile::~JPEGFile()
{
    delete [] m_buf;
}

bool JPEGFile::Open(const char *path)
{
    m_file = fopen(path, "rb");
    if(m_file == NULL) {      
        return false;
    }

    return true;
}

bool JPEGFile::Open(const cv::Mat &image)
{

    cv::imwrite("./tmp.jpg", image);
    m_file = fopen("./tmp.jpg", "rb");
    if(m_file == NULL) {      
        return false;
    }

    return true;
}

void JPEGFile::Close()
{
    if(m_file) {
        fclose(m_file);
        m_file = NULL;
        m_count = 0;
        m_bytes_used = 0;
    }
}

int JPEGFile::ReadFrame(char* in_buf, int in_buf_size, bool* end)
{
    if(m_file == NULL || m_buf == NULL) {
        return -1;
    }
    int i = 0;
    int bytes_read = (int)fread(m_buf, 1, m_buf_size, m_file);
    if(bytes_read == 0) {
        fseek(m_file, 0, SEEK_SET); 
        m_count = 0;
        m_bytes_used = 0;
        bytes_read = (int)fread(m_buf, 1, m_buf_size, m_file);
        if(bytes_read == 0)         {            
            this->Close();
            printf("error read file return zero\n");
            return -1;
        }
    }
    //printf("bytes_read = %d\n", bytes_read);
#if 1
    //for pic
    int size = (bytes_read<=in_buf_size ? bytes_read : in_buf_size);
    memcpy(in_buf, m_buf, size); 
    fseek(m_file, 0, SEEK_SET);
    return bytes_read;
#else
    //for mjpeg
    bool is_find_start = false;
    bool is_find_end = false;
    unsigned int start_pos = 0;
    unsigned int end_pos = 0;
    *end = false;
    //printf("0x%x 0x%x\n", m_buf[0], m_buf[1]);
    for (i=0; i<bytes_read-1; i++) {
        unsigned char cur = m_buf[i+0];
        unsigned char next = m_buf[i+1];
        
        if(is_find_start == false && cur == 0xff && next == 0xd8){
            is_find_start = true;
            start_pos = i;
            //printf("start_pos = %d\n", start_pos);
            continue;
        }
        if(is_find_start == true && cur == 0xff && next == 0xd9){//EOI
            is_find_end = true;
            end_pos = i + 2;
            //printf("end_pos = %d\n", end_pos);
            break;
        }else if(is_find_start == true && cur == 0xff && next == 0xd8){//
            is_find_end = true;
            end_pos = i;
            //printf("end_pos = %d\n", end_pos);
            break;
        }
    }
    //printf("-----------------------\n");
    if(is_find_start && is_find_end){

    }

    if(is_find_start && !is_find_end) {        
        is_find_end = true;
        end_pos = i + 1;
        //*end = true;
        //printf("!!end_pos = %d\n", end_pos);
    }

    if(!is_find_start || !is_find_end) {
        m_count = 0;
        m_bytes_used = 0;
        printf("check your file, no start or end flag\n");
        this->Close();
        return -1;
    }

    int start_end_len = end_pos - start_pos;
    int size = (start_end_len<=in_buf_size ? start_end_len : in_buf_size);
    //printf("start_end_len = %d, size = %d\n", start_end_len, size);
    memcpy(in_buf, &m_buf[start_pos], size); 
    m_bytes_used += size;
    fseek(m_file, m_bytes_used, SEEK_SET);
    return size;
#endif
}

void ProcFrameThread(ShareObject *p_share) {
    int height_out = p_share->height;
    int width_out = p_share->width;

    cv::Mat frame_input;
    cv::Mat frame_output;

    //LOGV(cv::getBuildInformation());

    cv::VideoCapture capture;
    capture.open("v4l2src device=/dev/video11 ! video/x-raw,format=NV12,width=3840,height=2160,framerate=30/1 ! videoconvert ! appsink",cv::CAP_GSTREAMER);

    if (!capture.isOpened()) {
        std::cout << "camera open failed." << std::endl;
        return;
    }

    while(true) {
        capture.read(frame_input);

        if (frame_input.empty()) {
            break;
        }

        cv::resize(frame_input, frame_output, cv::Size(width_out, height_out));

        if (p_share->mtx.try_lock()) {
            p_share->update = true;
            p_share->frame = frame_output;
            p_share->mtx.unlock();
        }

        //
    }
    
}

