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

#include "PGRCamera.h"

CPGRCamera::CPGRCamera()
{
	pixelsize = 4;
	_buffer = NULL;
	m_Image = NULL;
}


CPGRCamera::~CPGRCamera()
{
	close();
}

void CPGRCamera::open() 
{
	// create a camera context
	_error = flycaptureCreateContext(&_context);
	handleError("flycaptureCreateContext()");

	_gui_error = pgrcamguiCreateContext(&_gui_context);
	handleGuiError("pgrcamguiCreateContext()");

	// show the camera selection dialog
	unsigned long serial_number;
	int dialog_status;
	_gui_error = pgrcamguiShowCameraSelectionModal(_gui_context, _context, &serial_number, &dialog_status);
	handleGuiError("pgrcamguiShowCameraSelectionModal()");

	// initialize the selected camera
	if (serial_number != 0) 
	{
		_error = flycaptureInitializeFromSerialNumber(_context, serial_number);
		handleError("flycaptureInitializeFromSerialNumber()");
	} else 
	{
		exit(1);
	}

	// display camera information
	FlyCaptureInfoEx info;
	_error = flycaptureGetCameraInfo(_context, &info);
	handleError("flycaptureGetCameraInfo()");
	
	// set video mode and frame rate according to the context
	FlyCaptureVideoMode pgr_video_mode;
	FlyCaptureFrameRate pgr_frame_rate;
	_error = flycaptureGetCurrentVideoMode(_context, &pgr_video_mode, &pgr_frame_rate);
	handleError("flycaptureGetCurrentVideoMode()");

	// store video mode information
	switch (pgr_video_mode) 
	{
	case FLYCAPTURE_VIDEOMODE_640x480Y8:
	case FLYCAPTURE_VIDEOMODE_640x480Y16:
		xsize = 640;
		ysize = 480;
		break;

	case FLYCAPTURE_VIDEOMODE_800x600Y8:
	case FLYCAPTURE_VIDEOMODE_800x600Y16:
		xsize = 800;
		ysize = 600;
		break;

	case FLYCAPTURE_VIDEOMODE_1024x768Y8:
	case FLYCAPTURE_VIDEOMODE_1024x768Y16:
		xsize = 1024;
		ysize = 768;
		break;

	default:
		exit(1);
	}

	framerate = pgr_frame_rate;
	// create a buffer
	if(_buffer) 
	{
		delete[] _buffer;
	}
	if(m_Image)
	{
		cvReleaseImage(&m_Image);
	}

	_buffer = new unsigned char[xsize * ysize * pixelsize];
	m_Image = cvCreateImage(cvSize(xsize, ysize), IPL_DEPTH_8U, pixelsize);
}


void CPGRCamera::close() 
{
	if (_context != NULL) 
	{
		// destroy the camera context
		_error = flycaptureDestroyContext(_context);
		handleError("flycaptureDestroyContext()");

		_context = NULL;

		// delete the buffer
		if (_buffer) 
		{
			delete[] _buffer;
			_buffer = NULL;
		}

		if(m_Image)
		{
			cvReleaseImage(&m_Image);
		}
	}
}


void CPGRCamera::start() 
{
	// start camera capture
	_error = flycaptureStart(_context, FLYCAPTURE_VIDEOMODE_ANY, FLYCAPTURE_FRAMERATE_ANY);
	handleError("flycaptureStart()");
}


void CPGRCamera::stop() 
{
	// stop camera capture
	if(m_Image != NULL)
	{
		_error = flycaptureStop(_context);
		handleError("flycaptureStop()");
	}
}


void CPGRCamera::update() 
{
	// grab a raw image, which might be in stippled Bayer format
	FlyCaptureImage raw_image;
	_error = flycaptureGrabImage2(_context, &raw_image);
	handleError("flycaptureGrabImage2()");

	// convert it to BGRU32 format which is color
	FlyCaptureImage converted_image;
	if(raw_image.bStippled) 
	{
		converted_image.pData = _buffer;
		converted_image.pixelFormat = FLYCAPTURE_BGRU;
		_error = flycaptureConvertImage(_context, &raw_image, &converted_image);
		handleError("flycaptureConvertImage()");
	} 
	else 
	{
		converted_image = raw_image;
	}

	if(_buffer) 
	{
		memcpy(m_Image->imageData, converted_image.pData, sizeof(unsigned char)*xsize*ysize*pixelsize);
	}
}

void CPGRCamera::handleError(std::string function) 
{
	// useless
	if (_error != FLYCAPTURE_OK) 
	{
		exit(1);
	}
}


void CPGRCamera::handleGuiError(std::string function) 
{
	if (_gui_error != PGRCAMGUI_OK) 
	{
		exit(1);
	}
}
