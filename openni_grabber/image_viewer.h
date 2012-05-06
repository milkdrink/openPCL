#pragma once
#include <pcl/io/openni_grabber.h>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>

class image_viewer
{
public:
	image_viewer(pcl::OpenNIGrabber& grabber);
	~image_viewer(void);

	void image_callback (const boost::shared_ptr<openni_wrapper::Image> &image, 
							const boost::shared_ptr<openni_wrapper::DepthImage> &depth_image, float constant)
	{
		//	FPS_CALC ("image callback");
		boost::mutex::scoped_lock lock (image_mutex_);
		image_ = image;
		depth_image_ = depth_image;
	}
	void run();


public:
	boost::shared_ptr<openni_wrapper::Image> image_;
	boost::shared_ptr<openni_wrapper::DepthImage> depth_image_;


	boost::mutex image_mutex_;
	pcl::OpenNIGrabber& grabber_;
};
