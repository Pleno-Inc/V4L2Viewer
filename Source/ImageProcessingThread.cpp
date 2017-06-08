/*=============================================================================
  Copyright (C) 2015 Allied Vision Technologies.  All Rights Reserved.

  Redistribution of this file, in original or modified form, without
  prior written consent of Allied Vision Technologies is prohibited.

-------------------------------------------------------------------------------

  File:        ImageProcessingThread.cpp

  Description:

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

#include "ImageProcessingThread.h"
#include "ImageTransf.h"


ImageProcessingThread::ImageProcessingThread()
	: m_bAbort(false)
{
}

ImageProcessingThread::~ImageProcessingThread(void)
{
}

int ImageProcessingThread::QueueFrame(v4l2_buffer &buf, uint8_t *&buffer, uint32_t &length, 
									  uint32_t &width, uint32_t &height, uint32_t &pixelformat,
									  uint32_t &payloadSize, uint32_t &bytesPerLine, uint64_t &frameID)
{
	int result = -1;
    
    // make sure Viewer is never working in the past
    // if viewer is too slow we drop the frames.
    if (m_FrameQueue.GetSize() < MAX_QUEUE_SIZE)
    {
		m_FrameQueue.Enqueue(buf, buffer, length, width, height, pixelformat, payloadSize, bytesPerLine, frameID);
		result = 0;
    }
    
    return result;
}

// Set the data for the thread to work with
int ImageProcessingThread::QueueFrame(QImage &image, uint64_t &frameID)
{
    int result = -1;
    
    // make sure Viewer is never working in the past
    // if viewer is too slow we drop the frames.
    if (m_FrameQueue.GetSize() < MAX_QUEUE_SIZE)
    {
		m_FrameQueue.Enqueue(image, frameID);
		result = 0;
    }
    
    return result;
}

// Set the data for the thread to work with
int ImageProcessingThread::QueueFrame(QSharedPointer<MyFrame> pFrame)
{
    int result = -1;
    
    // make sure Viewer is never working in the past
    // if viewer is too slow we drop the frames.
    if (m_FrameQueue.GetSize() < MAX_QUEUE_SIZE)
    {
		m_FrameQueue.Enqueue(pFrame);
		result = 0;
    }
    
    return result;
}

// stop the internal processing thread and wait until the thread is really stopped
void ImageProcessingThread::StartThread()
{
	m_bAbort = false;
  
	start();
	
}

// stop the internal processing thread and wait until the thread is really stopped
void ImageProcessingThread::StopThread()
{
	m_bAbort = true;

	// wait until the thread is stopped
	while (isRunning())
		QThread::msleep(10);
	
	m_FrameQueue.Clear();
}

// Do the work within this thread
void ImageProcessingThread::run()
{
	int result = 0;
	
	while (!m_bAbort)
	{
		if(0 < m_FrameQueue.GetSize())
		{
			QSharedPointer<MyFrame> pFrame = m_FrameQueue.Dequeue();
			uint64_t frameID = pFrame->GetFrameId();
			v4l2_buffer buf = pFrame->GetV4l2Image();
			const uint8_t* pBuffer = pFrame->GetBuffer();
			uint32_t length = pFrame->GetBufferlength();
			uint32_t width = pFrame->GetWidth();
			uint32_t height = pFrame->GetHeight();
			uint32_t pixelformat = pFrame->GetPixelformat();
			uint32_t payloadSize = pFrame->GetPayloadSize();
			uint32_t bytesPerLine = pFrame->GetBytesPerLine();
			QImage convertedImage;
			
			result = AVT::Tools::ImageTransf::ConvertFrame(pBuffer, length, 
												  width, height, pixelformat, 
												  payloadSize, bytesPerLine, convertedImage);
			
			if (result == 0)
				emit OnFrameReady_Signal(convertedImage, frameID, buf.index);
		}

		QThread::msleep(1);
	}
}

