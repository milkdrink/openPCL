#include "stdafx.h"
#include <pcl/io/openni_grabber.h>
#include "image_viewer.h"

int main()
{
	std::string device_id ("");
	pcl::OpenNIGrabber::Mode image_mode = pcl::OpenNIGrabber::OpenNI_Default_Mode;

	pcl::OpenNIGrabber grabber (device_id, pcl::OpenNIGrabber::OpenNI_Default_Mode, image_mode);

//	grabber.start();

	image_viewer v(grabber);

	v.run();

	while (1)
	{
	}
	
	return 0;
}