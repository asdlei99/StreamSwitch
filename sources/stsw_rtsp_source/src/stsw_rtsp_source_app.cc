/**
 * StreamSwitch is an extensible and scalable media stream server for 
 * multi-protocol environment. 
 * 
 * Copyright (C) 2014  OpenSight (www.opensight.cn)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/**
 * stsw_rtsp_source_app.cc
 *      RtspSourceApp class implementation file, 
 * contains all the methods and logic of the RtspSourceApp class
 * 
 * author: jamken
 * date: 2015-5-6
**/ 

#include "stsw_rtsp_source_app.h"

#include "BasicUsageEnvironment.hh"

#define LOGGER_CHECK_INTERVAL 600 //600 sec





RtspSourceApp * RtspSourceApp::s_instance = NULL;


RtspSourceApp::RtspSourceApp()
: rtsp_client_(NULL), scheduler_(NULL), env_(NULL), logger_(NULL), watch_variable_(0), 
is_init_(false), exit_code_(0), logger_check_task_(NULL)
{
    
}

RtspSourceApp::~RtspSourceApp()
{
    Uninit();

}

int RtspSourceApp::Init(int argc, char ** argv)
{
    int ret;
    scheduler_ = BasicTaskScheduler::createNew();
    env_ = BasicUsageEnvironment::createNew(*scheduler_); 
    prog_name_ = argv[0];
    std::string rtsp_url;
    Boolean streamUsingTCP = True;
    Boolean enableRtspKeepAlive = True; 
    char const* singleMedium = NULL;
    char const* userName = NULL, * passwd = NULL;
    stream_switch::ArgParser parser;
    int verbosityLevel = 0;
    
    
    //
    //Parse options
    //parse the cmd line
    
    ParseArgv(argc, argv, &parser); // parse the cmd line


    //Create logger
    //
    
    // init global logger
    if(parser.CheckOption("log-file")){
        //init the global logger
        logger_ = new stream_switch::RotateLogger();
        std::string log_file = 
            parser.OptionValue("log-file", "");
        int log_size = 
            strtol(parser.OptionValue("log-size", "0").c_str(), NULL, 0);
        int rotate_num = 
            strtol(parser.OptionValue("log-rotate", "0").c_str(), NULL, 0);      
        int log_level = stream_switch::LOG_LEVEL_DEBUG;
        
        ret = logger_->Init("stsw_rtsp_source", 
            log_file, log_size, rotate_num, log_level, true);
        if(ret){
            delete logger_;
            logger_ = NULL;
            fprintf(stderr, "Init Logger faile\n");
            ret = -1;
            goto error_out1;
        }        
    }    
    
    
    
        
    //Create Stream_switch source and init it
    
    
    //Create Rtsp Client    
    rtsp_url = parser.OptionValue("url", "");
    if(parser.CheckOption("udp")){
        streamUsingTCP = False;
    }
    if(parser.CheckOption("no-keep-alive")){
        enableRtspKeepAlive = False;
    }
    if(parser.CheckOption("single-medium")){
        singleMedium = parser.OptionValue("single-medium", "video").c_str();
    }

    if(parser.CheckOption("user")){
        userName = parser.OptionValue("user", "").c_str();
    }

    if(parser.CheckOption("password")){
        passwd = parser.OptionValue("password", "").c_str();
    }
    
    if(parser.CheckOption("rtsp-verbose")){
        verbosityLevel = 1;
    }    
    
/*    
    rtsp_client_ = LiveRtspClient::CreateNew(
        *env_, (char *)"rtsp://172.16.56.120:554/user=admin&password=123456&id=1&type=0",  True, True, 
        NULL,  NULL, NULL, (LiveRtspClientListener *)this, 1);
*/
    rtsp_client_ = LiveRtspClient::CreateNew(
        *env_, rtsp_url.c_str(),  streamUsingTCP, enableRtspKeepAlive, 
        singleMedium,  userName, passwd, (LiveRtspClientListener *)this, 
        verbosityLevel);        
    if(rtsp_client_ == NULL){
        fprintf(stderr, "LiveRtspClient::CreateNew() Failed: Maybe parameter error\n");
        ret = -1;
        goto error_out2;
    }
    
    
    ROTATE_LOG(logger_, stream_switch::LOG_LEVEL_INFO, 
               "RTSP Source init successful");
    
    
    //install a logger check timer
    LoggerCheckHandler(this);

    
    is_init_ = true;
    
    return 0;

error_out2:

    //delete logger
    if(logger_){
        logger_->Uninit();
        delete logger_;
        logger_ = NULL;
    }        

error_out1:

    if(env_ != NULL) {
        env_->reclaim();
        env_ = NULL;
    }
    
    if(scheduler_ != NULL){
        delete scheduler_;
        scheduler_ = NULL;
    }   
    
    return ret;
    
}
void RtspSourceApp::Uninit()
{
    is_init_ = false;

    ROTATE_LOG(logger_, stream_switch::LOG_LEVEL_INFO, 
               "RTSP Source Uninit");

    
    //cancel the logger check timer
    if(logger_check_task_ != NULL) {
        env_->taskScheduler().unscheduleDelayedTask(logger_check_task_);
        logger_check_task_ = NULL;
    }     
    
    
    //shutdown delete rtsp Client
    if(rtsp_client_ != NULL){
        rtsp_client_->Shutdown();
        Medium::close(rtsp_client_);
        rtsp_client_ = NULL;
    }
    
    //uninit stream_switch source and delete
 

    //delete logger
    if(logger_){
        logger_->Uninit();
        delete logger_;
        logger_ = NULL;
    }    
   
    if(env_ != NULL) {
        env_->reclaim();
        env_ = NULL;
    }
    
    if(scheduler_ != NULL){
        delete scheduler_;
        scheduler_ = NULL;
    }    
    
    
    
}
int RtspSourceApp::DoLoop()
{
    if(!is_init_){
        fprintf(stderr, "RtspSourceApp Not Init\n");
        return -1;
    }    
    
    rtsp_client_->Start();
 
    ROTATE_LOG(logger_, stream_switch::LOG_LEVEL_INFO, 
               "RTSP Source start loop");
   
    
    env_->taskScheduler().doEventLoop(&watch_variable_);
    
    
    //shutdown rtsp client
    rtsp_client_->Shutdown(); 
    
    //shutdown source
    
    return exit_code_;
}



