/*=============================================================================
  Copyright (C) 2012 Allied Vision Technologies.  All Rights Reserved.

  Redistribution of this file, in original or modified form, without
  prior written consent of Allied Vision Technologies is prohibited.

-------------------------------------------------------------------------------

  File:        CameraObserver.cpp

  Description: The camera observer that is used for notifications
               regarding a change in the camera list.

-------------------------------------------------------------------------------

  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF TITLE,
  NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR  PURPOSE ARE
  DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================*/

#include "V4l2Helper.h"
#include "Camera.h"
#include "Logger.h"
#include <stdio.h>
#include <sstream>
#include <vector>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#include "Helper.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

namespace AVT {
namespace Tools {
namespace Examples {

////////////////////////////////////////////////////////////////////////////
// Implementation
////////////////////////////////////////////////////////////////////////////    

Camera::Camera()
    : m_bSIRunning(false)
    , m_UsedBufferCount(0)
    , m_nFileDescriptor(-1)
{
    connect(&m_DmaDeviceDiscoveryCallbacks, SIGNAL(OnCameraListChanged_Signal(const int &, unsigned int, unsigned long long, const QString &)), this, SLOT(OnCameraListChanged(const int &, unsigned int, unsigned long long, const QString &)));

    connect(&m_DmaSICallbacks, SIGNAL(OnFrameReady_Signal(const QImage &, const unsigned long long &)), this, SLOT(OnFrameReady(const QImage &, const unsigned long long &)));
	connect(&m_DmaSICallbacks, SIGNAL(OnFrameDone_Signal(const unsigned long long)), this, SLOT(OnFrameDone(const unsigned long long)));
    connect(&m_DmaSICallbacks, SIGNAL(OnRecordFrame_Signal(const unsigned long long &, const unsigned long long &)), this, SLOT(OnRecordFrame(const unsigned long long &, const unsigned long long &)));
    connect(&m_DmaSICallbacks, SIGNAL(OnDisplayFrame_Signal(const unsigned long long &, const unsigned long &, const unsigned long &, const unsigned long &)), this, SLOT(OnDisplayFrame(const unsigned long long &, const unsigned long &, const unsigned long &, const unsigned long &)));
    connect(&m_DmaSICallbacks, SIGNAL(OnMessage_Signal(const QString &)), this, SLOT(OnMessage(const QString &)));

}

Camera::~Camera()
{
    m_DmaDeviceDiscoveryCallbacks.SetTerminateFlag();
    m_DmaSICallbacks.SetTerminateFlag();

    CloseDevice();
}

unsigned int Camera::GetReceivedFramesCount()
{
    return m_DmaSICallbacks.GetReceivedFramesCount();
}

unsigned int Camera::GetIncompletedFramesCount()
{
    return m_DmaSICallbacks.GetIncompletedFramesCount();
}

int Camera::OpenDevice(std::string &deviceName)
{
	int result = -1;
	
	if (-1 == m_nFileDescriptor)
	{
		if ((m_nFileDescriptor = open(deviceName.c_str(), O_RDWR | O_NONBLOCK, 0)) == -1) 
		{
			Logger::LogEx("Camera::OpenDevice open '%s' failed errno=%d=%s", deviceName.c_str(), errno, V4l2Helper::ConvertErrno2String(errno).c_str());
			emit OnCameraError_Signal("OpenDevice: open '" + QString(deviceName.c_str()) + "' failed errno=" + QString((int)errno) + "=" + QString(V4l2Helper::ConvertErrno2String(errno).c_str()) + ".");
		}
		else
		{
			Logger::LogEx("Camera::OpenDevice open %s OK", deviceName.c_str());
			emit OnCameraMessage_Signal("OpenDevice: open " + QString(deviceName.c_str()) + " OK");
	
			m_DeviceName = deviceName;
	
			result = 0;
		}
	}
	else
	{
		Logger::LogEx("Camera::OpenDevice open %s failed because %s is already open", deviceName.c_str(), m_DeviceName.c_str());
		emit OnCameraError_Signal("OpenDevice: open " + QString(deviceName.c_str()) + " failed because " + QString(m_DeviceName.c_str()) + " is already open");
	}
	
	return result;
}

int Camera::CloseDevice()
{
    int result = -1;

    if (-1 != m_nFileDescriptor)
    {
        if (-1 == close(m_nFileDescriptor))
		{
			Logger::LogEx("Camera::CloseDevice close '%s' failed errno=%d=%s", m_DeviceName.c_str(), errno, V4l2Helper::ConvertErrno2String(errno).c_str());
			emit OnCameraError_Signal("CloseDevice: close '" + QString(m_DeviceName.c_str()) + "' failed errno=" + QString((int)errno) + "+" + QString(V4l2Helper::ConvertErrno2String(errno).c_str()) + ".");
		}
		else
		{
			Logger::LogEx("Camera::CloseDevice close %s OK", m_DeviceName.c_str());
			emit OnCameraMessage_Signal("CloseDevice: close " + QString(m_DeviceName.c_str()) + " OK");
		
			result = 0;
		}
    }
    
    m_nFileDescriptor = -1;
	
    return result;
}

/********************************************************************************/
// slots
/********************************************************************************/

// The event handler to show the processed frame
void Camera::OnFrameReady(const QImage &image, const unsigned long long &frameId)
{
	emit OnCameraFrameReady_Signal(image, frameId);
}

// The event handler to return the frame
void Camera::OnFrameDone(const unsigned long long frameHandle)
{
    if (m_bSIRunning)
    {
		for (int i = 0; i < m_UsedBufferCount; ++i)
		{
			if (frameHandle == (uint64_t)m_UserBufferContainerList[i]->pBuffer)
			{
                                QueueSingleUserBuffer(i);
				
				break;
			}
		}
        		
        
    }
}

// Event will be called when the a frame is recorded
void Camera::OnRecordFrame(const unsigned long long &frameID, const unsigned long long &framesInQueue)
{
    emit OnCameraRecordFrame_Signal(frameID, framesInQueue);
}

// Event will be called when the a frame is displayed
void Camera::OnDisplayFrame(const unsigned long long &frameID, const unsigned long &width, const unsigned long &height, const unsigned long &pixelformat)
{
    emit OnCameraDisplayFrame_Signal(frameID, width, height, pixelformat);
}

// Event will be called when for text notification
void Camera::OnMessage(const QString &msg)
{
    emit OnCameraMessage_Signal(msg);
}

// The event handler to set or remove devices 
void Camera::OnCameraListChanged(const int &reason, unsigned int cardNumber, unsigned long long deviceID, const QString &deviceName)
{
    emit OnCameraListChanged_Signal(reason, cardNumber, deviceID, deviceName);
}

/********************************************************************************/
// Device discovery
/********************************************************************************/

int Camera::DeviceDiscoveryStart()
{
    uint32_t device_count = 0;
    int nResult = 0;
	QString deviceName;
	int fileDiscriptor = -1;
	
	do
	{
		deviceName.sprintf("/dev/video%d", device_count);
		
		if ((fileDiscriptor = open(deviceName.toStdString().c_str(), O_RDWR)) == -1) 
		{
			Logger::LogEx("Camera::DeviceDiscoveryStart open %s failed", deviceName.toAscii().data());
			emit OnCameraError_Signal("DeviceDiscoveryStart: open " + deviceName + " failed");
		}
		else
		{
			struct v4l2_capability cap;
						
			Logger::LogEx("Camera::DeviceDiscoveryStart open %s found", deviceName.toAscii().data());
			emit OnCameraMessage_Signal("DeviceDiscoveryStart: open " + deviceName + " found");
			
			// queuery device capabilities
			if (-1 == V4l2Helper::xioctl(fileDiscriptor, VIDIOC_QUERYCAP, &cap)) 
			{
				Logger::LogEx("Camera::DeviceDiscoveryStart %s is no V4L2 device", deviceName.toAscii().data());
				emit OnCameraError_Signal("DeviceDiscoveryStart: " + deviceName + " is no V4L2 device");
			}
			else
			{
				if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) 
				{
					Logger::LogEx("Camera::DeviceDiscoveryStart %s is no video capture device", deviceName.toAscii().data());
					emit OnCameraError_Signal("DeviceDiscoveryStart: " + deviceName + " is no video capture device");
				}
			}
			
			
			if (-1 == close(fileDiscriptor))
			{
				Logger::LogEx("Camera::DeviceDiscoveryStart close %s failed", deviceName.toAscii().data());
				emit OnCameraError_Signal("DeviceDiscoveryStart: close " + deviceName + " failed");
			}
			else
			{
				emit OnCameraListChanged_Signal(UpdateTriggerPluggedIn, 0, device_count, deviceName);
			
				Logger::LogEx("Camera::DeviceDiscoveryStart close %s OK", deviceName.toAscii().data());
				emit OnCameraMessage_Signal("DeviceDiscoveryStart: close " + deviceName + " OK");
			}
		}
		
		device_count++;
	}
	while(device_count < 20);
	
