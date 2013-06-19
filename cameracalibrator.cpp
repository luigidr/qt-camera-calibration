#include "cameracalibrator.h"

CameraCalibrator::CameraCalibrator() :
    flag(0),
    mustInitUndistort(true)
{
}


// open chessboard images and extract corner points
int CameraCalibrator::addChessboardPoints(const std::vector<Mat>& imageList, cv::Size & boardSize)
{
    // points on the chessboard
    vector<Point2f> imageCorners;
    vector<Point3f> objectCorners;
    // 3D Scene Points
    // Initialize the chessboard corners in the chessboard reference frame.
    // The corners are at 3D location (X,Y,Z)= (i,j,0)
    for (int i=0; i<boardSize.height; i++)
        for (int j=0; j<boardSize.width; j++)
            objectCorners.push_back(Point3f(i, j, 0.0f));

    // 2D Image Points
    Mat image; // to contain the current chessboard image
    int successes = 0;
    // for all viewpoints
    int listSize = (int) imageList.size();
    for (int i=0; i<listSize; i++)
    {
        // get the image in grayscale
        image = imageList[i];
        cvtColor(image, image,CV_BGR2GRAY);
        // get the chessboard corners
        findChessboardCorners(image, boardSize, imageCorners);
        // get subpixel accuracy on the corners
        cv::cornerSubPix(
                    image,
                    imageCorners,
                    Size(5,5),
                    Size(-1,-1),
                    TermCriteria(
                        TermCriteria::MAX_ITER +
                        TermCriteria::EPS,
                        30,      // max number of iterations
                        0.1) //min accuracy
                    );
        // if we have a good board, add it to our data
        if (imageCorners.size() == (unsigned int) boardSize.area()) {
            // add image and scene points from one view
            addPoints(imageCorners, objectCorners);
            successes++;
        }
    }
    return successes;
}

// add scene points and corresponding image points
void CameraCalibrator::addPoints(const vector<Point2f>& imageCorners, const vector<Point3f>& objectCorners)
{
    // 2D image points from one view
    imagePoints.push_back(imageCorners);
    // corresponding 3D scene points
    objectPoints.push_back(objectCorners);
}

// calibrate the camera and returns the re-projection error
double CameraCalibrator::calibrate(Size &imageSize)
{
    // undistorter must be reinitialized
    mustInitUndistort = true;
    //Output rotations and translations vectors
    vector<Mat> rvecs, tvecs;
    // start calibration (you can use solvePnP instead of calibrateCamera)
    return calibrateCamera(objectPoints, // the 3D points
                           imagePoints,  // the image points
                            imageSize,    // image size
                            cameraMatrix, // output camera matrix
                            distCoeffs,   // output distortion matrix
                            rvecs, tvecs, // Rs, Ts
                            flag);        // set options
}

// remove distortion in the image (after calibration)
Mat CameraCalibrator::remap(const Mat &image)
{
    Mat undistorted;
    if (mustInitUndistort) { // called once per calibration
        initUndistortRectifyMap(
                        cameraMatrix,  // computed camera matrix
                        distCoeffs, // computed distortion matrix
                        Mat(), // optional rectification (none)
                        Mat(), // camera matrix to generate undistorted
                            image.size(),  // size of undistorted
                            CV_32FC1,      // type of output map
                            map1, map2);   // the x and y mapping functions
        mustInitUndistort = false;
    }

    // apply mapping functions
    cv::remap(image, undistorted, map1, map2, INTER_LINEAR); // interpolation type

    return undistorted;
}
