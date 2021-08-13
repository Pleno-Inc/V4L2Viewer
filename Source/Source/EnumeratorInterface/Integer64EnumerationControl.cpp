/*=============================================================================
  Copyright (C) 2021 Allied Vision Technologies.  All Rights Reserved.

  Redistribution of this file, in original or modified form, without
  prior written consent of Allied Vision Technologies is prohibited.

-------------------------------------------------------------------------------

  File:        Integer64EnumerationControl.cpp

  Description: This class inherits by IControlEnumerationHolder. It contains
               all necessary data for int64 controls which are enumerated.

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

#include "Integer64EnumerationControl.h"
#include "linux/v4l2-controls.h"
#include <math.h>

Integer64EnumerationControl::Integer64EnumerationControl(int32_t id, int64_t min, int64_t max, int64_t value, QString name, QString unit, bool bIsReadOnly, QWidget *parent):
    IControlEnumerationHolder(id, name, parent),
    m_Min(min),
    m_Max(max),
    m_Value(value)
{
    m_ControlInfo = QString(tr("%1 control accepts 64-bit integers. \n Minimum: %2 \n Maximum: %3 \n Unit: %4")
                          .arg(name)
                          .arg(m_Min)
                          .arg(m_Max)
                          .arg(unit));
    m_LineEdit.setValidator(new QIntValidator(m_Min, m_Max, this));
    m_LineEdit.setText(QString::number(m_Value));
    m_CurrentValue.setText(QString::number(m_Value));
    m_Slider.setOrientation(Qt::Horizontal);

    m_MainLayout.addWidget(&m_NameOfControl, 0, 0);
    m_MainLayout.addWidget(&m_Slider, 0, 1);
    m_MainLayout.addWidget(&m_LineEdit, 0, 2);

    m_MainLayout.setColumnStretch(0, 2);
    m_MainLayout.setColumnStretch(1, 2);
    m_MainLayout.setColumnStretch(2, 1);

    if (bIsReadOnly)
    {
        setEnabled(false);
        m_ControlInfo += tr("\n Control is READONLY");
    }
    else
    {
        setEnabled(true);
        connect(&m_LineEdit, SIGNAL(editingFinished()), this, SLOT(OnLineEditPressed()));
        connect(&m_Slider, SIGNAL(sliderReleased()), this, SLOT(OnSliderReleased()));
        if (id == V4L2_CID_EXPOSURE_ABSOLUTE)
        {
            connect(&m_Slider, SIGNAL(valueChanged(int)), this, SLOT(OnSliderLogValueChanged(int)));
            m_Slider.setMaximum(1000);
            m_Slider.setMinimum(0);
            m_Slider.setValue(GetSliderLogValue(value));
        }
        else
        {
            connect(&m_Slider, SIGNAL(valueChanged(int)), this, SLOT(OnSliderValueChanged(int)));
            m_Slider.setMaximum(max);
            m_Slider.setMinimum(min);
            m_Slider.setValue(value);
        }
    }
}

void Integer64EnumerationControl::UpdateValue(int64_t value)
{
    m_LineEdit.blockSignals(true);
    m_LineEdit.setText(QString::number(value));
    m_CurrentValue.setText(QString::number(value));
    m_LineEdit.blockSignals(false);

    m_Slider.blockSignals(true);
    if (m_id != V4L2_CID_EXPOSURE_ABSOLUTE)
    {
        m_Slider.setValue(value);
    }
    else
    {
        m_Slider.setValue(GetSliderLogValue(value));
    }
    m_Slider.blockSignals(false);
}

void Integer64EnumerationControl::OnLineEditPressed()
{
    int64_t value = static_cast<int64_t>(m_LineEdit.text().toLongLong());
    emit PassNewValue(m_id, value);
}

void Integer64EnumerationControl::OnSliderValueChanged(int value)
{
    m_LineEdit.setText(QString::number(value));
    m_SliderValue = static_cast<int64_t>(value);

    emit PassSliderValue(m_id, m_SliderValue);
}

void Integer64EnumerationControl::OnSliderLogValueChanged(int value)
{
    int minSliderValue = m_Slider.minimum();
    int maxSliderValue = m_Slider.maximum();

    double minExposure = log(m_Min);
    double maxExposure = log(m_Max);

    double scale = (maxExposure - minExposure) / (maxSliderValue - minSliderValue);

    double outValue = exp(minExposure + scale*(value - minSliderValue));
    m_SliderValue = static_cast<int64_t>(outValue);
    m_LineEdit.setText(QString::number(m_SliderValue));

    emit PassSliderValue(m_id, m_SliderValue);
}

void Integer64EnumerationControl::OnSliderReleased()
{
    emit PassNewValue(m_id, m_SliderValue);
}

int32_t Integer64EnumerationControl::GetSliderLogValue(int64_t value)
{
    double logExposureMin = log(m_Min);
    double logExposureMax = log(m_Max);
    int32_t minSliderExp = m_Slider.minimum();
    int32_t maxSliderExp = m_Slider.maximum();
    double scale = (logExposureMax - logExposureMin) / (maxSliderExp - minSliderExp);
    double result = minSliderExp + ( log(value) - logExposureMin ) / scale;
    return static_cast<int32_t>(result);
}