    //m_DmaDeviceDiscoveryCallbacks.Start();

    return 0;
}

int Camera::DeviceDiscoveryStop()
{
    Logger::LogEx("Device Discovery stopped.");

    //m_DmaDeviceDiscoveryCallbacks.Stop();

    return 0;
}
    
/********************************************************************************/
// SI helper
/********************************************************************************/


int Camera::SIStartChannel(uint32_t pixelformat, uint32_t payloadsize, uint32_t width, uint32_t height, uint32_t bytesPerLine, void *pPrivateData)
{
    int nResult = 0;
    
    Logger::LogEx("Camera::SIStartChannel pixelformat=%d, payloadsize=%d, width=%d, height=%d.", pixelformat, payloadsize, width, height);
	
	m_DmaSICallbacks.SetParameter(m_nFileDescriptor, pixelformat, payloadsize, width, height, bytesPerLine);

    m_DmaSICallbacks.ResetIncompletedFramesCount();
	
	m_bSIRunning = true;

    return nResult;
}

int Camera::SIStopChannel()
{
    int nResult = 0;
    
    m_bSIRunning = false;

    Logger::LogEx("Camera::SIStopChannel.");
    
    return nResult;
}

/*********************************************************************************************************/
// DCI commands
/*********************************************************************************************************/

int Camera::SendAcquisitionStart()
{
	int result = -1;
	v4l2_buf_type type;
	
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_STREAMON, &type))
	{
        Logger::LogEx("Camera::SendAcquisitionStart VIDIOC_STREAMON failed");
        emit OnCameraError_Signal("SendAcquisitionStart: VIDIOC_STREAMON failed.");
	}
	else
	{
        Logger::LogEx("Camera::SendAcquisitionStart VIDIOC_STREAMON OK");
        emit OnCameraMessage_Signal("SendAcquisitionStart: VIDIOC_STREAMON OK.");
		
		result = 0;
	}
	
	return result;
}

int Camera::SendAcquisitionStop()
{
	int result = -1;
	v4l2_buf_type type;
	
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	
	if (-1 == V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_STREAMOFF, &type))
	{
        Logger::LogEx("Camera::SendAcquisitionStop VIDIOC_STREAMOFF failed");
        emit OnCameraError_Signal("SendAcquisitionStop: VIDIOC_STREAMOFF failed.");
	}
	else
	{
        Logger::LogEx("Camera::SendAcquisitionStop VIDIOC_STREAMOFF OK");
        emit OnCameraMessage_Signal("SendAcquisitionStop: VIDIOC_STREAMOFF OK.");
		
		result = 0;
	}
	
	return result;
}

