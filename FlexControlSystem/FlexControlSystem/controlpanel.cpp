#include "controlpanel.h"
#include "ui_controlpanel.h"
#include <QDebug>


ControlPanel::ControlPanel(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ControlPanel)
{
//    ui->setupUi(this);

    ui->setupUi(parent);

    m_pMotionGroup = new QButtonGroup();
    m_pMotionGroup->setExclusive(true);

    m_lstMotionButtons =
    {
        ui->pbUp,
        ui->pbDown,
        ui->pbLeft,
        ui->pbRight,
        ui->pbGather,
    };

    for (QPushButton *pBtn : m_lstMotionButtons)
    {
        pBtn->setCheckable(true);
        m_pMotionGroup->addButton(pBtn);
    }

    connect(m_pMotionGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked), this, &ControlPanel::OnMotionGroupButtonClicked);
}

ControlPanel::~ControlPanel()
{
    delete ui;
}


void ControlPanel::OnMotionGroupButtonClicked(QAbstractButton *pButton)
{
    if (ui->pbUp == pButton)
    {
        emit sigMotion(MotionIndex::MotionUp);
    }
}
