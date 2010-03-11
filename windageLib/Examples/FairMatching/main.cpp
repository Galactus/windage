#include <iostream>
#include <vector>

#include <cv.h>
#include <highgui.h>

#include <windage.h>

const char* IMAGE_FILE_NAME_1 = "Miniature/result.imgr10_COL.jpg";
const char* IMAGE_FILE_NAME_2 = "Miniature/result.imgr11_COL.jpg";
const double REPROJECTION_ERRPR = 5.0;

bool Matching(windage::Algorithms::SearchTree* searchtree, std::vector<windage::FeaturePoint>* feature1, std::vector<windage::FeaturePoint>* feature2, std::vector<windage::FeaturePoint>* matchedPoint1, std::vector<windage::FeaturePoint>* matchedPoint2)
{
	searchtree->Training(feature1);
	for(unsigned int i=0; i<feature2->size(); i++)
	{
		int index = searchtree->Matching((*feature2)[i]);
		if(index >= 0)
		{
			matchedPoint1->push_back((*feature1)[index]);
			matchedPoint2->push_back((*feature2)[i]);
			(*matchedPoint1)[matchedPoint1->size()-1].SetRepositoryID(index);
			(*matchedPoint2)[matchedPoint1->size()-1].SetRepositoryID(i);
		}
	}
	return true;
}

void main()
{
	// capture image
	IplImage* image1 = cvLoadImage(IMAGE_FILE_NAME_1);
	IplImage* image2 = cvLoadImage(IMAGE_FILE_NAME_2);
	IplImage* grayImage1 = cvCreateImage(cvGetSize(image1), IPL_DEPTH_8U, 1);
	IplImage* grayImage2 = cvCreateImage(cvGetSize(image2), IPL_DEPTH_8U, 1);
	IplImage* resultImage = cvCreateImage(cvSize(image1->width*2, image1->height), IPL_DEPTH_8U, 3);

	cvCvtColor(image1, grayImage1, CV_BGR2GRAY);
	cvCvtColor(image2, grayImage2, CV_BGR2GRAY);

	windage::Calibration* calibration = new windage::Calibration();
	windage::Algorithms::FeatureDetector* detector = new windage::Algorithms::SIFTGPUdetector();
	windage::Algorithms::SearchTree* tree = new windage::Algorithms::FLANNtree();
	windage::Algorithms::HomographyEstimator* estimator = new windage::Algorithms::RANSACestimator();
	windage::Algorithms::OutlierChecker* checker = new windage::Algorithms::OutlierChecker();
	
	std::vector<windage::FeaturePoint>* feature = NULL;
	std::vector<windage::FeaturePoint> feature1;
	std::vector<windage::FeaturePoint> feature2;
	std::vector<windage::FeaturePoint> matching1;
	std::vector<windage::FeaturePoint> matching2;

	calibration->Initialize(608.894958, 609.015991, 295.023712, 254.171387, 0, 0, 0, 0);
	estimator->AttatchCameraParameter(calibration);
	estimator->AttatchReferencePoint(&matching1);
	estimator->AttatchScenePoint(&matching2);
	estimator->SetReprojectionError(REPROJECTION_ERRPR);

	checker->AttatchEstimator(estimator);
	checker->SetReprojectionError(REPROJECTION_ERRPR);

	cvNamedWindow("result");


	bool processing = true;
	while(processing)
	{
		feature1.clear();
		feature2.clear();
		matching1.clear();
		matching2.clear();
		
		detector->DoExtractKeypointsDescriptor(grayImage1);
		feature = detector->GetKeypoints();
		for(unsigned int i=0; i<feature->size(); i++)
		{
			feature1.push_back((*feature)[i]);
		}

		detector->DoExtractKeypointsDescriptor(grayImage2);
		feature = detector->GetKeypoints();
		for(unsigned int i=0; i<feature->size(); i++)
		{
			feature2.push_back((*feature)[i]);
		}

		Matching(tree, &feature1, &feature2, &matching1, &matching2);
		estimator->Calculate();
		checker->Calculate();

		cvSetImageROI(resultImage, cvRect(0, 0, image1->width, image1->height));
		cvCopyImage(image1, resultImage);
		cvSetImageROI(resultImage, cvRect(image1->width, 0, image1->width, image1->height));
		cvCopyImage(image2, resultImage);
		cvResetImageROI(resultImage);

		for(unsigned int i=0; i<matching1.size(); i++)
		{
			if(matching1[i].IsOutlier() == false)
				cvLine(resultImage, cvPoint(matching1[i].GetPoint().x, matching1[i].GetPoint().y), cvPoint(image1->width + matching2[i].GetPoint().x, matching2[i].GetPoint().y), CV_RGB(255, 0, 0));
		}
		
		cvShowImage("result", resultImage);

		char ch = cvWaitKey(1);
		switch(ch)
		{
		case 'q':
		case 'Q':
		case 27:
			processing = false;
			break;
		}
	}

	cvDestroyAllWindows();
}
