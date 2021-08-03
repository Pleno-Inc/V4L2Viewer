/*=============================================================================
Copyright (C) 2021 Allied Vision Technologies.  All Rights Reserved.

Redistribution of this file, in original or modified form, without
prior written consent of Allied Vision Technologies is prohibited.

-------------------------------------------------------------------------------

File:        V4L2Viewer.h

Description: This class is a main class of the application. It describes how
             the main window looks like and also performs all actions
             which takes place in the GUI.

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

#ifndef V4L2VIEWER_H
#define V4L2VIEWER_H

#include "Camera.h"
#include "DeviationCalculator.h"
#include "FrameObserver.h"
#include "MyFrame.h"
#include "AboutWidget.h"
#include "ui_V4L2Viewer.h"
#include "ControlsHolderWidget.h"
#include "ActiveExposureWidget.h"
#include "CustomGraphicsView.h"

#include <list>

#include <QMainWindow>
#include <QtWidgets>
#include <QTimer>
#include <QTranslator>
#include <QDockWidget>


class V4L2Viewer : public QMainWindow
{
    Q_OBJECT

public:
    V4L2Viewer( QWidget *parent = 0, Qt::WindowFlags flags = 0 );
    ~V4L2Viewer();

private:

    std::list<QSharedPointer<V4L2Viewer> > m_pViewer;

    bool m_BLOCKING_MODE;
    IO_METHOD_TYPE m_MMAP_BUFFER;
    bool m_VIDIOC_TRY_FMT;
    bool m_ShowFrames;

    // The currently streaming camera
    Camera m_Camera;

    uint32_t m_nStreamNumber;

    // The Qt GUI
    Ui::V4L2ViewerClass ui;
    // The menu widget to setup the number of used frames
    QWidgetAction *m_NumberOfUsedFramesWidgetAction;
    // The line which holds the number of used frames
    QLineEdit *m_NumberOfUsedFramesLineEdit;
    QWidgetAction *m_NumberOfFixedFrameRateWidgetAction;
    QLineEdit *m_NumberOfFixedFrameRate;
    // A list of known camera IDs
    std::vector<uint32_t> m_cameras;
    // The state of the camera (opened/closed)
    bool m_bIsOpen;
    // The current streaming state
    bool m_bIsStreaming;
    // Timer to show the frames received from the frame observer
    QTimer m_FramesReceivedTimer;
    // Graphics scene to show the image
    QSharedPointer<QGraphicsScene> m_pScene;
    // Pixel map for the graphics scene
    QGraphicsPixmapItem *m_PixmapItem;
    // store radio buttons for blocking/non-blocking mode in a group
    QButtonGroup* m_BlockingModeRadioButtonGroup;
    // This value holds translations for the internationalization
    QTranslator *m_pGermanTranslator;
    // The about widget which holds information about Qt
    AboutWidget *m_pAboutWidget;
    // The settings menu on the top bar
    QMenu *m_pSettingsMenu;
    // This variable stores minimum exposure for the logarithmic slider calculations
    int32_t m_MinimumExposure;
    // This variable stores maximum exposure for the logarithmic slider calculations
    int32_t m_MaximumExposure;

    int32_t m_MinimumExposureAbs;
    int32_t m_MaximumExposureAbs;

    int32_t m_sliderGainValue;
    int32_t m_sliderBrightnessValue;
    int32_t m_sliderGammaValue;
    int32_t m_sliderExposureValue;

    // The enumeration control widget which holds all of the enum controls gathered
    // from the Camera class object
    ControlsHolderWidget *m_pEnumerationControlWidget;
    // The active exposure widget
    QPointer<ActiveExposureWidget> m_pActiveExposureWidget;

    bool m_bIsCropAvailable;
    bool m_bIsFrameIntervalAvailable;

    // Queries and lists all known cameras
    //
    // Parameters:
    // [in] (uint32_t) cardNumber
    // [in] (uint64_t) cameraID
    // [in] (const QString &) deviceName
    // [in] (const QString &) info - information about device
    void UpdateCameraListBox(uint32_t cardNumber, uint64_t cameraID, const QString &deviceName, const QString &info);
    // Update the viewer range
    void UpdateViewerLayout();
    // Update the zoom buttons
    void UpdateZoomButtons();
    // Open/Close the camera
    //
    // Parameters:
    // [in] (const uint32_t) cardNumber
    // [in] (const QString &) deviceName
    //
    // Returns:
    // (int) result of open/close/setup camera
    int OpenAndSetupCamera(const uint32_t cardNumber, const QString &deviceName);
    // This function closes the camera
    //
    // Parameters:
    // [in] (const uint32_t) cardNumber
    //
    // Returns:
    // (int) - result of the closing operation
    int CloseCamera(const uint32_t cardNumber);
    // called by OnCameraPayloadSizeReady when payload size arrived
    //
    // Parameters:
    // [in] (uint32_t) pixelFormat
    // [in] (uint32_t) payloadSize
    // [in] (uint32_t) width
    // [in] (uint32_t) height
    // [in] (uint32_t) bytesPerLine
    void StartStreaming(uint32_t pixelFormat, uint32_t payloadSize, uint32_t width, uint32_t height, uint32_t bytesPerLine);

    // This function overrides change event, in order to
    // update language when changed
    //
    // Parameters:
    // [in] (QEvent *) event - change event
    virtual void changeEvent(QEvent *event) override;

    // This function is called by master viewer window
    void RemoteClose();
    // This function reads all data from the camera and updates
    // widgets
    void GetImageInformation();
    // Check if IO Read was checked and remove it when not capable
    void Check4IOReadAbility();
    // This function sets title text in the viewer
    void SetTitleText();
    // This function updates current camera pixel format
    void UpdateCameraFormat();
    // This function updates current item in the pixel format comboBox
    void UpdateCurrentPixelFormatOnList(QString pixelFormat);
    // The function for get device info
    QString GetDeviceInfo();
    // This function updates all of the slider passed by function parameter
    //
    // Parameters:
    // [in] (QSlider *) slider - slider object
    // [in] (int32_t) value - new value to update slider
    void UpdateSlidersPositions(QSlider *slider, int32_t value);
    // This function returns slider value based on the logarithmic value
    //
    // Parameters:
    // [in] (int32_t) value - logarithmic value
    //
    // Returns:
    // (int32_t) - position on slider
    int32_t GetSliderValueFromLog(int32_t value);

private slots:
    void OnLogToFile();
    void OnShowFrames();
    // The event handler to close the program
    void OnMenuCloseTriggered();
    // The event handler to set IO MMAP
    void OnUseMMAP();
    // The event handler to set IO USERPTR
    void OnUseUSERPTR();
    // The event handler for open / close camera
    void OnOpenCloseButtonClicked();
    // The event handler for the camera list changed event
    void OnCameraListChanged(const int &reason, unsigned int cardNumber, unsigned long long deviceID, const QString &deviceName, const QString &info);
    // The event handler for starting acquisition
    void OnStartButtonClicked();
    // The event handler for stopping acquisition
    void OnStopButtonClicked();

    // The event handler to resize the image to fit to window
    void OnZoomFitButtonClicked();
    // The slot function is called when the zoom in buton is clicked
    void OnZoomInButtonClicked();
    // The slot function is called when the zoom out buton is clicked
    void OnZoomOutButtonClicked();
    // The slot function saves image and is called when the save button is clicked
    void OnSaveImageClicked();
    // The event handler to show the frames received
    void OnUpdateFramesReceived();
    // The event handler to show the processed frame
    void OnFrameReady(const QImage &image, const unsigned long long &frameId);
    // The event handler to show the processed frame ID
    void OnFrameID(const unsigned long long &frameId);
    // The event handler to open a camera on double click event
    void OnListBoxCamerasItemDoubleClicked(QListWidgetItem * item);

    // This slot function is called when user change the save format in the
    // settings in top menu bar. It changes format to be .png
    void OnSavePNG();
    // This slot function is called when user change the save format in the
    // settings in top menu bar. It changes format to be raw
    void OnSaveRAW();
    // This slot function is called when the xOffset line edit is edited,
    // then crops the image in the X axis
    void OnCropXOffset();
    // This slot function is called when the yOffset line edit is edited,
    // then crops the image in the Y axis
    void OnCropYOffset();
    // This slot function is called when the crop width line edit is edited,
    // then crops the image within it's width
    void OnCropWidth();
    // This slot function is called when the crop height line edit is edited,
    // then crops the image within it's height
    void OnCropHeight();
    // This slot function is called when the width line edit is changed,
    // then tries to adjust frame size to the new image size
    void OnWidth();
    // This slot function is called when the height line edit is changed,
    // then tries to adjust frame size to the new image size
    void OnHeight();
    // This slot function is called on gain line edit change. It tries to
    // set new gain value to the camera
    void OnGain();
    // This slot function is called when the auto gain checkbox is clicked.
    // It changes state of the auto gain
    void OnAutoGain();
    // This slot function is called when the exposure line edit value is changed.
    // It changes exposure in the camera
    void OnExposure();
    // This slot function is called when the exposure auto checkbox is clicked.
    // It changes state of the auto exposure
    void OnAutoExposure();
    // This slot function is called when the pixel format on the comboBox widget
    // has changed, then the chosen format is set to the camera
    void OnPixelFormatChanged(const QString &item);
    // This slot function is called when the gamma line edit was changed,
    // then value is passed to camera
    void OnGamma();
    // This slot function is called when the Brightness/Blacklevel line edit
    // was changed. Value is passed to the camera
    void OnBrightness();
    // This slot function is called when the auto white balance checkbox, changed
    // it's state, then the state is passed to the camera
    void OnContinousWhiteBalance();
    // This slot function is called when the frame rate line edit is changed.
    // Value is then passed to the camera
    void OnFrameRate();
    // This slot function reads all values from the camera
    void OnReadAllValues();
    // This slot function adds new items to the pixel format combo box
    //
    // Parameters:
    // [in] (const QString &) - new pixel format which is going to be added
    void OnCameraPixelFormat(const QString &);
    // This slot function is called when the language is changed in the top menu bar
    void OnLanguageChange();

    // This slot function is called in one second interval, by the worker thread
    // which reads value when the exposure auto is turned on
    //
    // Parameters:
    // [in] (int32_t) value - value to be passed
    void OnUpdateAutoExposure(int32_t value);
    // This slot function is called in one second interval, by the worker thread
    // which reads value when the gain auto is turned on
    //
    // Parameters:
    // [in] (int32_t) value - value to be passed
    void OnUpdateAutoGain(int32_t value);
    // This slot function is called when the exposure slider was moved
    //
    // Parameters:
    // [in] (int) value - value to be passed
    void OnSliderExposureValueChange(int value);
    // This slot function is called when the gain slider was moved
    //
    // Parameters:
    // [in] (int) value - value to be passed
    void OnSliderGainValueChange(int value);
    // This slot function is called when the gamma slider was moved
    //
    // Parameters:
    // [in] (int) value - value to be passed
    void OnSliderGammaValueChange(int value);
    // This slot function is called when the blacklevel/brightness slider was moved
    //
    // Parameters:
    // [in] (int) value - value to be passed
    void OnSliderBrightnessValueChange(int value);
    // This slot function is called when the sliders are being released,
    // then it updates all of the controls
    void OnSlidersReleased();
    // This slot function is called when the camera list button is clicked,
    // it shows or hide camera list, depending on whether it's hidden or not
    void OnCameraListButtonClicked();
    // This slot function is called when the splitter between left panel,
    // and image area was moved
    //
    // Parameters:
    // [in] (int) pos - position to which the splitter was moved
    // [in] (int) index - index of the splitter's element which was moved
    void OnMenuSplitterMoved(int pos, int index);

    // This slot function shows/hides enumeration control on button click
    void ShowHideEnumerationControlWidget();

    // This slot function passes integer control's data
    //
    // Parameters:
    // [in] (int32_t) id - id of the control
    // [in] (int32_t) min - minimum value of the control
    // [in] (int32_t) max - maximum value of the control
    // [in] (int32_t) value - current value of the control
    // [in] (QString) name - name of the control
    // [in] (QString) unit - unit of the control
    // [in] (bool) bIsReadOnly - state which indicates whether control is readonly
    void PassIntDataToEnumerationWidget(int32_t id, int32_t min, int32_t max, int32_t value, QString name, QString unit, bool bIsReadOnly);
    // This slot function passes 64 integer control's data
    //
    // Parameters:
    // [in] (int64_t) id - id of the control
    // [in] (int64_t) min - minimum value of the control
    // [in] (int64_t) max - maximum value of the control
    // [in] (int64_t) value - current value of the control
    // [in] (QString) name - name of the control
    // [in] (QString) unit - unit of the control
    // [in] (bool) bIsReadOnly - state which indicates whether control is readonly
    void PassIntDataToEnumerationWidget(int32_t id, int64_t min, int64_t max, int64_t value, QString name, QString unit, bool bIsReadOnly);
    // This slot function passes boolean control's data
    //
    // Parameters:
    // [in] (int32_t) id - id of the control
    // [in] (bool) value - current value of the control
    // [in] (QString) name - name of the control
    // [in] (QString) unit - unit of the control
    // [in] (bool) bIsReadOnly - state which indicates whether control is readonly
    void PassBoolDataToEnumerationWidget(int32_t id, bool value, QString name, QString unit, bool bIsReadOnly);
    // This slot function passes button control's data
    //
    // Parameters:
    // [in] (int32_t) id - id of the control
    // [in] (QString) name - name of the control
    // [in] (QString) unit - unit of the control
    // [in] (bool) bIsReadOnly - state which indicates whether control is readonly
    void PassButtonDataToEnumerationWidget(int32_t id, QString name, QString unit, bool bIsReadOnly);
    // This slot function passes list control's data
    //
    // Parameters:
    // [in] (int32_t) id - id of the control
    // [in] (int32_t) value - current value of the control
    // [in] (QList<QString>) list - list of the values
    // [in] (QString) name - name of the control
    // [in] (QString) unit - unit of the control
    // [in] (bool) bIsReadOnly - state which indicates whether control is readonly
    void PassListDataToEnumerationWidget(int32_t id, int32_t value, QList<QString> list, QString name, QString unit, bool bIsReadOnly);
    // This slot function passes integer list control's data
    //
    // Parameters:
    // [in] (int32_t) id - id of the control
    // [in] (int32_t) value - current value of the control
    // [in] (QList<int64_t>) list - list of the values
    // [in] (QString) name - name of the control
    // [in] (QString) unit - unit of the control
    // [in] (bool) bIsReadOnly - state which indicates whether control is readonly
    void PassListDataToEnumerationWidget(int32_t id, int32_t value, QList<int64_t> list, QString name, QString unit, bool bIsReadOnly);

    // This slot function is called when the exposure active button was clicked
    void OnExposureActiveClicked();

    // This slot function passes invert state to the camera
    //
    // Parameters:
    // [in] (bool) state - new state
    void PassInvertState(bool state);
    // This slot function passes active line mode state to the camera
    //
    // Parameters:
    // [in] (bool) state - new state
    void PassActiveState(bool state);
    // This slot function passes active line selector value to the camera
    //
    // Parameters:
    // [in] (int32_t) state - new value
    void PassLineSelectorValue(int32_t value);
    // This slot function updates zoom label on zoom event from the CustomGraphicsView
    // object
    void OnUpdateZoomLabel();
    // This slot function is called when the dock widget is docked or undocked
    //
    //
    // Parameters:
    // [in] (bool) topLevel - state of the docker widget (docked/undocked)
    void OnDockWidgetPositionChanged(bool topLevel);

    void OnDockWidgetVisibilityChanged(bool visible);

    void OnActiveExposureWidgetClosed();
};

#endif // V4L2VIEWER_H
