/* ========================================================================
 * PROJECT: windage Library
 * ========================================================================
 * This work is based on the original windage Library developed by
 *   Woonhyuk Baek
 *   Woontack Woo
 *   U-VR Lab, GIST of Gwangju in Korea.
 *   http://windage.googlecode.com/
 *   http://uvr.gist.ac.kr/
 *
 * Copyright of the derived and new portions of this work
 *     (C) 2009 GIST U-VR Lab.
 *
 * This framework is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this framework; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * For further information please contact 
 *   Woonhyuk Baek
 *   <windage@live.com>
 *   GIST U-VR Lab.
 *   Department of Information and Communication
 *   Gwangju Institute of Science and Technology
 *   1, Oryong-dong, Buk-gu, Gwangju
 *   South Korea
 * ========================================================================
 ** @author   Woonhyuk Baek
 * ======================================================================== */

#include <iostream>

#include <highgui.h>
#include <windage.h>

#define FLIP
//#define RECTIFICATION
#define ADAPTIVE_THRESHOLD

//#define USE_IMAGE_SEQUENCE

const int OBJECT_COUNT = 4;
const int FIND_FEATURE_COUNT = 10;

const int WIDTH = 640;
const double RATIO =(double)WIDTH / 640.0;
const int HEIGHT = 480*RATIO;

// 4.5mm wide angle
//const double intrinsicValues[8] = {778.195*RATIO, 779.430*RATIO, 324.659*RATIO, 235.685*RATIO, -0.333103, 0.173760, 0.000653, 0.001114};
// 6mm standard angle
const double intrinsicValues[8] = {1029.400*RATIO, 1028.675*RATIO, 316.524*RATIO, 211.395*RATIO, -0.206360, 0.238378, 0.001089, -0.000769};

const int PIP_RATIO = 6;
const int PIP_WIDTH = WIDTH/PIP_RATIO;
const int PIP_HEIGHT = HEIGHT/PIP_RATIO * 2;

void main()
{
	windage::Logger* log = new windage::Logger(&std::cout);
	windage::Logger* fpslog = new windage::Logger(&std::cout);

	// connect camera
	CvCapture* capture = cvCaptureFromCAM(CV_CAP_ANY);
//	cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH, WIDTH);
//	cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, HEIGHT);

	// saving
	bool saving = false;
	CvVideoWriter* writer = NULL;

	char message[100];
	IplImage* inputImage = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_8U, 3);
	IplImage* undistImage = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_8U, 3);
	IplImage* grayImage = cvCreateImage(cvGetSize(inputImage), IPL_DEPTH_8U, 1);
	IplImage* resultImage = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_8U, 3);
	
	IplImage* tempImage = cvCreateImage(cvSize(WIDTH, HEIGHT), IPL_DEPTH_8U, 3);
	IplImage* tempImage2 = cvCreateImage(cvSize(WIDTH, HEIGHT*2), IPL_DEPTH_8U, 3);

	std::vector<IplImage*> referenceImage;
	std::vector<IplImage*> trainingImage;

	// Tracker Initialize
	for(int i=1; i<=OBJECT_COUNT; i++)
	{
		sprintf(message, "reference%d.png", i);
		referenceImage.push_back(cvLoadImage(message));
		sprintf(message, "reference%d_160.png", i);
		trainingImage.push_back(cvLoadImage(message, 0));
	}

	windage::MultipleSURFTracker* multipleTracker = new windage::MultipleSURFTracker();
	multipleTracker->Initialize(intrinsicValues[0], intrinsicValues[1], intrinsicValues[2], intrinsicValues[3], intrinsicValues[4], intrinsicValues[5], intrinsicValues[6], intrinsicValues[7]);
	multipleTracker->InitializeOpticalFlow(WIDTH, HEIGHT, cvSize(8, 8), 3);
	multipleTracker->SetDetectIntervalTime(1.0/1.0);
	multipleTracker->SetPoseEstimationMethod(windage::PROSAC);
	multipleTracker->SetOutlinerRemove(true);
	multipleTracker->SetFeatureExtractThreshold(20);
	for(int i=0; i<trainingImage.size(); i++)
	{
		std::cout << "attatch reference image #" << i << std::endl;
		multipleTracker->AttatchReferenceImage(trainingImage[i], 267.0, 200.0, 4.0, 8);
	}
	//dummmy
