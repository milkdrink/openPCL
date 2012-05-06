#include "StdAfx.h"
#include "image_viewer.h"
#include <opencv/cv.h>
#include <highgui.h>

image_viewer::image_viewer(pcl::OpenNIGrabber& grabber)
	: grabber_ (grabber)
{
}

image_viewer::~image_viewer(void)
{
}


void image_viewer::run()
{
	boost::function<void (const boost::shared_ptr<openni_wrapper::Image>&, const boost::shared_ptr<openni_wrapper::DepthImage>&, float) > image_cb = boost::bind (&image_viewer::image_callback, this, _1, _2, _3);
	boost::signals2::connection image_connection = grabber_.registerCallback (image_cb);

	grabber_.start ();

	cv::Mat  m_DepthShow( 480,640,CV_8UC1);
	cv::Mat  m_ImageShow( 480,640,CV_8UC3);

	while (true)
	{
		boost::mutex::scoped_lock lock (image_mutex_);


		if (image_)
		{
			boost::shared_ptr<openni_wrapper::Image> image;
			image.swap (image_);

			m_ImageShow.data = (uchar *)image->getMetaData ().Data ();
			cv::imshow("Image", m_ImageShow);


		}
		if (depth_image_)
		{
			boost::shared_ptr<openni_wrapper::DepthImage> depth_image;
			depth_image.swap (depth_image_);

			m_DepthShow.data = (uchar *)depth_image->getDepthMetaData ().Data ();
			

		}
		cv::imshow("Depth", m_DepthShow);
		cv::waitKey(1);

	}
}