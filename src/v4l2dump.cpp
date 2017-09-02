/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** v4l2dump.cpp
** 
** Dump frame from a V4L2 capture device 
** 
** -------------------------------------------------------------------------*/

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <signal.h>

#include <fstream>

#include "logger.h"

#include "V4l2Device.h"
#include "V4l2Capture.h"
#include "V4l2Output.h"

#include "h264_stream.h"
//#include "hevc_stream.h"

int stop=0;

/* ---------------------------------------------------------------------------
**  SIGINT handler
** -------------------------------------------------------------------------*/
void sighandler(int)
{ 
       printf("SIGINT\n");
       stop =1;
}

/* ---------------------------------------------------------------------------
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char* argv[]) 
{	
	int verbose=0;
	const char *in_devname = "/dev/video0";	
	int c = 0;
	V4l2Access::IoType ioTypeIn  = V4l2Access::IOTYPE_MMAP;
	
	while ((c = getopt (argc, argv, "hP:F:v::rw")) != -1)
	{
		switch (c)
		{
			case 'v':	verbose   = 1; if (optarg && *optarg=='v') verbose++;  break;
			case 'r':	ioTypeIn  = V4l2Access::IOTYPE_READWRITE; break;			
			case 'h':
			{
				std::cout << argv[0] << " [-v[v]] [-W width] [-H height] source_device dest_device" << std::endl;
				std::cout << "\t -v            : verbose " << std::endl;
				std::cout << "\t -vv           : very verbose " << std::endl;
				std::cout << "\t -r            : V4L2 capture using read interface (default use memory mapped buffers)" << std::endl;
				std::cout << "\t source_device : V4L2 capture device (default "<< in_devname << ")" << std::endl;
				exit(0);
			}
		}
	}
	if (optind<argc)
	{
		in_devname = argv[optind];
		optind++;
	}	

	// initialize log4cpp
	initLogger(verbose);

	// init V4L2 capture interface
	V4L2DeviceParameters param(in_devname, 0, 0, 0, 0,verbose);
	V4l2Capture* videoCapture = V4l2Capture::create(param, ioTypeIn);
	
	if (videoCapture == NULL)
	{	
		LOG(WARN) << "Cannot reading from V4L2 capture interface for device:" << in_devname; 
	}
	else
	{
		h264_stream_t* h264 = h264_new();
//		hevc_stream_t* hevc = hevc_new();
		
		timeval tv;
		
		LOG(NOTICE) << "Start reading from " << in_devname ; 
		signal(SIGINT,sighandler);				
		while (!stop) 
		{
			tv.tv_sec=1;
			tv.tv_usec=0;
			int ret = videoCapture->isReadable(&tv);
			if (ret == 1)
			{
				char buffer[videoCapture->getBufferSize()];
				int rsize = videoCapture->read(buffer, sizeof(buffer));
				if (rsize == -1)
				{
					LOG(NOTICE) << "stop " << strerror(errno); 
					stop=1;					
				}
				else
				{
					int nal_start = 0, nal_end = 0;
					uint8_t* p = (uint8_t*)buffer;
					LOG(NOTICE) << "size:" << rsize;
					if (videoCapture->getFormat() == V4L2_PIX_FMT_H264) {		
						while ((find_nal_unit(p, rsize, &nal_start, &nal_end)>=-1) && (nal_end>nal_start)) {
							p += nal_start;
							read_nal_unit(h264, p, nal_end - nal_start);
							LOG(NOTICE) << "    - nal:" << h264->nal->nal_unit_type;
							
							p += (nal_end - nal_start);
							rsize -= nal_end;
						}
					}
/*					else if (videoCapture->getFormat() == V4L2_PIX_FMT_HEVC) {		
						while (find_nal_unit(p, rsize, &nal_start, &nal_end) > 0) {
							p += nal_start;
							read_debug_hevc_nal_unit(hevc, p, nal_end - nal_start);
							LOG(NOTICE) << "- nal:" << hevc->nal->nal_unit_type;
							
							p += (nal_end - nal_start);
							rsize -= nal_end;
						}
					}*/
				}
			}
			else if (ret == -1)
			{
				LOG(NOTICE) << "stop " << strerror(errno); 
				stop=1;
			}
		}
		

		delete videoCapture;
	}
	
	return 0;
}