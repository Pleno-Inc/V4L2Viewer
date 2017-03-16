/*=============================================================================
  Copyright (C) 2012 Allied Vision Technologies.  All Rights Reserved.

  Redistribution of this file, in original or modified form, without
  prior written consent of Allied Vision Technologies is prohibited.

-------------------------------------------------------------------------------

  File:        FrameObserver.cpp

  Description: The frame observer that is used for notifications
               regarding the arrival of a newly acquired frame.

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

#include <sstream>
#include <QPixmap>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#include <FrameObserverUSER.h>
#include <Logger.h>

namespace AVT {
namespace Tools {
namespace Examples {


////////////////////////////////////////////////////////////////////////////
// Implementation
////////////////////////////////////////////////////////////////////////////    

FrameObserverUSER::FrameObserverUSER() 
{
}

FrameObserverUSER::~FrameObserverUSER()
{
}

int FrameObserverUSER::ReadFrame()
{
    v4l2_buffer buf;
    int result = -1;

	memset(&buf, 0, sizeof(buf));

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_USERPTR;
	
	result = V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_DQBUF, &buf);

	if (result >= 0  && m_bStreamRunning) 
	{
		uint32_t length = buf.length;
		uint8_t* buffer = (uint8_t*)buf.m.userptr;
		
		if (0 != buffer && 0 != length)
		{  
		    QImage convertedImage;

		    m_FrameId++;
		    m_nReceivedFramesCounter++;
		    
		    result = DisplayFrame(buffer, length, convertedImage);

		    m_pImageProcessingThread->QueueFrame(convertedImage, m_FrameId);
		
		    if (m_bRecording && -1 != result)
		    {
			if (m_FrameRecordQueue.GetSize() < MAX_RECORD_FRAME_QUEUE_SIZE)
			{
				m_FrameRecordQueue.Enqueue(convertedImage, m_FrameId);
				OnRecordFrame_Signal(m_FrameId, m_FrameRecordQueue.GetSize());
			}
			else
			{
				if (m_FrameRecordQueue.GetSize() == MAX_RECORD_FRAME_QUEUE_SIZE)
					OnMessage_Signal(QString("Following frames are not saved, more than %1 would freeze the system.").arg(MAX_RECORD_FRAME_QUEUE_SIZE));
			}
		    }
		
		    
		
		    QueueSingleUserBuffer(buf.index);
		}
		else
		{
		    if (!m_MessageSendFlag)
		    {
		        m_MessageSendFlag = true;
		        OnMessage_Signal(QString("Frame buffer empty !"));
		    }
		    
		    if (m_bStreamRunning)
			V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_QBUF, &buf);
		}
	}
	if (!m_bStreamRunning)
	    m_bStreamStopped = true;

	return result;
}


/*********************************************************************************************************/
// Frame buffer handling
/*********************************************************************************************************/

int FrameObserverUSER::CreateUserBuffer(uint32_t bufferCount, uint32_t bufferSize)
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
				emit OnError_Signal("Camera::CreateUserBuffer: VIDIOC_REQBUFS does not support user pointer i/o.");
			} else {
				Logger::LogEx("Camera::CreateUserBuffer VIDIOC_REQBUFS error");
				emit OnError_Signal("Camera::CreateUserBuffer: VIDIOC_REQBUFS error.");
			}
		}
		else 
		{
			Logger::LogEx("Camera::CreateUserBuffer VIDIOC_REQBUFS OK");
			emit OnMessage_Signal("Camera::CreateUserBuffer: VIDIOC_REQBUFS OK.");
		
			// create local buffer container
			m_UserBufferContainerList.resize(bufferCount);
        
			if (m_UserBufferContainerList.size() != bufferCount) 
		        {
			    Logger::LogEx("Camera::CreateUserBuffer buffer container error");
			    emit OnError_Signal("Camera::CreateUserBuffer: buffer container error.");
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
			        emit OnError_Signal("Camera::CreateUserBuffer: buffer creation error.");
			        return -1;
			    }
		        }
			
			m_UsedBufferCount = bufferCount;
			result = 0;
		}
    }

    return result;
}

int FrameObserverUSER::QueueAllUserBuffer()
{
    int result = -1;
    
    // queue the buffer
    for (uint32_t i=0; i<m_UsedBufferCount; i++)
    {
		v4l2_buffer buf;

		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.index = i;
		buf.memory = V4L2_MEMORY_USERPTR;
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

int FrameObserverUSER::QueueSingleUserBuffer(const int index)
{
	int result = 0;
	v4l2_buffer buf;
	
	CLEAR(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.index = index;
	buf.memory = V4L2_MEMORY_USERPTR;
        buf.m.userptr = (unsigned long)m_UserBufferContainerList[index]->pBuffer;
        buf.length = m_UserBufferContainerList[index]->nBufferlength;
	
	if (m_bStreamRunning)
	{  
	    if (-1 == V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_QBUF, &buf))
	    {
		Logger::LogEx("Camera::QueueSingleUserBuffer VIDIOC_QBUF queue #%d buffer=%p failed", index, m_UserBufferContainerList[index]->pBuffer);
	    }
	}
				    
	return result;
}

int FrameObserverUSER::DeleteUserBuffer()
{
    int result = 0;

    // delete all user buffer
	for (int x = 0; x < m_UsedBufferCount; x++)
	{
    	    if (0 != m_UserBufferContainerList[x]->pBuffer)
	        delete [] m_UserBufferContainerList[x]->pBuffer;
	    if (0 != m_UserBufferContainerList[x])
	        delete m_UserBufferContainerList[x];
	}
	
	m_UserBufferContainerList.resize(0);
	
	
	// free all internal buffers
	v4l2_requestbuffers req;
	// creates user defined buffer
	CLEAR(req);
        req.count  = 0;
        req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;
	
	// requests 4 video capture buffer. Driver is going to configure all parameter and doesn't allocate them.
	V4l2Helper::xioctl(m_nFileDescriptor, VIDIOC_REQBUFS, &req);
	
	
    return result;
}

}}} // namespace AVT::Tools::Examples