void RtspSourceApp::ParseArgv(int argc, char *argv[], 
                                stream_switch::ArgParser *parser)
{
    int ret = 0;
    std::string err_info;
    parser->RegisterBasicOptions();
    parser->RegisterSourceOptions();
    
    parser->RegisterOption("url", 'u', OPTION_FLAG_REQUIRED | OPTION_FLAG_WITH_ARG,
                   "RTSP_URL", 
                   "RTSP url this source connect to, must leading with \"rtsp://\"", NULL, NULL);  
/*                   
    parser->RegisterOption("frame-size", 'F',  OPTION_FLAG_WITH_ARG,
                   "SIZE", 
                   "Frame size to send out", NULL, NULL);                     
    parser->RegisterOption("fps", 'f',  OPTION_FLAG_WITH_ARG,
                   "NUM", 
                   "Frames per secode to send", NULL, NULL);         
*/

    parser->RegisterOption("udp", 0,  0, NULL, 
                   "using RTPoverUDP, if absent, RTPOverRTSP would be adopted by default", NULL, NULL);  
                   

    parser->RegisterOption("no-keep-alive", 0,  0, NULL, 
                   "disable the keep-alive feature on rtsp session, which is enabled by default", NULL, NULL);    

    parser->RegisterOption("single-medium", 'm', OPTION_FLAG_WITH_ARG, "MEDIUM_NAME", 
                   "set up only one media sub session of RTSP whose name is specified by MEDIUM_NAME", NULL, NULL);      


    parser->RegisterOption("user", 'U', OPTION_FLAG_WITH_ARG , "NAME", 
                   "RTSP username for auth, if present", NULL, NULL);  

    parser->RegisterOption("password", 'P', OPTION_FLAG_WITH_ARG , "PASSWORD", 
                   "RTSP password for auth, if present", NULL, NULL);  

    parser->RegisterOption("rtsp-verbose", 'V',  0, NULL, 
                   "Output RTSP verbose info to stderr", NULL, NULL);  

    parser->RegisterOption("debug-flags", 'd', 
                    OPTION_FLAG_LONG | OPTION_FLAG_WITH_ARG,  "FLAG", 
                    "debug flag for stream_switch core library. "
                    "Default is 0, means no debug dump" , 
                    NULL, NULL);  
  
    ret = parser->Parse(argc, argv, &err_info);//parse the cmd args
    if(ret){
        fprintf(stderr, "Option Parsing Error:%s\n", err_info.c_str());
        exit(-1);
    }

    //check options correct
    
    if(parser->CheckOption("help")){
        std::string option_help;
        option_help = parser->GetOptionsHelp();
        fprintf(stderr, 
        "A RTSP live source which connect to a RTSP server and reads the media frames from it\n"
        "Usange: %s [options]\n"
        "\n"
        "Option list:\n"
        "%s"
        "\n"
        "User can send SIGINT/SIGTERM signal to terminate this source\n"
        "\n", "stsw_rtsp_source", option_help.c_str());
        exit(0);
    }else if(parser->CheckOption("version")){
        
        fprintf(stderr, "0.1.0\n");
        exit(0);
    }
    
  
    if(parser->CheckOption("log-file")){
        if(!parser->CheckOption("log-size")){
            fprintf(stderr, "log-size must be set if log-file is enabled\n");
            exit(-1);
        }     
    }
   

}


