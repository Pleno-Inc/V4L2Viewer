/*=============================================================================
  Copyright (C) 2012 Allied Vision Technologies.  All Rights Reserved.

  Redistribution of this file, in original or modified form, without
  prior written consent of Allied Vision Technologies is prohibited.

-------------------------------------------------------------------------------

  File:        FrameObserver.h

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

#ifndef FRAMEOBSERVER_INCLUDE
#define FRAMEOBSERVER_INCLUDE

#include <vector>
#include <queue>

#include <QObject>
#include <QThread>
#include <QImage>
#include <QSharedPointer>

#include "MyFrameQueue.h"
#include "ImageProcessingThread.h"
#include "Helper.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "videodev2_av.h"

#include "V4l2Helper.h"
#include "LoggerMutex.h"

////////////////////////////////////////////////////////////////////////////////
// TYPEDEFS
////////////////////////////////////////////////////////////////////////////////

#define MAX_VIEWER_USER_BUFFER_COUNT	50

////////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////////


namespace AVT {
namespace Tools {
namespace Examples {

typedef struct _USER_BUFFER
{
    uint8_t         *pBuffer;
    size_t  		nBufferlength;
} USER_BUFFER, *PUSER_BUFFER, **PPUSER_BUFFER;


class FrameObserver : public QThread
{
    Q_OBJECT

  public:
    // We pass the camera that will deliver the frames to the constructor
	FrameObserver(bool showFrames);
    //
	virtual ~FrameObserver();

	int StartStream(bool blockingMode, int fileDescriptor, uint32_t pixelformat, 
			uint32_t payloadsize, uint32_t width, uint32_t height, uint32_t bytesPerLine,
			uint32_t enableLogging, int32_t logFrameStart, int32_t logFrameEnd,
			int32_t dumpFrameStart, int32_t dumpFrameEnd, uint32_t enableRAW10Correction,
			std::vector<uint8_t> &rData);
	int StopStream();
	
    // Get the number of frames
	// This function will clear the counter of received frames
	unsigned int GetReceivedFramesCount();

	// Get the number of uncompleted frames
	unsigned int GetDroppedFramesCount();

	// Set the number of uncompleted frames
	void ResetDroppedFramesCount();

    // Recording
    void SetRecording(bool start);
    void DisplayStepBack();
    void DisplayStepForw();
    void DeleteRecording();
//	MyFrameQueue& GetRecordQueue();
    
    virtual int CreateAllUserBuffer(uint32_t bufferCount, uint32_t bufferSize);
    virtual int QueueAllUserBuffer();
    virtual int QueueSingleUserBuffer(const int index);
    virtual int DeleteAllUserBuffer();
    
    //void FrameDone(const unsigned long long frameHandle);

    void SwitchFrameTransfer2GUI(bool showFrames);

protected:
	// v4l2
    virtual int ReadFrame(v4l2_buffer &buf);
	virtual int GetFrameData(v4l2_buffer &buf, uint8_t *&buffer, uint32_t &length);
	int ProcessFrame(v4l2_buffer &buf);
	void DequeueAndProcessFrame();
		
	// Do the work within this thread
    virtual void run();
	
protected:
    const static int MAX_RECORD_FRAME_QUEUE_SIZE = 100;
    bool m_bRecording;
    MyFrameQueue m_FrameRecordQueue;

	// Counter to count the received images
	uint32_t m_nReceivedFramesCounter;

	// Counter to count the received uncompleted images
	unsigned int m_nDroppedFramesCounter;

    // Variable to abort the running thread
	int m_nFileDescriptor;
	uint32_t m_Pixelformat;
	uint32_t m_nWidth;
	uint32_t m_nHeight;
	uint32_t m_PayloadSize;
	uint32_t m_RealPayloadsize;
	uint32_t m_BytesPerLine;
	uint64_t m_FrameId;
	uint32_t m_DQBUF_last_errno;
	
	bool m_MessageSendFlag;
    bool m_BlockingMode;
	bool m_bStreamRunning;
	bool m_bStreamStopped;
	
	uint32_t m_EnableRAW10Correction;
	uint32_t m_EnableLogging;
	int32_t m_FrameCount;
	int32_t m_LogFrameStart;
	int32_t m_LogFrameEnd;
	int32_t m_DumpFrameStart;
	int32_t m_DumpFrameEnd;
	
	bool m_ShowFrames;
	
	std::vector<PUSER_BUFFER>		    m_UserBufferContainerList;
	AVT::BaseTools::LocalMutex                  m_UsedBufferMutex;
	
	// Shared pointer to a worker thread for the image processing
	QSharedPointer<ImageProcessingThread> m_pImageProcessingThread;
	
	std::vector<uint8_t> 			    m_rCSVData;

private slots:
	//Event handler for getting the processed frame to an image
	void OnFrameReadyFromThread(const QImage &image, const unsigned long long &frameId, const int &bufIndex);
	//Event handler for sending messages to GUI
	void OnMessageFromThread(const QString &msg);
	
signals:
	// Event will be called when a frame is processed by the internal thread and ready to show
	void OnFrameReady_Signal(const QImage &image, const unsigned long long &frameId);
	// Event will be called when a frame is processed by the internal thread and ready to show
	void OnFrameID_Signal(const unsigned long long &frameId);
	// Event will be called when the frame processing is done and the frame can be returned to streaming engine
	//void OnFrameDone_Signal(const unsigned long long frameHandle);
    // Event will be called when the a frame is recorded
	void OnRecordFrame_Signal(const unsigned long long &, const unsigned long long &);
    // Event will be called when the a frame is displayed
	void OnDisplayFrame_Signal(const unsigned long long &);
    // Event will be called when for text notification
    void OnMessage_Signal(const QString &msg);
    // Event will be called on error
    void OnError_Signal(const QString &text);
};

}}} // namespace AVT::Tools::Examples

#endif /* FRAMEOBSERVER_INCLUDE */

