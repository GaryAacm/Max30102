#include "MaxPlot.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGestureEvent>
#include <QPinchGesture>
#include <QDebug>
#include <QPixmap>
#include <QDateTime>
#include <MQTTClient.h>


MaxPlot::MaxPlot(QWidget *parent)
    : QMainWindow(parent), plot(new QCustomPlot(this)), logo(new QLabel(this)),
      sampleCount(0), windowSize(50), isTouching(false),
      max30102(nullptr)
{
    this->setStyleSheet("background-color:black;");

    max30102 = new MAX30102("/dev/i2c-4", TCA9548A_ADDR, MAX30102_ADDR);

    QWidget *centralWidget = new QWidget(this);
    centralWidget->setStyleSheet("background-color:black");
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    QPixmap logoPixmap("/home/orangepi/Desktop/zjj/Qt_use/logo.png");
    QPixmap scaledLogo = logoPixmap.scaled(300, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    logo->setPixmap(scaledLogo);
    logo->setAlignment(Qt::AlignCenter);

    logo->setStyleSheet("background-color:black;");
    logo->setFixedHeight(120);

    setupPlot();
    setupSlider();
    setupGestures();

    exitButton = new QPushButton("Exit", this);
    exitButton->setFixedSize(100, 40);
    exitButton->setStyleSheet("background-color:white;");
    connect(exitButton, &QPushButton::clicked, this, &MaxPlot::onExitButtonClicked);

    button1 = new QPushButton("fun1", this);
    button1->setFixedSize(100, 40);
    button1->setStyleSheet("background-color:white;");

    button2 = new QPushButton("fun2", this);
    button2->setFixedSize(100, 40);
    button2->setStyleSheet("background-color:white;");

    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addWidget(exitButton);
    topLayout->addWidget(button1);
    topLayout->addWidget(button2);

    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addWidget(slider);

    mainLayout->addWidget(logo, 0);
    mainLayout->addWidget(plot, 1);

    mainLayout->addLayout(topLayout);
    mainLayout->addLayout(bottomLayout, 0);

    setCentralWidget(centralWidget);

    startTime = QDateTime::currentDateTime();
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MaxPlot::updatePlot);
    timer->start(33);
    setAttribute(Qt::WA_AcceptTouchEvents);

    showFullScreen();
}

MaxPlot::~MaxPlot()
{

    delete max30102;
}

void MaxPlot::onExitButtonClicked()
{
    close();
}

void MaxPlot::setupPlot()
{
    plot->addGraph();
    plot->graph(0)->setName("Middle_RED");
    plot->graph(0)->setPen(QPen(Qt::red));
    plot->graph(0)->setLineStyle(QCPGraph::lsLine);
    plot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssNone));

    plot->addGraph();
    plot->graph(1)->setName("Middle_IR");
    plot->graph(1)->setPen(QPen(Qt::green));
    plot->graph(1)->setLineStyle(QCPGraph::lsLine);
    plot->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssNone));

    QList<QColor> colors = {Qt::gray, Qt::blue, Qt::gray, Qt::blue, Qt::cyan, Qt::darkBlue, Qt::yellow, Qt::darkYellow, Qt::magenta, Qt::darkGreen};
    for (int i = 2; i < 10; ++i)
    {
        plot->addGraph();
        if (i % 2 == 0)
        {
            plot->graph(i)->setName(QString("RED_%1").arg(i / 2));
        }
        else
        {
            plot->graph(i)->setName(QString("IR_%1").arg(i / 2));
        }

        plot->graph(i)->setPen(QPen(colors[i % colors.size()]));

        plot->graph(i)->setLineStyle(QCPGraph::lsLine);
        plot->graph(i)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssNone));
    }

    plot->xAxis->setLabel("Time (s)");
    plot->yAxis->setLabel("Amplitude");

    plot->xAxis->setRange(0, windowSize);
    plot->yAxis->setRange(0, 200000);

    plot->legend->setVisible(true);

    plot->setBackground(Qt::black);
    plot->xAxis->setBasePen(QPen(Qt::white));
    plot->yAxis->setBasePen(QPen(Qt::white));
    plot->xAxis->setTickPen(QPen(Qt::white));
    plot->yAxis->setTickPen(QPen(Qt::white));
    plot->xAxis->setSubTickPen(QPen(Qt::white));
    plot->yAxis->setSubTickPen(QPen(Qt::white));
    plot->xAxis->setLabelColor(Qt::white);
    plot->yAxis->setLabelColor(Qt::white);
    plot->xAxis->setTickLabelColor(Qt::white);
    plot->yAxis->setTickLabelColor(Qt::white);
}

