#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    isCameraRunning = false;
    isCalibrate = false;
    boardSize.width = 8;
    boardSize.height = 6;
    numSeq = 0;
    numRequiredSnapshot = 20;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_startCameraButton_clicked()
{
    // the webcam is not yet started
    if(!isCameraRunning)
    {
        // open camera stream
        capture.open(CV_CAP_ANY); // default: 0

        if(!capture.isOpened())
            return;

        // set the acquired frame size to the size of its container
        capture.set(CV_CAP_PROP_FRAME_WIDTH, ui->before_img->size().width());
        capture.set(CV_CAP_PROP_FRAME_HEIGHT, ui->before_img->size().height());

        isCameraRunning = true;

        // start timer for acquiring the video
        cameraTimer.start(33); // 33 ms = 30 fps
        // at the timeout() event, execute the cameraTimerTimeout() method
        connect(&cameraTimer, SIGNAL(timeout()), this, SLOT(cameraTimerTimeout()));

        // update the user interface...
        ui->startCameraButton->setText("Stop Camera");
        ui->after_img->setText("Images needed for calibration: " + QString::number(numRequiredSnapshot));
        if (numSeq < numRequiredSnapshot)
            ui->takeSnaphotButton->setEnabled(true);
    }
    else
    {
        if(!capture.isOpened())
            return;

        // stop timer
        cameraTimer.stop();
        // release camera stream
        capture.release();

        isCameraRunning = false;

        // restore the user interface to the original status...
        ui->startCameraButton->setText("Start Camera");
        ui->before_img->clear();
        ui->after_img->clear();
        ui->takeSnaphotButton->setEnabled(false);
        ui->success_label->setText("");
    }
}

void MainWindow::cameraTimerTimeout()
{
    if(isCameraRunning && capture.isOpened())
    {
        // store the frame to show in a Qt window
        QImage frameToShow, frameUndistorted;

        // get the current frame from the video stream
        capture >> image;

        // show the chessboard pattern
        findAndDrawPoints();

        // prepare the image for the Qt format...
        // ... change color channel ordering (from BGR in our Mat to RGB in QImage)
        cvtColor(image, image, CV_BGR2RGB);

        // Qt image
        // image.step is needed to properly show some images (due to padding byte added in the Mat creation)
        frameToShow = QImage((const unsigned char*)(image.data), image.cols, image.rows, image.step, QImage::Format_RGB888);

        // display on label
        ui->before_img->setPixmap(QPixmap::fromImage(frameToShow));

        // show undistorted (after calibration) frames
        if (isCalibrate) {
            // remap of undistorted frame and conversion in the Qt format
            Mat undistorted = cameraCalib.remap(image);
            frameUndistorted = QImage((uchar*)undistorted.data, undistorted.cols, undistorted.rows, undistorted.step, QImage::Format_RGB888);
            ui->after_img->setPixmap(QPixmap::fromImage(frameUndistorted));
        }
    }
}

void MainWindow::findAndDrawPoints()
{
    std::vector<Point2f> imageCorners;
    bool found = findChessboardCorners(image, boardSize, imageCorners);
    // store the image to be used for the calibration process
    if(found)
        image.copyTo(imageSaved);
    // show the found corners on screen, if any
    drawChessboardCorners(image, boardSize, imageCorners, found);
}

void MainWindow::on_takeSnaphotButton_clicked()
{
    if (isCameraRunning && imageSaved.data)
    {
        // store the image, if valid
        imageList.push_back(imageSaved);
        numSeq++;
        startCalibration();
    }
}

// start the calibration process
void MainWindow::startCalibration()
{
    if(numSeq >= numRequiredSnapshot)
    {
        ui->takeSnaphotButton->setEnabled(false);

        // open chessboard images and extract corner points
        successes = cameraCalib.addChessboardPoints(imageList,boardSize);

        // calibrate the camera frames
        Size calibSize = Size(ui->after_img->size().width(), ui->after_img->size().height());
        cameraCalib.calibrate(calibSize);

        isCalibrate = true;
        ui->success_label->setText("Successful images used: " + QString::number(successes));
    }
}
