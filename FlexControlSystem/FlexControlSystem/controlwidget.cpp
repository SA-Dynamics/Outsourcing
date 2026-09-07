#include "controlwidget.h"
#include "ui_controlwidget.h"
#include <QDebug>
#include <QRegularExpressionValidator>


ControlWidget::ControlWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ControlWidget)
{
    ui->setupUi(this);

    m_pControlWidget = new ControlPanel(ui->ControlTabWidget);
    m_pModeWidget = new ControlPanel(ui->ModeTabWidget);

    QRegularExpression Regex("^[0-9.]+$");
    QRegularExpressionValidator *Validator = new QRegularExpressionValidator(Regex, this);
    ui->leIP->setValidator(Validator);
}

ControlWidget::~ControlWidget()
{
    delete ui;
}


void ControlWidget::on_pbUpdatePort_clicked()
{
    foreach (const QSerialPortInfo &Port, QSerialPortInfo::availablePorts())
    {
        ui->cbCOM->addItem(Port.portName());
    }
}

void ControlWidget::on_leIP_textChanged(const QString &arg1)
{

}