int Camera::ReadPayloadsize(uint32_t &payloadsize)
{
    int result = -1;
	v4l2_format fmt;
	
	CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_G_FMT, &fmt))
	{                
        Logger::LogEx("Camera::ReadPayloadsize VIDIOC_G_FMT OK =%d", fmt.fmt.pix.sizeimage);
        emit OnCameraMessage_Signal(QString("ReadPayloadsize VIDIOC_G_FMT: OK =%1.").arg(fmt.fmt.pix.sizeimage));
		
		payloadsize = fmt.fmt.pix.sizeimage;
		
		result = 0;
	}
	else
	{
		Logger::LogEx("Camera::ReadPayloadsize VIDIOC_G_FMT failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
        emit OnCameraError_Signal(QString("ReadPayloadsize VIDIOC_G_FMT: failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
	}
	
    return result;
}

int Camera::ReadFrameSize(uint32_t &width, uint32_t &height)
{
    int result = -1;
	v4l2_format fmt;
	
	CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_G_FMT, &fmt))
	{       
        Logger::LogEx("Camera::ReadFrameSize VIDIOC_G_FMT OK =%dx%d", fmt.fmt.pix.width, fmt.fmt.pix.height);
        emit OnCameraMessage_Signal(QString("ReadFrameSize VIDIOC_G_FMT: OK =%1x%2.").arg(fmt.fmt.pix.width).arg(fmt.fmt.pix.height));
         
		width = fmt.fmt.pix.width;
		height = fmt.fmt.pix.height;
		
		result = 0;
	}
	else
	{
		Logger::LogEx("Camera::ReadFrameSize VIDIOC_G_FMT failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
        emit OnCameraError_Signal(QString("ReadFrameSize VIDIOC_G_FMT: failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
	}
	
    return result;
}

int Camera::SetFrameSize(uint32_t width, uint32_t height)
{
	int result = -1;
	v4l2_format fmt;
	
	CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_G_FMT, &fmt))
	{                
		Logger::LogEx("Camera::SetFrameSize VIDIOC_G_FMT OK");
		emit OnCameraError_Signal(QString("SetFrameSize VIDIOC_G_FMT: OK."));

		fmt.fmt.pix.width = width;
		fmt.fmt.pix.height = height;
	
		if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_S_FMT, &fmt))
		{                
			Logger::LogEx("Camera::SetFrameSize VIDIOC_S_FMT OK =%dx%d", width, height);
			emit OnCameraMessage_Signal(QString("SetFrameSize VIDIOC_S_FMT: OK =%1x%2.").arg(width).arg(height));

			result = 0;
		}
	}
	else
	{
		Logger::LogEx("Camera::SetFrameSize VIDIOC_G_FMT failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
        emit OnCameraError_Signal(QString("SetFrameSize VIDIOC_G_FMT: failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
	}
	
    return result;
}

int Camera::SetWidth(uint32_t width)
{
    int result = -1;
	v4l2_format fmt;
	
	CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_G_FMT, &fmt))
	{       
		Logger::LogEx("Camera::SetWidth VIDIOC_G_FMT OK");
		emit OnCameraError_Signal(QString("SetWidth VIDIOC_G_FMT: OK."));
		
		fmt.fmt.pix.width = width;
	
		if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_S_FMT, &fmt))
		{             
			Logger::LogEx("Camera::SetWidth VIDIOC_S_FMT OK =%d", width);
			emit OnCameraMessage_Signal(QString("SetWidth VIDIOC_S_FMT: OK =%1.").arg(width));   
			
			result = 0;
		}
	}
	else
	{
		Logger::LogEx("Camera::SetWidth VIDIOC_G_FMT failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
        emit OnCameraError_Signal(QString("SetWidth VIDIOC_G_FMT: failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
	}
	
    return result;
}

int Camera::ReadWidth(uint32_t &width)
{
    int result = -1;
	v4l2_format fmt;
	
	CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_G_FMT, &fmt))
	{                
        Logger::LogEx("Camera::ReadWidth VIDIOC_G_FMT OK =%d", width);
        emit OnCameraMessage_Signal(QString("ReadWidth VIDIOC_G_FMT: OK =%1.").arg(width));

		width = fmt.fmt.pix.width;
		
		result = 0;
	}
	else
	{
		Logger::LogEx("Camera::ReadWidth VIDIOC_G_FMT failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
        emit OnCameraError_Signal(QString("ReadWidth VIDIOC_G_FMT: failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
	}
	
    return result;
}

int Camera::SetHeight(uint32_t height)
{
    int result = -1;
	v4l2_format fmt;
	
	CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_G_FMT, &fmt))
	{                
		Logger::LogEx("Camera::SetHeight VIDIOC_G_FMT OK");
		emit OnCameraError_Signal(QString("SetHeight VIDIOC_G_FMT: OK."));

		fmt.fmt.pix.height = height;

		if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_S_FMT, &fmt))
		{
            Logger::LogEx("Camera::SetHeight VIDIOC_S_FMT OK =%d", height);
			emit OnCameraMessage_Signal(QString("SetHeight VIDIOC_S_FMT: OK =%1.").arg(height));
                
			result = 0;
		}
	}
	else
	{
		Logger::LogEx("Camera::SetHeight VIDIOC_G_FMT failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
        emit OnCameraError_Signal(QString("SetHeight VIDIOC_G_FMT: failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
	}
	
    return result;
}

int Camera::ReadHeight(uint32_t &height)
{
    int result = -1;
	v4l2_format fmt;
	
	CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_G_FMT, &fmt))
	{       
        Logger::LogEx("Camera::ReadHeight VIDIOC_G_FMT OK =%d", fmt.fmt.pix.height);
        emit OnCameraMessage_Signal(QString("ReadHeight VIDIOC_G_FMT: OK =%1.").arg(fmt.fmt.pix.height));
         
		height = fmt.fmt.pix.height;
		
		result = 0;
	}
	else
	{
		Logger::LogEx("Camera::ReadHeight VIDIOC_G_FMT failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
        emit OnCameraError_Signal(QString("ReadHeight VIDIOC_G_FMT: failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
	}
	
    return result;
}

int Camera::ReadFormats()
{
    int result = -1;
	v4l2_fmtdesc fmt;
	v4l2_frmsizeenum fmtsize;
	
	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	while (V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_ENUM_FMT, &fmt) >= 0 && fmt.index <= 100)
	{
		std::string tmp = (char*)fmt.description;
		Logger::LogEx("Camera::ReadFormats VIDIOC_ENUM_FMT index = %d", fmt.index);
		emit OnCameraMessage_Signal("ReadFormats VIDIOC_ENUM_FMT: index = " + QString("%1").arg(fmt.index) + ".");
		Logger::LogEx("Camera::ReadFormats VIDIOC_ENUM_FMT type = %d", fmt.type);
		emit OnCameraMessage_Signal("ReadFormats VIDIOC_ENUM_FMT: type = " + QString("%1").arg(fmt.type) + ".");
		Logger::LogEx("Camera::ReadFormats VIDIOC_ENUM_FMT pixelformat = %d = %s", fmt.pixelformat, V4l2Helper::ConvertPixelformat2EnumString(fmt.pixelformat).c_str());
		emit OnCameraMessage_Signal("ReadFormats VIDIOC_ENUM_FMT: pixelformat = " + QString("%1").arg(fmt.pixelformat) + " = " + QString(V4l2Helper::ConvertPixelformat2EnumString(fmt.pixelformat).c_str()) + ".");
		Logger::LogEx("Camera::ReadFormats VIDIOC_ENUM_FMT description = %s", fmt.description);
		emit OnCameraMessage_Signal("ReadFormats VIDIOC_ENUM_FMT: description = " + QString(tmp.c_str()) + ".");
		
		emit OnCameraPixelformat_Signal(QString("%1").arg(QString(V4l2Helper::ConvertPixelformat2String(fmt.pixelformat).c_str())));
				
		CLEAR(fmtsize);
		fmtsize.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmtsize.pixel_format = fmt.pixelformat;
		fmtsize.index = 0;
		while (V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_ENUM_FRAMESIZES, &fmtsize) >= 0 && fmtsize.index <= 100)
		{              
			if (fmtsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
			{
				v4l2_frmivalenum fmtival;
				
				Logger::LogEx("Camera::ReadFormats VIDIOC_ENUM_FRAMESIZES size enum discrete width = %d", fmtsize.discrete.width);
				emit OnCameraMessage_Signal("ReadFormats VIDIOC_ENUM_FRAMESIZES size enum discrete width = " + QString("%1").arg(fmtsize.discrete.width) + ".");
				Logger::LogEx("Camera::ReadFormats size VIDIOC_ENUM_FRAMESIZES enum discrete height = %d", fmtsize.discrete.height);
				emit OnCameraMessage_Signal("ReadFormats VIDIOC_ENUM_FRAMESIZES size enum discrete height = " + QString("%1").arg(fmtsize.discrete.height) + ".");
				
				emit OnCameraFramesize_Signal(QString("disc:%1x%2").arg(fmtsize.discrete.width).arg(fmtsize.discrete.height));
				
				CLEAR(fmtival);
				fmtival.index = 0;
				fmtival.pixel_format = fmt.pixelformat;
				fmtival.width = fmtsize.discrete.width;
				fmtival.height = fmtsize.discrete.height;
				while (V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_ENUM_FRAMEINTERVALS, &fmtival) >= 0)
				{
					// TODO
					//Logger::LogEx("Camera::ReadFormat ival type= %d", fmtival.type);
					//emit OnCameraMessage_Signal("ReadFormatsiz ival type = " + QString("%1").arg(fmtival.type) + ".");
					fmtival.index++;
				}
			} 
			else if (fmtsize.type == V4L2_FRMSIZE_TYPE_STEPWISE)
			{
				Logger::LogEx("Camera::ReadFormats VIDIOC_ENUM_FRAMESIZES size enum stepwise min_width = %d", fmtsize.stepwise.min_width);
				emit OnCameraMessage_Signal("ReadFormats VIDIOC_ENUM_FRAMESIZES size enum stepwise min_width = " + QString("%1").arg(fmtsize.stepwise.min_width) + ".");
				Logger::LogEx("Camera::ReadFormats VIDIOC_ENUM_FRAMESIZES size enum stepwise min_height = %d", fmtsize.stepwise.min_height);
				emit OnCameraMessage_Signal("ReadFormats VIDIOC_ENUM_FRAMESIZES size enum stepwise min_height = " + QString("%1").arg(fmtsize.stepwise.min_height) + ".");
				Logger::LogEx("Camera::ReadFormats VIDIOC_ENUM_FRAMESIZES size enum stepwise max_width = %d", fmtsize.stepwise.max_width);
				emit OnCameraMessage_Signal("ReadFormats VIDIOC_ENUM_FRAMESIZES size enum stepwise max_width = " + QString("%1").arg(fmtsize.stepwise.max_width) + ".");
				Logger::LogEx("Camera::ReadFormats VIDIOC_ENUM_FRAMESIZES size enum stepwise max_height = %d", fmtsize.stepwise.max_height);
				emit OnCameraMessage_Signal("ReadFormats VIDIOC_ENUM_FRAMESIZES size enum stepwise max_height = " + QString("%1").arg(fmtsize.stepwise.max_height) + ".");
				Logger::LogEx("Camera::ReadFormats VIDIOC_ENUM_FRAMESIZES size enum stepwise step_width = %d", fmtsize.stepwise.step_width);
				emit OnCameraMessage_Signal("ReadFormats VIDIOC_ENUM_FRAMESIZES size enum stepwise step_width = " + QString("%1").arg(fmtsize.stepwise.step_width) + ".");
				Logger::LogEx("Camera::ReadFormats VIDIOC_ENUM_FRAMESIZES size enum stepwise step_height = %d", fmtsize.stepwise.step_height);
				emit OnCameraMessage_Signal("ReadFormats VIDIOC_ENUM_FRAMESIZES size enum stepwise step_height = " + QString("%1").arg(fmtsize.stepwise.step_height) + ".");
				
				emit OnCameraFramesize_Signal(QString("min:%1x%2,max:%3x%4,step:%5x%6").arg(fmtsize.stepwise.min_width).arg(fmtsize.stepwise.min_height).arg(fmtsize.stepwise.max_width).arg(fmtsize.stepwise.max_height).arg(fmtsize.stepwise.step_width).arg(fmtsize.stepwise.step_height));
			}
			
			result = 0;
			
			fmtsize.index++;
		}
		
		if (fmtsize.index >= 100)
		{
			Logger::LogEx("Camera::ReadFormats no VIDIOC_ENUM_FRAMESIZES never terminated with EINVAL within 100 loops.");
		    emit OnCameraError_Signal("ReadFormats: no VIDIOC_ENUM_FRAMESIZES never terminated with EINVAL within 100 loops.");
        }
		
		fmt.index++;
	}

	if (fmt.index >= 100)
	{
		Logger::LogEx("Camera::ReadFormats no VIDIOC_ENUM_FMT never terminated with EINVAL within 100 loops.");
		emit OnCameraError_Signal("ReadFormats: get VIDIOC_ENUM_FMT never terminated with EINVAL within 100 loops.");
	}
	
    return result;
}

int Camera::SetPixelformat(uint32_t pixelformat, QString pfText)
{
    int result = -1;
	v4l2_format fmt;
	
	CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_G_FMT, &fmt))
	{                
		Logger::LogEx("Camera::SetPixelformat VIDIOC_G_FMT OK");
		emit OnCameraError_Signal(QString("SetPixelformat VIDIOC_G_FMT: OK."));

		fmt.fmt.pix.pixelformat = pixelformat;
		
		if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_S_FMT, &fmt))
		{                
			Logger::LogEx("Camera::SetPixelformat VIDIOC_S_FMT to %d OK", pixelformat);
			emit OnCameraMessage_Signal(QString("SetPixelformat VIDIOC_S_FMT: to %1 OK.").arg(pixelformat));

			result = 0;
		}
	}
	else
	{
		Logger::LogEx("Camera::SetPixelformat VIDIOC_G_FMT failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
        emit OnCameraError_Signal(QString("SetPixelformat VIDIOC_G_FMT: failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
	}
	
    return result;
}

int Camera::ReadPixelformat(uint32_t &pixelformat, uint32_t &bytesPerLine, QString &pfText)
{
    int result = -1;
	v4l2_format fmt;
	
	CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_G_FMT, &fmt))
	{                
		Logger::LogEx("Camera::ReadPixelformat VIDIOC_G_FMT OK =%d", fmt.fmt.pix.pixelformat);
        emit OnCameraMessage_Signal(QString("ReadPixelformat VIDIOC_G_FMT: OK =%1.").arg(fmt.fmt.pix.pixelformat));

		pixelformat = fmt.fmt.pix.pixelformat;
		bytesPerLine = fmt.fmt.pix.bytesperline;
		pfText = QString(V4l2Helper::ConvertPixelformat2EnumString(fmt.fmt.pix.pixelformat).c_str());
		
		result = 0;
	}
	else
	{
		Logger::LogEx("Camera::ReadPixelformat VIDIOC_G_FMT failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
        emit OnCameraError_Signal(QString("ReadPixelformat VIDIOC_G_FMT: failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
	}
	
    return result;
}

int Camera::ReadGain(uint32_t &gain)
{
    int result = -1;
	v4l2_queryctrl ctrl;
	
	CLEAR(ctrl);
	ctrl.id = V4L2_CID_GAIN;
	
	if (V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_QUERYCTRL, &ctrl) >= 0)
	{
		v4l2_control fmt;

		Logger::LogEx("Camera::ReadGain VIDIOC_QUERYCTRL V4L2_CID_GAIN OK");
		emit OnCameraMessage_Signal(QString("ReadGain VIDIOC_QUERYCTRL V4L2_CID_GAIN: OK."));

		CLEAR(fmt);
		fmt.id = V4L2_CID_GAIN;

		if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_G_CTRL, &fmt))
		{       
			Logger::LogEx("Camera::ReadGain VIDIOC_G_CTRL V4L2_CID_GAIN OK =%d", fmt.value);
			emit OnCameraMessage_Signal(QString("ReadGain VIDIOC_G_CTRL: V4L2_CID_GAIN OK =%1.").arg(fmt.value));
         
			gain = fmt.value;
			
			result = 0;
		}
		else
		{
			Logger::LogEx("Camera::ReadGain VIDIOC_G_CTRL V4L2_CID_GAIN failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
			emit OnCameraError_Signal(QString("ReadGain VIDIOC_G_CTRL: V4L2_CID_GAIN failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
		}
	}
	else
	{
		Logger::LogEx("Camera::ReadGain VIDIOC_QUERYCTRL V4L2_CID_GAIN failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
		emit OnCameraMessage_Signal(QString("ReadGain VIDIOC_QUERYCTRL V4L2_CID_GAIN: failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
		
		result = -2;
	}
	
    return result;
}

int Camera::SetGain(uint32_t gain)
{
	int result = -1;
	v4l2_control fmt;
	
	CLEAR(fmt);
    fmt.id = V4L2_CID_GAIN;
	fmt.value = gain;

	if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_S_CTRL, &fmt))
	{                
		Logger::LogEx("Camera::SetGain VIDIOC_S_CTRL V4L2_CID_GAIN to %d OK", gain);
        emit OnCameraMessage_Signal(QString("SetGain VIDIOC_S_CTRL: V4L2_CID_GAIN to %1 OK.").arg(gain));

		result = 0;
	}
	else
	{
		Logger::LogEx("Camera::SetGain VIDIOC_S_CTRL V4L2_CID_GAIN failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
        emit OnCameraError_Signal(QString("SetGain VIDIOC_S_CTRL: V4L2_CID_GAIN failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
	}
	
    return result;
}

int Camera::ReadAutoGain(bool &autogain)
{
    int result = -1;
	v4l2_queryctrl ctrl;
	
	CLEAR(ctrl);
	ctrl.id = V4L2_CID_AUTOGAIN;
	
	if (V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_QUERYCTRL, &ctrl) >= 0)
	{
		v4l2_control fmt;

		Logger::LogEx("Camera::ReadAutoGain VIDIOC_QUERYCTRL V4L2_CID_AUTOGAIN OK");
		emit OnCameraMessage_Signal(QString("ReadAutoGain VIDIOC_QUERYCTRL: V4L2_CID_AUTOGAIN OK."));
		
		CLEAR(fmt);
		fmt.id = V4L2_CID_AUTOGAIN;

		if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_G_CTRL, &fmt))
		{       
			Logger::LogEx("Camera::ReadAutoGain VIDIOC_G_CTRL V4L2_CID_AUTOGAIN OK =%d", fmt.value);
			emit OnCameraMessage_Signal(QString("ReadAutoGain VIDIOC_G_CTRL: V4L2_CID_AUTOGAIN OK =%1.").arg(fmt.value));
         
			autogain = fmt.value;
			
			result = 0;
		}
		else
		{
			Logger::LogEx("Camera::ReadAutoGain VIDIOC_G_CTRL V4L2_CID_AUTOGAIN failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
			emit OnCameraError_Signal(QString("ReadAutoGain VIDIOC_G_CTRL: V4L2_CID_AUTOGAIN failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
		}
	}
	else
	{
		Logger::LogEx("Camera::ReadAutoGain VIDIOC_QUERYCTRL V4L2_CID_AUTOGAIN failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
		emit OnCameraMessage_Signal(QString("ReadAutoGain VIDIOC_QUERYCTRL: V4L2_CID_AUTOGAIN failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
		
		result = -2;
	}
	
    return result;
}

int Camera::SetAutoGain(bool autogain)
{
    int result = -1;
	v4l2_control fmt;
	
	CLEAR(fmt);
    fmt.id = V4L2_CID_AUTOGAIN;
	fmt.value = autogain;

	if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_S_CTRL, &fmt))
	{          
		Logger::LogEx("Camera::SetAutoGain VIDIOC_S_CTRL V4L2_CID_AUTOGAIN to %s OK", ((autogain)?"true":"false"));
        emit OnCameraMessage_Signal(QString("SetAutoGain VIDIOC_S_CTRL: V4L2_CID_AUTOGAIN to %1 OK.").arg((autogain)?"true":"false"));
		
		result = 0;
	}
	else
	{
		Logger::LogEx("Camera::SetAutoGain VIDIOC_S_CTRL V4L2_CID_AUTOGAIN failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
        emit OnCameraError_Signal(QString("SetAutoGain VIDIOC_S_CTRL: V4L2_CID_AUTOGAIN failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
	}
	
    return result;
}

int Camera::ReadExposure(uint32_t &exposure)
{
    int result = -1;
	v4l2_queryctrl ctrl;
	
	CLEAR(ctrl);
	ctrl.id = V4L2_CID_EXPOSURE;
	
	if (V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_QUERYCTRL, &ctrl) >= 0)
	{
		v4l2_control fmt;

		Logger::LogEx("Camera::ReadExposure VIDIOC_QUERYCTRL Enum V4L2_CID_EXPOSURE OK");
		emit OnCameraMessage_Signal(QString("ReadExposure VIDIOC_QUERYCTRL Enum V4L2_CID_EXPOSURE: OK."));
		
		CLEAR(fmt);
		fmt.id = V4L2_CID_EXPOSURE;

		if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_G_CTRL, &fmt))
		{                
			Logger::LogEx("Camera::ReadExposure VIDIOC_G_CTRL V4L2_CID_EXPOSURE OK =%d", fmt.value);
			emit OnCameraMessage_Signal(QString("ReadExposure VIDIOC_G_CTRL: V4L2_CID_EXPOSURE OK =%1.").arg(fmt.value));

			exposure = fmt.value;
			
			result = 0;
		}
		else
		{
			Logger::LogEx("Camera::ReadExposure VIDIOC_G_CTRL V4L2_CID_EXPOSURE failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
			emit OnCameraError_Signal(QString("ReadExposure VIDIOC_G_CTRL: V4L2_CID_EXPOSURE failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
		}
	}
	else
	{
		Logger::LogEx("Camera::ReadExposure VIDIOC_QUERYCTRL Enum V4L2_CID_EXPOSURE failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
		emit OnCameraMessage_Signal(QString("ReadExposure VIDIOC_QUERYCTRL Enum V4L2_CID_EXPOSURE: failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
		
		result = -2;
	}
	
    return result;
}

int Camera::SetExposure(uint32_t exposure)
{
    int result = -1;
	v4l2_control fmt;
	
	CLEAR(fmt);
    fmt.id = V4L2_CID_EXPOSURE;
	fmt.value = exposure;

	if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_S_CTRL, &fmt))
	{                
		Logger::LogEx("Camera::SetExposure VIDIOC_S_CTRL V4L2_CID_EXPOSURE to %d OK", exposure);
        emit OnCameraMessage_Signal(QString("SetExposure VIDIOC_S_CTRL: V4L2_CID_EXPOSURE to %1 OK.").arg(exposure));
		result = 0;
	}
	else
	{
		Logger::LogEx("Camera::SetExposure VIDIOC_S_CTRL V4L2_CID_EXPOSURE failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
        emit OnCameraError_Signal(QString("SetExposure VIDIOC_S_CTRL: V4L2_CID_EXPOSURE failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
	}
	
    return result;
}

int Camera::ReadAutoExposure(bool &autoexposure)
{
    int result = -1;
	v4l2_queryctrl ctrl;
	
	CLEAR(ctrl);
	ctrl.id = V4L2_CID_EXPOSURE_AUTO;
	
	if (V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_QUERYCTRL, &ctrl) >= 0)
	{
		v4l2_control fmt;
		
		Logger::LogEx("Camera::ReadAutoExposure VIDIOC_QUERYCTRL V4L2_CID_EXPOSURE_AUTO OK");
		emit OnCameraMessage_Signal(QString("ReadAutoExposure VIDIOC_QUERYCTRL: V4L2_CID_EXPOSURE_AUTO OK."));

		CLEAR(fmt);
		fmt.id = V4L2_CID_EXPOSURE_AUTO;

		if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_G_CTRL, &fmt))
		{                
			Logger::LogEx("Camera::ReadAutoExposure VIDIOC_G_CTRL V4L2_CID_EXPOSURE_AUTO OK =%d", fmt.value);
			emit OnCameraMessage_Signal(QString("ReadAutoExposure VIDIOC_G_CTRL: V4L2_CID_EXPOSURE_AUTO OK =%1.").arg(fmt.value));

			autoexposure = fmt.value;
			
			result = 0;
		}
		else
		{
			Logger::LogEx("Camera::ReadAutoExposure VIDIOC_G_CTRL V4L2_CID_EXPOSURE_AUTO failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
			emit OnCameraError_Signal(QString("ReadAutoExposure VIDIOC_G_CTRL: V4L2_CID_EXPOSURE_AUTO failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
		}
	}
	else
	{
		Logger::LogEx("Camera::ReadAutoExposure VIDIOC_QUERYCTRL Enum V4L2_CID_EXPOSURE_AUTO failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
		emit OnCameraMessage_Signal(QString("ReadAutoExposure VIDIOC_QUERYCTRL Enum V4L2_CID_EXPOSURE_AUTO: failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
		
		result = -2;
	}
	
    return result;
}

int Camera::SetAutoExposure(bool autoexposure)
{
    int result = -1;
	v4l2_control fmt;
	
	CLEAR(fmt);
    fmt.id = V4L2_CID_EXPOSURE_AUTO;
	fmt.value = autoexposure;

	if (-1 != V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_S_CTRL, &fmt))
	{                
		Logger::LogEx("Camera::SetAutoExposure VIDIOC_S_CTRL V4L2_CID_EXPOSURE_AUTO to %s OK", ((autoexposure)?"true":"false"));
        emit OnCameraMessage_Signal(QString("SetAutoExposure VIDIOC_S_CTRL: V4L2_CID_EXPOSURE_AUTO to %1 OK.").arg(((autoexposure)?"true":"false")));
		
		result = 0;
	}
	else
	{
		Logger::LogEx("Camera::SetAutoExposure VIDIOC_S_CTRL V4L2_CID_EXPOSURE_AUTO failed errno=%d=%s", errno, V4l2Helper::ConvertErrno2String(errno).c_str());
        emit OnCameraError_Signal(QString("SetAutoExposure VIDIOC_S_CTRL: V4L2_CID_EXPOSURE_AUTO failed errno=%1=%2.").arg(errno).arg(V4l2Helper::ConvertErrno2String(errno).c_str()));
	}
	
    return result;
}

/*********************************************************************************************************/
// Frame buffer handling
/*********************************************************************************************************/

int Camera::CreateUserBuffer(uint32_t bufferCount, uint32_t bufferSize)
{
    int result = -1;

    if (bufferCount <= MAX_VIEWER_USER_BUFFER_COUNT)
    {
        v4l2_requestbuffers req;

		// creates user defined buffer
		CLEAR(req);

        req.count  = bufferCount;
        req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_USERPTR;

		// requests 4 video capture buffer. Driver is going to configure all parameter and doesn't allocate them.
		if (-1 == V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_REQBUFS, &req)) 
		{
			if (EINVAL == errno) 
			{
				Logger::LogEx("Camera::CreateUserBuffer VIDIOC_REQBUFS does not support user pointer i/o");
				emit OnCameraError_Signal("Camera::CreateUserBuffer: VIDIOC_REQBUFS does not support user pointer i/o.");
			} else {
				Logger::LogEx("Camera::CreateUserBuffer VIDIOC_REQBUFS error");
				emit OnCameraError_Signal("Camera::CreateUserBuffer: VIDIOC_REQBUFS error.");
			}
		}
		else 
		{
			Logger::LogEx("Camera::CreateUserBuffer VIDIOC_REQBUFS OK");
			emit OnCameraError_Signal("Camera::CreateUserBuffer: VIDIOC_REQBUFS OK.");
		}

        // create local buffer container
        m_UserBufferContainerList.resize(bufferCount);
        
        if (m_UserBufferContainerList.size() != bufferCount) 
		{
			Logger::LogEx("Camera::CreateUserBuffer buffer container error");
			emit OnCameraError_Signal("Camera::CreateUserBuffer: buffer container error.");
			return -1;
        }

        // get the length and start address of each of the 4 buffer structs and assign the user buffer addresses
        for (int x = 0; x < bufferCount; ++x) 
		{
			m_UserBufferContainerList[x] = new USER_BUFFER;
            m_UserBufferContainerList[x]->nBufferlength = bufferSize;
            m_UserBufferContainerList[x]->pBuffer = new uint8_t[bufferSize];

            if (!m_UserBufferContainerList[x]->pBuffer) 
			{
				Logger::LogEx("Camera::CreateUserBuffer buffer creation error");
				emit OnCameraError_Signal("Camera::CreateUserBuffer: buffer creation error.");
				return -1;
            }
        }

        m_UsedBufferCount = bufferCount;
        result = 0;
    }

    return result;
}

int Camera::QueueAllUserBuffer()
{
	int result = -1;
	
    // queue the buffer
    for (uint32_t i=0; i<m_UsedBufferCount; i++)
    {
		v4l2_buffer buf;

		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_USERPTR;
		buf.index = i;
		buf.m.userptr = (unsigned long)m_UserBufferContainerList[i]->pBuffer;
		buf.length = m_UserBufferContainerList[i]->nBufferlength;

		if (-1 == V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_QBUF, &buf))
		{
			Logger::LogEx("Camera::QueueUserBuffer VIDIOC_QBUF queue #%d buffer=%p failed", i, m_UserBufferContainerList[i]->pBuffer);
			return result;
		}
		else
		{
            Logger::LogEx("Camera::QueueUserBuffer VIDIOC_QBUF queue #%d buffer=%p OK", i, m_UserBufferContainerList[i]->pBuffer);
			result = 0;
		}
    }
    
    return result;
}

int Camera::QueueSingleUserBuffer(const int index)
{
	int result = 0;
	v4l2_buffer buf;

	CLEAR(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_USERPTR;
	buf.index = index;
	buf.m.userptr = (unsigned long)m_UserBufferContainerList[index]->pBuffer;
	buf.length = m_UserBufferContainerList[index]->nBufferlength;

	if (-1 == V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_QBUF, &buf))
	{
		Logger::LogEx("Camera::QueueSingleUserBuffer VIDIOC_QBUF queue #%d buffer=%p failed", index, m_UserBufferContainerList[index]->pBuffer);
	}
				
	return result;
}

int Camera::DeleteUserBuffer()
{
    int result = 0;

    // delete all user buffer
	for (int x = 0; x < m_UsedBufferCount; x++)
	{
		delete [] m_UserBufferContainerList[x]->pBuffer;
		
		delete m_UserBufferContainerList[x];
	}
	
	m_UserBufferContainerList.resize(0);
	
    return result;
}

/*********************************************************************************************************/
// Info
/*********************************************************************************************************/

int Camera::GetCameraDriverName(uint32_t index, std::string &strText)
{
    int result = 0;
	
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;

	// queuery device capabilities
	if (-1 == V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_QUERYCAP, &cap)) 
	{
        Logger::LogEx("Camera::GetCameraDriverName VIDIOC_QUERYCAP %s is no V4L2 device\n", m_DeviceName.c_str());
		emit OnCameraError_Signal("Camera::GetCameraDriverName VIDIOC_QUERYCAP " + QString(m_DeviceName.c_str()) + " is no V4L2 device.");
        return -1;
    } 
	else
	{
		Logger::LogEx("Camera::GetCameraDriverName VIDIOC_QUERYCAP %s OK\n", m_DeviceName.c_str());
	}

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) 
	{
        Logger::LogEx("Camera::GetCameraDriverName %s is no video capture device\n", m_DeviceName.c_str());
		emit OnCameraError_Signal("Camera::GetCameraDriverName " + QString(m_DeviceName.c_str()) + " is no video capture device.");
        return -1;
    }
	else
	{
		Logger::LogEx("Camera::GetCameraDriverName VIDIOC_QUERYCAP %s driver name=%s\n", m_DeviceName.c_str(), (char*)cap.driver);
	}
	
    strText = (char*)cap.driver;
    
    return result;
}

int Camera::GetCameraDeviceName(uint32_t index, std::string &strText)
{
    int result = 0;
	
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;

	// queuery device capabilities
	if (-1 == V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_QUERYCAP, &cap)) 
	{
        Logger::LogEx("Camera::GetCameraDeviceName %s is no V4L2 device\n", m_DeviceName.c_str());
		emit OnCameraError_Signal("Camera::GetCameraDeviceName " + QString(m_DeviceName.c_str()) + " is no V4L2 device.");
        return -1;
    }
	else
	{
		Logger::LogEx("Camera::GetCameraDeviceName VIDIOC_QUERYCAP %s OK\n", m_DeviceName.c_str());
	}

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) 
	{
        Logger::LogEx("Camera::GetCameraDeviceName %s is no video capture device\n", m_DeviceName.c_str());
		emit OnCameraError_Signal("Camera::GetCameraDeviceName " + QString(m_DeviceName.c_str()) + " is no video capture device.");
        return -1;
    }
	else
	{
		Logger::LogEx("Camera::GetCameraDeviceName VIDIOC_QUERYCAP %s device name=%s\n", m_DeviceName.c_str(), (char*)cap.card);
	}

    strText = (char*)cap.card;

    return result;
}

int Camera::GetCameraBusInfo(uint32_t index, std::string &strText)
{
    int result = 0;
	
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;

	// queuery device capabilities
	if (-1 == V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_QUERYCAP, &cap)) 
	{
        Logger::LogEx("Camera::GetCameraBusInfo %s is no V4L2 device\n", m_DeviceName.c_str());
		emit OnCameraError_Signal("Camera::GetCameraBusInfo " + QString(m_DeviceName.c_str()) + " is no V4L2 device.");
        return -1;
    }
	else
	{
		Logger::LogEx("Camera::GetCameraBusInfo VIDIOC_QUERYCAP %s OK\n", m_DeviceName.c_str());
	}

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) 
	{
        Logger::LogEx("Camera::GetCameraBusInfo %s is no video capture device\n", m_DeviceName.c_str());
		emit OnCameraError_Signal("Camera::GetCameraBusInfo " + QString(m_DeviceName.c_str()) + " is no video capture device.");
        return -1;
    }
	else
	{
		Logger::LogEx("Camera::GetCameraBusInfo VIDIOC_QUERYCAP %s bus info=%s\n", m_DeviceName.c_str(), (char*)cap.bus_info);
	}

    strText = (char*)cap.bus_info;

    return result;
}

int Camera::GetCameraDriverVersion(uint32_t index, std::string &strText)
{
    int result = 0;
	std::stringstream tmp;
	
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;

	// queuery device capabilities
	if (-1 == V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_QUERYCAP, &cap)) 
	{
        Logger::LogEx("Camera::GetCameraDriverVersion %s is no V4L2 device\n", m_DeviceName.c_str());
		emit OnCameraError_Signal("Camera::GetCameraDriverVersion " + QString(m_DeviceName.c_str()) + " is no V4L2 device.");
        return -1;
    }
	else
	{
		Logger::LogEx("Camera::GetCameraDriverVersion VIDIOC_QUERYCAP %s OK\n", m_DeviceName.c_str());
	}

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) 
	{
        Logger::LogEx("Camera::GetCameraDriverVersion %s is no video capture device\n", m_DeviceName.c_str());
		emit OnCameraError_Signal("Camera::GetCameraDriverVersion " + QString(m_DeviceName.c_str()) + " is no video capture device.");
        return -1;
    }
	else
	{
		tmp << ((cap.version/0x10000)&0xFF) << "." << ((cap.version/0x100)&0xFF) << "." << (cap.version&0xFF);
		Logger::LogEx("Camera::GetCameraDriverVersion VIDIOC_QUERYCAP %s driver version=%s\n", m_DeviceName.c_str(), tmp.str().c_str());
	}

    strText = tmp.str();
    
    return result;
}

int Camera::GetCameraCapabilities(uint32_t index, std::string &strText)
{
	int result = 0;
	std::stringstream tmp;
	
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;

	// queuery device capabilities
	if (-1 == V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_QUERYCAP, &cap)) 
	{
        Logger::LogEx("Camera::GetCameraCapabilities %s is no V4L2 device\n", m_DeviceName.c_str());
		emit OnCameraError_Signal("Camera::GetCameraCapabilities " + QString(m_DeviceName.c_str()) + " is no V4L2 device.");
        return -1;
    }
	else
	{
		Logger::LogEx("Camera::GetCameraCapabilities VIDIOC_QUERYCAP %s OK\n", m_DeviceName.c_str());
	}

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) 
	{
        Logger::LogEx("Camera::GetCameraCapabilities %s is no video capture device\n", m_DeviceName.c_str());
		emit OnCameraError_Signal("Camera::GetCameraCapabilities " + QString(m_DeviceName.c_str()) + " is no video capture device.");
        return -1;
    }
	else
	{
		tmp << "0x" << std::hex << cap.capabilities << std::endl
			<< "    Read/Write = " << ((cap.capabilities & V4L2_CAP_READWRITE)?"Yes":"No") << std::endl
			<< "    Streaming = " << ((cap.capabilities & V4L2_CAP_STREAMING)?"Yes":"No");
		Logger::LogEx("Camera::GetCameraCapabilities VIDIOC_QUERYCAP %s driver version=%s\n", m_DeviceName.c_str(), tmp.str().c_str());
	}

	
    strText = tmp.str();
    
    return result;
}

    
// Recording
void Camera::SetRecording(bool start)
{
    m_DmaSICallbacks.SetRecording(start);
}

void Camera::DisplayStepBack()
{
    m_DmaSICallbacks.DisplayStepBack();
}

void Camera::DisplayStepForw()
{
    m_DmaSICallbacks.DisplayStepForw();
}

void Camera::DeleteRecording()
{
    m_DmaSICallbacks.DeleteRecording();
}

/*********************************************************************************************************/
// Commands
/*********************************************************************************************************/

/*********************************************************************************************************/
// Tools
/*********************************************************************************************************/



}}} // namespace AVT::Tools::Examples