//////////////////////////////////////////////////////////
//Timer task handler
void RtspSourceApp::LoggerCheckHandler(void* clientData)
{
    RtspSourceApp * app = (RtspSourceApp *)clientData;
    app->logger_check_task_ = NULL;
    
    //check logger
    if(app->logger_ != NULL){
        app->logger_->CheckRotate();       
    }

    app->logger_check_task_ =
            app->env_->taskScheduler().scheduleDelayedTask(LOGGER_CHECK_INTERVAL * 1000000, 
				 (TaskFunc*)LoggerCheckHandler, app);

    
}




///////////////////////////////////////////////////////////
// LiveRtspClientListener implementation
void RtspSourceApp::OnMediaFrame(
        const stream_switch::MediaFrameInfo &frame_info, 
        const char * frame_data, 
        size_t frame_size
)
{
#if 0    
    fprintf(stderr, "RtspSourceApp::OnMediaFrame() is called with the below frame:\n");
    fprintf(stderr, 
                "index:%d, type:%d, time:%lld.%03d, ssrc:0x%x, size: %d\n", 
                (int)frame_info.sub_stream_index, 
                frame_info.frame_type, 
                (long long)frame_info.timestamp.tv_sec, 
                (int)(frame_info.timestamp.tv_usec/1000), 
                (unsigned)frame_info.ssrc, 
                (int)(frame_size));
#endif    
}
void RtspSourceApp::OnError(RtspClientErrCode err_code, const char * err_info)
{
    if(logger_ != NULL){
        ROTATE_LOG(logger_, stream_switch::LOG_LEVEL_ERR, 
               "RtspSourceApp::OnError() is called with RtspClientErrCode err_code(%d):%s\n",
                err_code, (err_info!=NULL)? err_info:"");
    }else{
        
        fprintf(stderr, "RtspSourceApp::OnError() is called with RtspClientErrCode err_code(%d):%s\n",
                err_code, (err_info!=NULL)? err_info:"");
                
    }
    
    
    exit_code_ = -1;
    SetWatch();
}

void RtspSourceApp::OnMetaReady(const stream_switch::StreamMetadata &metadata)
{
  
    fprintf(stderr, "RtspSourceApp::OnMetaReady() is called with the below metadata:\n");
    fprintf(stderr, 
            "play_type:%d, source_proto:%s, ssrc:0x%x, bps: %d, substream number: %d\n", 
            (int)metadata.play_type,  
            metadata.source_proto.c_str(), 
            (unsigned)metadata.ssrc, 
            (int)metadata.bps,
            (int)(metadata.sub_streams.size()));    
            
    stream_switch::SubStreamMetadataVector::const_iterator it;
    for(it = metadata.sub_streams.begin();
        it != metadata.sub_streams.end();
        it++){  
      
        fprintf(stderr, 
                "Stream %u --- media_type:%d, codec_name:%s, direction:%d, extra_data size:%d\n", 
                it->sub_stream_index, 
                (int)it->media_type,
                it->codec_name.c_str(), 
                (int)it->direction, 
                (int)it->extra_data.size());                
            
    }
    fprintf(stderr, "\n");
  
}
void RtspSourceApp::OnRtspOK()
{
    if(logger_ != NULL){
        ROTATE_LOG(logger_, stream_switch::LOG_LEVEL_INFO, 
               "RTSP Negotiation is successful\n");
    }else{
        
        fprintf(stderr, "RTSP Negotiation is successful\n");
                
    }
    
   
}
    