void MaxPlot::setupGestures()
{
    grabGesture(Qt::PinchGesture);
}

bool MaxPlot::event(QEvent *event)
{
    if (event->type() == QEvent::Gesture)
    {
        return gestureEvent(static_cast<QGestureEvent *>(event));
    }

    return QMainWindow::event(event);
}

bool MaxPlot::gestureEvent(QGestureEvent *event)
{
    if (QGesture *pinch = event->gesture(Qt::PinchGesture))
    {
        pinchTriggered(static_cast<QPinchGesture *>(pinch));
    }
    return true;
}

void MaxPlot::setupSlider()
{
    slider = new QSlider(Qt::Horizontal, this);
    slider->setRange(0, 0);
    slider->setEnabled(false);
    slider->setStyleSheet("background-color:white;");
    connect(slider, &QSlider::valueChanged, this, &MaxPlot::onSliderMoved);
    connect(slider, &QSlider::sliderPressed, this, &MaxPlot::onSliderPressed);
    connect(slider, &QSlider::sliderReleased, this, &MaxPlot::onSliderReleased);
    isView = false;
}

void MaxPlot::onSliderPressed()
{
    isView = true;
}

void MaxPlot::onSliderReleased()
{
    int sliderMax = slider->maximum();
    int position = slider->value();
    isView = false;
}

// onSliderMoved() 函数
void MaxPlot::onSliderMoved(int position)
{
    if (isView)
    {
        plot->xAxis->setRange(position, position + windowSize);
        plot->replot();
    }
}

void MaxPlot::pinchTriggered(QPinchGesture *gesture)
{
    QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();
    if (changeFlags & QPinchGesture::ScaleFactorChanged)
    {
        qreal scaleFactor = gesture->scaleFactor();

        plot->xAxis->scaleRange(scaleFactor, plot->xAxis->range().center());
        plot->yAxis->scaleRange(scaleFactor, plot->yAxis->range().center());

        plot->replot();
    }
}

void MaxPlot::updatePlot()
{

    uint32_t middle_red, middle_ir;
    uint32_t red[8] = {0}, ir[8] = {0};
    max30102->get_middle_data(&middle_red, &middle_ir);

    max30102->get_branch_data(red, ir);

    double elapsedTime = startTime.msecsTo(QDateTime::currentDateTime()) / 1000.0;
    xData.append(elapsedTime);

    redData_middle.append(static_cast<double>(middle_red));
    irData_middle.append(static_cast<double>(middle_ir));

    red0.append(static_cast<double>(red[1]));
    ir0.append(static_cast<double>(ir[1]));
    red1.append(static_cast<double>(red[3]));
    ir1.append(static_cast<double>(ir[3]));
    red2.append(static_cast<double>(red[5]));
    ir2.append(static_cast<double>(ir[5]));
    red3.append(static_cast<double>(red[7]));
    ir3.append(static_cast<double>(ir[7]));

    plot->graph(0)->setData(xData, redData_middle);
    plot->graph(1)->setData(xData, irData_middle);
    plot->graph(2)->setData(xData, red0);
    plot->graph(3)->setData(xData, ir0);
    plot->graph(4)->setData(xData, red1);
    plot->graph(5)->setData(xData, ir1);
    plot->graph(6)->setData(xData, red2);
    plot->graph(7)->setData(xData, ir2);
    plot->graph(8)->setData(xData, red3);
    plot->graph(9)->setData(xData, ir3);

    sampleCount++;

    if (elapsedTime <= windowSize)
    {
        plot->xAxis->setRange(0, windowSize);
        slider->setRange(0, windowSize);
        slider->setEnabled(false);
    }
    else
    {
        if (!slider->isEnabled())
        {
            slider->setEnabled(true);
        }
        double maxSliderValue = elapsedTime - windowSize;

        if (!slider->isSliderDown())
        {
            slider->setRange(0, static_cast<int>(elapsedTime - windowSize));

            if (!isView)
            {

                slider->setValue(static_cast<int>(maxSliderValue));
                plot->xAxis->setRange(elapsedTime - windowSize, elapsedTime);
            }
            plot->xAxis->setRange(elapsedTime - windowSize, elapsedTime);
        }
    }
    plot->replot();
}

void MaxPlot::closeEvent(QCloseEvent *event)
{
    emit windowClosed();
    QMainWindow::closeEvent(event);
}