#include "CustomOptionWidget.h"

CustomOptionWidget::CustomOptionWidget(QString text, QWidget *parent) : QWidget(parent)
{
    m_pTextLabel = new QLabel(text, this);
    m_pLineEdit = new QLineEdit(this);
    m_pGridLayout = new QGridLayout(this);
    m_pGridLayout->addWidget(m_pTextLabel, 0, 0);
    m_pGridLayout->addWidget(m_pLineEdit, 0, 1);
    setLayout(m_pGridLayout);
}

QLineEdit *CustomOptionWidget::GetLineEditWidget()
{
    return m_pLineEdit;
}