/*
	for(int i=0; i<26; i++)
	{		
		multipleTracker->AttatchReferenceImage(trainingImage[OBJECT_COUNT-1], 26.70, 20.00, 4.0, 8);
		std::cout << "attatch dummy reference image #" << i << std::endl;
	}
//*/

	windage::Calibration* calibration = new windage::Calibration();
	calibration->Initialize(intrinsicValues[0], intrinsicValues[1], intrinsicValues[2], intrinsicValues[3], intrinsicValues[4], intrinsicValues[5], intrinsicValues[6], intrinsicValues[7]);
	calibration->InitUndistortionMap(WIDTH, HEIGHT);

	int fastThreshold = 80;
	const int MAX_FAST_THRESHOLD = 80;
	const int MIN_FAST_THRESHOLD = 40;
	const int ADAPTIVE_THRESHOLD_VALUE = 500;
	const int THRESHOLD_STEP = 1;

	IplImage* grabFrame = NULL;
	int featureCount = 0;
	
	fpslog->updateTickCount();
	bool processing = true;
	cvNamedWindow("result");
	while(processing)
	{
		// camera frame grabbing and convert to gray color
		fpslog->log("FPS", fpslog->calculateFPS());
		fpslog->updateTickCount();
		
		log->updateTickCount();
		grabFrame = cvQueryFrame(capture);
		cvFlip(grabFrame, undistImage);
		calibration->Undistortion( undistImage, inputImage);
		cvCvtColor(inputImage, grayImage, CV_BGRA2GRAY);
		cvSmooth(grayImage, grayImage, CV_GAUSSIAN, 3, 3);
		log->log("capture", log->calculateProcessTime());

		// call tracking algorithm
		log->updateTickCount();
		multipleTracker->SetFeatureExtractThreshold(fastThreshold);
		multipleTracker->UpdateCameraPose(grayImage);

		double fps = log->calculateFPS();
		double trackingTime = log->calculateProcessTime();

		// update fast threshold for Adaptive threshold
#ifdef ADAPTIVE_THRESHOLD
		featureCount = multipleTracker->GetFeatureCount();
		if(featureCount > ADAPTIVE_THRESHOLD_VALUE )	fastThreshold = MIN(MAX_FAST_THRESHOLD, fastThreshold+THRESHOLD_STEP);
		else											fastThreshold = MAX(MIN_FAST_THRESHOLD, fastThreshold-THRESHOLD_STEP);
#endif
		
		log->log("tracking", trackingTime);
		log->logNewLine();

//*
		// draw tracking result
		for(int i=0; i<multipleTracker->GetTrackerCount(); i++)
		{
			int matchedCount = multipleTracker->GetMatchedCount(i);
			if(matchedCount > FIND_FEATURE_COUNT)
			{
				multipleTracker->DrawOutLine(inputImage, i, true);
				multipleTracker->DrawInfomation(inputImage, i, 5.0);

				CvPoint center = multipleTracker->GetCameraParameter(i)->ConvertWorld2Image(0.0, 0.0, 0.0);
				
				center.x += 10;
				center.y += 10;
				sprintf(message, "Reference #%d", i);
				windage::Utils::DrawTextToImage(inputImage, center, message);
			}

		}
//		multipleTracker->DrawDebugInfo(inputImage);
//*/
//*
		cvZero(resultImage);
		for(int i=0; i<OBJECT_COUNT; i++)
		{
			cvSetImageROI(tempImage2, cvRect(0, 0, WIDTH, HEIGHT));
			cvCopy(referenceImage[i], tempImage2);
			cvSetImageROI(tempImage2, cvRect(0, HEIGHT, WIDTH, HEIGHT));
			cvCopy(inputImage, tempImage2);
			cvResetImageROI(tempImage2);
			multipleTracker->DrawDebugInfo2(tempImage2, i);

			CvRect rect = cvRect(10 + PIP_WIDTH * i, HEIGHT - 10 - PIP_HEIGHT, PIP_WIDTH, PIP_HEIGHT);
			cvRectangle(resultImage, cvPoint(rect.x, rect.y), cvPoint(rect.x+rect.width, rect.y+rect.height), CV_RGB(255, 255, 255), 5);
			cvSetImageROI(resultImage, rect);
			cvResize(tempImage2, resultImage);
			cvResetImageROI(resultImage);

//			cvNamedWindow("temp");
//			if(multipleTracker->GetMatchedCount(i) > FIND_FEATURE_COUNT)
//				cvShowImage("temp", tempImage2);
		}
//*/
		windage::Utils::CompundImmersiveImage(resultImage, inputImage, CV_RGB(0, 0, 0), 0.75);
//*/
//*
		sprintf(message, "Tracking FPS : %03.2f, Time : %.2f(ms)", fps, trackingTime);
		windage::Utils::DrawTextToImage(inputImage, cvPoint(20, 30), message);
		sprintf(message, "FAST feature count : %d, threashold : %d", featureCount, fastThreshold);
		windage::Utils::DrawTextToImage(inputImage, cvPoint(20, 50), message);
		sprintf(message, "Match count ");
		windage::Utils::DrawTextToImage(inputImage, cvPoint(20, 70), message);
		for(int i=0; i<OBJECT_COUNT; i++)
		{
			sprintf(message, "#%d:%d ", i, multipleTracker->GetMatchedCount(i));
			windage::Utils::DrawTextToImage(inputImage, cvPoint(160 + 65*i, 70), message);
		}
		sprintf(message, "DB count : %d", multipleTracker->GetTrackerCount());
		windage::Utils::DrawTextToImage(inputImage, cvPoint(20, 90), message);
//*/
		if(saving)
		{
			if(writer)
				cvWriteFrame(writer, inputImage);
		}

		char ch = cvWaitKey(1);
		switch(ch)
		{
		case 'q':
		case 'Q':
			processing = false;
			break;
		case ' ':
			cvWaitKey();
			break;
		case 's':
			saving = true;
			if(writer) cvReleaseVideoWriter(&writer);
			writer = cvCreateVideoWriter("saveimage\\capture.avi", CV_FOURCC_DEFAULT, 30, cvSize(inputImage->width, inputImage->height), 1);
			break;
		}

		cvShowImage("result", inputImage);
	}

	if(writer) cvReleaseVideoWriter(&writer);
	cvReleaseCapture(&capture);
}