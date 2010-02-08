/* ========================================================================
 * PROJECT: windage Library
 * ========================================================================
 * This work is based on the original windage Library developed by
 *   Woonhyuk Baek
 *   Woontack Woo (wwoo@gist.ac.kr)
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
#include <omp.h>

#include <gl/glut.h>
#include <cv.h>
#include <highgui.h>

#include <windage.h>
#include "../Common/OpenGLRenderer.h"

const int WIDTH = 1024;
const int HEIGHT = (WIDTH * 3) / 4;
const int RENDERING_WIDTH = 640;
const int RENDERING_HEIGHT = (RENDERING_WIDTH * 3) / 4;
const double INTRINSIC_VALUES[8] = {WIDTH*1.2, WIDTH*1.2, WIDTH/2, HEIGHT/2, 0, 0, 0, 0};

const int IMAGE_FILE_COUNT = 3;
const char* IMAGE_FILE_NAME = "test%02d.jpg";
//const char* IMAGE_FILE_NAME = "Test/testImage%d.png";
const double VIRTUAL_CAMERA_DISTANCE = 0.5;
const double SCALE_FACTOR = 1.0;

OpenGLRenderer* renderer = NULL;
windage::Calibration* calibration[IMAGE_FILE_COUNT];
windage::Reconstruction::StereoReconstruction* stereo[IMAGE_FILE_COUNT-1];

windage::Logger* logging;
double threshold = 30.0;
double angle = 0.0;
double angleStep = 0.5;

void keyboard(unsigned char ch, int x, int y)
{
	switch(ch)
	{
	case 'q':
	case 'Q':
		cvDestroyAllWindows();
		exit(0);
		break;
	}
}

void idle(void)
{
	angle += angleStep;
	if(angle >= 360.0)
		angle = 0.0;
	glutPostRedisplay();
}

void display()
{
	// clear screen
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// draw virtual object
	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

	double radian = angle * CV_PI / 180.0;
	double dx = sin(radian) * VIRTUAL_CAMERA_DISTANCE;
	double dy = cos(radian) * VIRTUAL_CAMERA_DISTANCE;
//	gluLookAt(dx, dy, VIRTUAL_CAMERA_DISTANCE, 0.0, 0.0, VIRTUAL_CAMERA_DISTANCE/2.0, 0.0, 0.0, 1.0);
	gluLookAt(dx, dy, -VIRTUAL_CAMERA_DISTANCE, 0.0, 0.0, 100.0, 0.0, -1.0, 0.0);
//	gluLookAt(0.0, radian - CV_PI, 0.0, 0.0, 0.0, 100.0, 0.0, -1.0, 0.0);

	glPushMatrix();
	{
		for(int i=0; i<IMAGE_FILE_COUNT-1; i++)
		{
			std::vector<windage::ReconstructionPoint>* point3D = stereo[i]->GetReconstructionPoints();

			glPointSize(5.0f);
			glBegin(GL_POINTS);
			{
				for(unsigned int j=0; j<point3D->size(); j++)
				{
					if((*point3D)[j].IsOutlier() == false)
					{
						CvScalar color = (*point3D)[j].GetColor();
						windage::Vector4 point = (*point3D)[j].GetPoint();
						glColor3f(color.val[2]/255.0, color.val[1]/255.0, color.val[0]/255.0);
						glVertex3f(point.x, point.y, point.z);
					}
				}
			}
			glEnd();
		}

		renderer->DrawAxis((double)RENDERING_WIDTH / 4.0);

	}
	glPopMatrix();

	glutSwapBuffers();
}

void main()
{
	std::vector<IplImage*> inputImage;
	std::vector<IplImage*> grayImage;
	inputImage.resize(IMAGE_FILE_COUNT);
	grayImage.resize(IMAGE_FILE_COUNT);

	std::vector<std::vector<windage::FeaturePoint>> featurePoint;
	featurePoint.resize(IMAGE_FILE_COUNT);

	std::vector<std::vector<windage::FeaturePoint>> matchedPoint;
	matchedPoint.resize((IMAGE_FILE_COUNT-1)*2);

	logging = new windage::Logger(&std::cout);
	logging->log("initialize"); logging->logNewLine();
	
	for(int i=0; i<IMAGE_FILE_COUNT; i++)
	{
		calibration[i] = new windage::Calibration();
		calibration[i]->Initialize(INTRINSIC_VALUES[0], INTRINSIC_VALUES[1], INTRINSIC_VALUES[2], INTRINSIC_VALUES[3]);
	}

	for(int i=0; i<IMAGE_FILE_COUNT-1; i++)
	{
		stereo[i] = new windage::Reconstruction::StereoReconstruction();
		stereo[i]->AttatchBaseCameraParameter(calibration[i]);
		stereo[i]->AttatchUpdateCameraParameter(calibration[i+1]);
	}

	logging->logNewLine();
	logging->log("load image & feature extract - matching"); logging->logNewLine();
	
	char filename[100];
	for(int i=0; i<IMAGE_FILE_COUNT; i++)
	{
		sprintf_s(filename, IMAGE_FILE_NAME, i);
		inputImage[i] = cvLoadImage(filename);
		grayImage[i] = cvCreateImage(cvGetSize(inputImage[i]), IPL_DEPTH_8U, 1);

		cvCvtColor(inputImage[i], grayImage[i], CV_BGR2GRAY);

		logging->log("\tload image "); logging->log(i); logging->log(" : "); logging->log(filename); logging->logNewLine();
	}
	
	logging->logNewLine();
	logging->log("\tfeature extract"); logging->logNewLine();

	#pragma omp parallel for
	for(int i=0; i<IMAGE_FILE_COUNT; i++)
	{
		windage::Algorithms::FeatureDetector* detector = new windage::Algorithms::SIFTdetector();
		detector->DoExtractKeypointsDescriptor(grayImage[i]);
		std::vector<windage::FeaturePoint>* temp = detector->GetKeypoints();

		featurePoint[i].clear();
		for(unsigned int j=0; j<temp->size(); j++)
			featurePoint[i].push_back((*temp)[j]);

		delete detector;

		logging->log("\tkeypoint count "); logging->log(i); logging->log(" : "); logging->log((int)featurePoint[i].size()); logging->logNewLine();
	}

	#pragma omp parallel for
	for(int i=0; i<IMAGE_FILE_COUNT-1; i++)
	{
		logging->logNewLine();
		logging->log("matching "); logging->log(i); logging->log("-"); logging->log(i+1); logging->logNewLine();
		logging->log("\tfeature matching"); logging->logNewLine();

		matchedPoint[2*i].clear();
		matchedPoint[2*i+1].clear();

		windage::Algorithms::SearchTree* searchtree = new windage::Algorithms::FLANNtree(50);
		searchtree->SetRatio(0.6);
		searchtree->Training(&featurePoint[i]);
		for(unsigned int j=0; j<featurePoint[i+1].size(); j++)
		{
			int index = searchtree->Matching(featurePoint[i+1][j]);
			if(index >= 0)
			{
				matchedPoint[2*i].push_back(featurePoint[i][index]);
				matchedPoint[2*i+1].push_back(featurePoint[i+1][j]);
			}
		}
		delete searchtree;

		int matchCount = (int)matchedPoint[2*i].size();
		logging->log("\tmatching count : "); logging->log(matchCount); logging->logNewLine();
	}

	for(int i=0; i<IMAGE_FILE_COUNT-1; i++)
	{
		logging->logNewLine();
		logging->log("reconstruction"); logging->logNewLine();

		stereo[i]->AttatchMatchedPoint1(&matchedPoint[2*i]);
		stereo[i]->AttatchMatchedPoint2(&matchedPoint[2*i+1]);

		double error = 0.0;
		stereo[i]->CalculateNormalizedPoint();
		stereo[i]->ComputeEssentialMatrixRANSAC(&error);

		int inlierCount = stereo[i]->GetInlierCount();
		int matchCount = (int)matchedPoint[2*i].size();
		logging->log("\tinlier ratio : "); logging->log(inlierCount); logging->log("/"); logging->log(matchCount); logging->logNewLine();
		logging->log("\terror : "); logging->log(error); logging->logNewLine();
	
		logging->logNewLine();
		logging->log("color matching"); logging->logNewLine();

		std::vector<windage::ReconstructionPoint>* point3D = stereo[i]->GetReconstructionPoints();
		for(unsigned int j=0; j<point3D->size(); j++)
		{
			if((*point3D)[j].IsOutlier() == false)
			{
				CvScalar color1 = cvGet2D(inputImage[i], matchedPoint[2*i][j].GetPoint().y, matchedPoint[2*i][j].GetPoint().x);
				CvScalar color2 = cvGet2D(inputImage[i+1], matchedPoint[2*i+1][j].GetPoint().y, matchedPoint[2*i+1][j].GetPoint().x);

				(*point3D)[j].SetColor(CV_RGB(	(color1.val[2]+color2.val[2])/2.0,
												(color1.val[1]+color2.val[1])/2.0,
												(color1.val[0]+color2.val[0])/2.0 ) );
			}
		}
	}

	logging->logNewLine();
	logging->log("scale up"); logging->logNewLine();

	for(int i=0; i<IMAGE_FILE_COUNT-1; i++)
	{
		std::vector<windage::ReconstructionPoint>* point3D = stereo[i]->GetReconstructionPoints();
		for(unsigned int j=0; j<point3D->size(); j++)
		{
			(*point3D)[j].SetPoint((*point3D)[j].GetPoint() * SCALE_FACTOR);
		}
	}

	logging->logNewLine();
	logging->log("camera pose information"); logging->logNewLine();
	logging->log("\tintrinsic"); logging->logNewLine();
	logging->log(calibration[0]->GetIntrinsicMatrix());
	for(int i=0; i<IMAGE_FILE_COUNT; i++)
	{
		logging->log("\tcamera parameter "); logging->log(i); logging->logNewLine();
		logging->log(calibration[i]->GetExtrinsicMatrix());
	}
	
	logging->logNewLine();
	logging->log("draw result"); logging->logNewLine();

	// initialize rendering engine using GLUT
	renderer = new OpenGLRenderer();
	renderer->Initialize(RENDERING_WIDTH, RENDERING_HEIGHT, "Sparse Reconstruction");
	
	glutDisplayFunc(display);
	glutIdleFunc(idle);
	glutKeyboardFunc(keyboard);

	glutMainLoop();

	cvDestroyAllWindows();
}