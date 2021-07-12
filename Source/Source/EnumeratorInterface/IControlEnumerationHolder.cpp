/*=============================================================================
  Copyright (C) 2021 Allied Vision Technologies.  All Rights Reserved.

  Redistribution of this file, in original or modified form, without
  prior written consent of Allied Vision Technologies is prohibited.

-------------------------------------------------------------------------------

  File:        IControlEnumerationHolder.cpp

  Description: This interface class describes how the derived control
               classes should looks like.

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

#include "IControlEnumerationHolder.h"
#include <QMouseEvent>

IControlEnumerationHolder::IControlEnumerationHolder(int32_t id, QString name, QWidget *parent) :
    QWidget(parent),
    m_id(id)
{
    m_NameOfControl.setText(name);
    setLayout(&m_MainLayout);
    m_MainLayout.setMargin(0);
}

IControlEnumerationHolder::~IControlEnumerationHolder()
{

}

int32_t IControlEnumerationHolder::GetWidgetControlId()
{
    return m_id;
}

QString IControlEnumerationHolder::GetControlInfo()
{
    return m_ControlInfo;
}
