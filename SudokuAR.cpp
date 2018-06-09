// SudokuAR.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "SudokuAR.h"



const cv::Scalar SudokuAR::m_color = cv::Scalar(255, 0, 0);

const std::string SudokuAR::grayWindow = "Gray";
const std::string SudokuAR::adaptiveThresholdWindow = "Adaptive Threshold";
const std::string SudokuAR::resultsWindow = "Result";
const std::string SudokuAR::markerWindow = "Marker";
const std::string SudokuAR::trackbarsWindow = "Trackbars";


const std::string SudokuAR::blockSizeTrackbarName = "block size";
const std::string SudokuAR::constTrackbarName = "C";

//const std::string SudokuAR::houghThresholdTrackbarName = "hough thr";
//const std::string SudokuAR::minLineLengthTrackbarName = "minLineLength";
//const std::string SudokuAR::maxLineGapTrackbarName = "maxLineGap";

const int SudokuAR::blockSizeSliderMax = 1001;
const int SudokuAR::constSliderMax = 100;
const int SudokuAR::markerThresholdSliderMax = 255;

const int SudokuAR::numberOfSides = 4; // Square
const int SudokuAR::nOfIntervals = 9;

const int SudokuAR::MAX_AREA = 30000;
const std::string SudokuAR::minAreaTrackbarName = "min area";
const std::string SudokuAR::maxAreaTrackbarName = "max area";

#define NUMBERS 20

SudokuAR::SudokuAR() :
	  m_blockSizeSlider(19)
	, m_constSlider(2)
	, m_minArea(5000)
	, m_maxArea(20000)
	//, m_houghThreshold(85)
	//, m_minLineLength(105)
	//, m_maxLineGap(7)
{
	m_feedInitialized = false;
	m_playVideo = true;
	m_isFirstStripe = true;
	m_isFirstMarker = true;

	// Initialize the matrix of line parameters
	m_lineParamsMat = cv::Mat(cv::Size(4, 4), CV_32F, m_lineParams);

	////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////// Create windows to display results ////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////
	
	cv::namedWindow(grayWindow, CV_WINDOW_NORMAL);
	cv::resizeWindow(grayWindow, 500, 800);
	
	cv::namedWindow(resultsWindow, CV_WINDOW_AUTOSIZE);
	//cv::namedWindow(resultsWindow, CV_WINDOW_NORMAL);
	//cv::resizeWindow(resultsWindow, 500, 800);

	cv::namedWindow(trackbarsWindow, CV_WINDOW_NORMAL);
	cv::resizeWindow(trackbarsWindow, 500, 600);

	cv::namedWindow(markerWindow, CV_WINDOW_NORMAL);
	cv::resizeWindow(markerWindow, 500, 600);


	// Numbers
	/*for (int i = 0; i < NUMBERS; i++)
	{
		cv::namedWindow(std::to_string(i), CV_WINDOW_NORMAL);
		cv::resizeWindow(std::to_string(i), 150, 150);
	}*/
	cv::namedWindow("numbers", CV_WINDOW_NORMAL);
	cv::resizeWindow("numbers", 200, 200);
	
	

	////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////// Trackbars ///////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////

	cv::createTrackbar(blockSizeTrackbarName, trackbarsWindow,
		&m_blockSizeSlider, blockSizeSliderMax, onBlockSizeSlider, this);
	cv::createTrackbar(constTrackbarName, trackbarsWindow,
		&m_constSlider, constSliderMax);

	//cv::createTrackbar(houghThresholdTrackbarName, trackbarsWindow,
	//	&m_houghThreshold, 400);
	//cv::createTrackbar(minLineLengthTrackbarName, trackbarsWindow,
	//	&m_minLineLength, 500);
	//cv::createTrackbar(maxLineGapTrackbarName, trackbarsWindow,
	//	&m_maxLineGap, 500);

	cv::createTrackbar(minAreaTrackbarName, trackbarsWindow,
		&m_minArea, MAX_AREA);
	cv::createTrackbar(maxAreaTrackbarName, trackbarsWindow,
		&m_maxArea, MAX_AREA);

	// Create heap
	//m_memStorage = cvCreateMemStorage();

	// Set the contours pointer to NULL
	//m_contours = NULL;
}

SudokuAR::~SudokuAR()
{
	m_cap.release();
	cv::destroyAllWindows();
	//cvReleaseMemStorage(&m_memStorage); // Release heap
}

void SudokuAR::onBlockSizeSlider(int v, void* ptr)
{
	// resolve 'this':
	SudokuAR *that = (SudokuAR*)ptr;

	if (that->m_blockSizeSlider % 2 == 0) {
		that->m_blockSizeSlider -= 1;
		//cv::setTrackbarPos(blockSizeTrackbarName, adaptiveThresholdWindow, that->m_blockSizeSlider);
	}
	if (that->m_blockSizeSlider < 3) {
		that->m_blockSizeSlider = 3;
		//cv::setTrackbarPos(blockSizeTrackbarName, adaptiveThresholdWindow, that->m_blockSizeSlider);
	}
}

void SudokuAR::initVideoStream(std::string movie)
{
	if (m_cap.isOpened())
		m_cap.release();

	m_cap.open(movie); // open a movie
	if (m_cap.isOpened() == false) {
		std::cout << "No video file found. Exiting." << std::endl;
		exit(0);
	}

	m_feedInitialized = true;
}

void SudokuAR::initVideoStream()
{
	if (m_cap.isOpened())
		m_cap.release();

	m_cap.open(0); // open the default camera
	if (m_cap.isOpened() == false) {
		std::cout << "No webcam found, using a video file" << std::endl;
		exit(0);
	}

	m_feedInitialized = true;
}

bool SudokuAR::processNextFrame()
{

	m_cap >> m_src;

	// If the frame is empty, break immediately
	if (m_src.empty())
		return false;

	// 0. Rotate image (and maybe resize)
	cv::resize(m_src, m_src, cv::Size(960, 540));
	//std::cout << m_src.size << std::endl;
	cv::rotate(m_src, m_src, cv::ROTATE_90_CLOCKWISE);

	// 0. Make copies of the source image for later use
	m_dst = m_src.clone();
	
	///////////////////////////////////////////////////////////////////////////////////
	///////////////////// A. Detect sudoku grid ///////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////
	// A.1 Converto to gray scale
	cv::cvtColor(m_src, m_gray, CV_BGR2GRAY); // BGR to GRAY

	// A.2 Apply Gaussian blurring (smoothing)
	cv::GaussianBlur(m_gray, m_blur, cv::Size(5, 5), 0, 0);

	// A.3 adaptiveThreshold
	cv::adaptiveThreshold(m_blur, m_threshold, 255, cv::ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY, m_blockSizeSlider, m_constSlider);

	// A.4 Bitwise NOT operation
	cv::bitwise_not(m_threshold, m_threshold);
	
	// A.5 Dilation in order to fill up spaces between lines (not strictly necessary)
	cv::Mat kernel = (cv::Mat_<uchar>(3, 3) << 0, 1, 0, 1, 1, 1, 0, 1, 0);
	cv::dilate(m_threshold, m_threshold, kernel);
	
	// A.6. Find contours in our image and find our sudoku grid. We store the corners of 
	//      the sudoku in the atribute ```m_corners``
	m_sudokuCorners = findSudoku();

	if (m_sudokuCorners != NULL) {
		cv::circle(m_dst, m_sudokuCorners[0], 3, cv::Scalar(0, 255, 0), -1, 8, 0); // green - top-left
		cv::circle(m_dst, m_sudokuCorners[1], 3, cv::Scalar(0, 0, 255), -1, 8, 0); // red - bottom-left
		cv::circle(m_dst, m_sudokuCorners[2], 3, cv::Scalar(255, 255, 255), -1, 8, 0); // white - bottom-right
		cv::circle(m_dst, m_sudokuCorners[3], 3, cv::Scalar(0, 0, 0), -1, 8, 0); // black - top-right

		//processCorners();
		perspectiveTransform(m_sudokuCorners);
		cv::imshow(markerWindow, m_sudoku);

		// Extract the 81 subimages of the sudoku
		extractSubimages();
	}

	

	//////////////////////////////////////////////////

	// 100. Display the resulting frame
	cv::imshow(grayWindow, m_threshold);
	cv::imshow(resultsWindow, m_dst);
	

	// Release Mat
	m_src.release();
	m_dst.release();

	// Read input key from user
	char key = (char)cv::waitKey(25);
	if (key == 'q' || key == 'Q')
		return false;

	return true;
}

cv::Point* SudokuAR::findSudoku()
{
	///////////////////////////////////////////////////
	// B.1 From the linear segments image, find the contours(cv::findContours)
	cv::findContours(m_threshold, m_contours, m_hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE);

	int largestArea = 0;
	int largestContourndex = 0;
	cv::Scalar color(255, 255, 255);	

	if (!m_contours.empty() && !m_hierarchy.empty()) {

		// Go through all found contours
		for (int i = 0; i < m_contours.size(); i++)
		{
			cv::approxPolyDP(m_contours[i], m_approx, cv::arcLength(m_contours[i], true) * 0.02, true);

			// If contour does not have 4 sides, exit iteration
			if (m_approx.size() != 4) continue;

			m_boundingBox = cv::boundingRect(m_contours[i]);
			//putText(m_dst, std::to_string(numChildren), m_boundingBox.tl(), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar(0, 0, 255, 255));

			int iArea = m_boundingBox.area();
			std::string sArea = std::to_string(iArea);

			if (iArea < m_minArea || iArea >(m_threshold.cols * m_threshold.rows - 10000)) continue;

			/////////////////////////////////////////

			// Get first child
			int firstChild = m_hierarchy[i][2];
			// If no first child, exit iteration
			if (firstChild == -1) continue;

			int numChildren = 0;
			int prevChild = firstChild;
			int nextChild = -1;
			for (int i = 0; i < 1000; i++)
			{
				if (m_hierarchy[prevChild][0] >= 0)
				{
					nextChild = m_hierarchy[prevChild][0];
					cv::approxPolyDP(m_contours[nextChild], m_approxChild, cv::arcLength(m_contours[nextChild], true) * 0.02, true);
					if (m_approxChild.size() != 4) continue;
					//m_square = (cv::Point*) &m_approxChild[0];
					//cv::polylines(m_dst, &m_square, &numberOfSides, 1, true, cv::Scalar(0, 255, 0), 2, 8, 0);
					prevChild = nextChild;
					numChildren++;
				}
			}

			if (numChildren < 20) continue;

			cv::Point* points(&m_approx[0]);
			cv::polylines(m_dst, &points, &numberOfSides, 1, true, cv::Scalar(0, 0, 255), 2, 8, 0);
			return points;
			
		} // Contours	
	}
	return NULL;
}

/*
void SudokuAR::findMarkers()
{
	///////////////////////////////////////////////////
	// B.1 From the linear segments image, find the contours(cv::findContours)
	tmpMat = m_threshold.clone();
	tmpIpl = tmpMat;
	cvFindContours(&tmpIpl, m_memStorage, &m_contours, sizeof(CvContour), CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE);

	int maxArea = 0;

	// Go through all found contours
	for (; m_contours != 0; m_contours = m_contours->h_next)
	{
		// 1. Approximate a contour with another contour with less vertices 
		//	  so that the distance between them is less or equal to the specified precision
		//    It provides another CvSeq*
		m_approx = cvApproxPoly(m_contours, sizeof(CvContour), m_memStorage, CV_POLY_APPROX_DP, cvContourPerimeter(m_contours)*0.02, 0);

		// 2. Exit the present iteration if the number of points of the approximated curve 
		//    is different from 4 points
		if (m_approx->total != numberOfSides) continue;

		// 3. Convert the CvSeq* to type cv::Mat
		m_result = cv::cvarrToMat(m_approx);

		// 4. Find a bounding box that contains the approximated polygon AND draw a rectangle
		m_boundingBox = cv::boundingRect(m_result);

		int iArea = m_boundingBox.area();
		std::string sArea = std::to_string(iArea);

		if (iArea < m_minArea || iArea > (m_threshold.cols * m_threshold.rows - 10000)) continue;

		// 
		if (iArea > maxArea) {
			maxArea = iArea;
			// Create a pointer of cv::Point to store the 4 corners of the quadrilateral
			m_square = (const cv::Point*) m_result.data;
		}
	} // Contours	

	  // Draw lines along the 4 corners of the quadrilateral
	cv::polylines(m_dst, &m_square, &numberOfSides, 1, true, cv::Scalar(0, 0, 255), 1, 8, 0);

}
*/

void SudokuAR::perspectiveTransform(const cv::Point* corners) {

	int maxLength = (int)sqrt((corners[0].x - corners[1].x)*(corners[0].x - corners[1].x) + (corners[0].y - corners[1].y)*(corners[0].y - corners[1].y));

	cv::Point2f sourceCorners[4];
	cv::Point2f targetCorners[4];
	
	// When using subpixel accuracy
	sourceCorners[0] = corners[3];         targetCorners[0] = cv::Point2f(0, 0);
	sourceCorners[1] = corners[2];		   targetCorners[1] = cv::Point2f(maxLength - 1, 0);
	sourceCorners[2] = corners[1];         targetCorners[2] = cv::Point2f(maxLength - 1, maxLength - 1);
	sourceCorners[3] = corners[0];         targetCorners[3] = cv::Point2f(0, maxLength - 1);

	// When NOT using subpixel accuracy
	/*
	sourceCorners[0] = corners[0];         targetCorners[0] = cv::Point2f(0, 0);
	sourceCorners[1] = corners[3];		   targetCorners[1] = cv::Point2f(maxLength - 1, 0);
	sourceCorners[2] = corners[2];         targetCorners[2] = cv::Point2f(maxLength - 1, maxLength - 1);
	sourceCorners[3] = corners[1];         targetCorners[3] = cv::Point2f(0, maxLength - 1);
	*/

	// Create and compute matrix of perspective transform
	cv::Mat projMat(cv::Size(3, 3), CV_32FC1);
	projMat = cv::getPerspectiveTransform(sourceCorners, targetCorners);

	// Initialize image for the sudoku
	m_sudoku = cv::Mat(cv::Size(maxLength, maxLength), CV_8UC1);

	// Change the perspective in the marker image using the previously calculated matrix
	cv::warpPerspective(m_gray, m_sudoku, projMat, cv::Size(maxLength, maxLength));

	cv::rotate(m_sudoku, m_sudoku, cv::ROTATE_90_COUNTERCLOCKWISE);
}

void SudokuAR::extractSubimages() {
	
	cv::resize(m_sudoku, m_sudoku, cv::Size(288, 288));

	int width = m_sudoku.cols;
	int height = m_sudoku.rows;

	std::cout << width << " " << height << std::endl;

	int subWidth = (width / 9);
	int subHeight = (height / 9);

	std::cout << subWidth << " " << subHeight << std::endl;

	cv::Size subSize(subWidth, subHeight);

	/////
	float totalAverage = 0;
	float average = 0;
	/////
	
	cv::Mat reconstructedSudoku;
	int idx = 0;
	for (unsigned v = 0; v < height; v += subHeight)
	{
		for (unsigned u = 0; u < width; u += subWidth)
		{
			// Get subimage
			cv::Mat subimage = cv::Mat(m_sudoku, cv::Rect(u, v, subWidth, subHeight)).clone();

			// Crop the image in order to only have the number, no borders at all! --> CNNs don't like that
			/* Set Region of Interest */
			int offset_x = 4;
			int offset_y = 4;

			cv::Rect roi;
			roi.x = offset_x;
			roi.y = offset_y;
			roi.width = subimage.size().width - (offset_x * 2);
			roi.height = subimage.size().height - (offset_y * 2);

			/* Crop the original image to the defined ROI */
			subimage = subimage(roi);

			// Push it into the vector of subimages
			m_subimages[idx++] = subimage;

			average = (float) mean(subimage)[0];
			//std::cout << average << std::endl;
			totalAverage += average;
		}
	}

	totalAverage /= 81;
	std::cout << "AVERAGE: " << totalAverage << std::endl;

	for (int i = 0; i < 81; i++)
	{
		average = (float)mean(m_subimages[i])[0];

		if (average < totalAverage - 6) {
			m_subimagesIsImage[i] = true;
			putText(m_subimages[i], "T", cv::Point(0, 10), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar(255, 0, 255, 0));
		}
		else {
			m_subimagesIsImage[i] = false;
			putText(m_subimages[i], "F", cv::Point(5, 20), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar(0, 0, 255, 255));
		}

		std::cout << average << std::endl;

		cv::imshow("numbers", m_subimages[i]);
		cv::waitKey();
	}
}

void SudokuAR::processCorners()
{
	// Go through the 4 corners of the found quadrilateral
	for (int corner = 0; corner < numberOfSides; corner++)
	{
		// Draw circles on the corners of the rectangle
		cv::circle(m_dst, m_sudokuCorners[corner], 1, cv::Scalar(0, 255, 0), -1, 8, 0);

		// For every corner
		// get the intervals between the current corner and the one next to it
		double dx = (double)(m_sudokuCorners[(corner + 1) % 4].x - m_sudokuCorners[corner].x) / float(nOfIntervals);
		double dy = (double)(m_sudokuCorners[(corner + 1) % 4].y - m_sudokuCorners[corner].y) / float(nOfIntervals);

		// For every side of the quadrilateral, compute a Stripe object (a cvMat, actually)
		// which we will position at the location of every delimiter (there are 6) between
		// the current corner and the one next to it
		computeStripe(dx, dy);

		// Array for accurate delimiters
		cv::Point2f delimiters[nOfIntervals-1];

		// Go through every delimiter
		for (int delim = 1; delim < nOfIntervals; ++delim) // [1, 2, 3, 4, 5, 6, ..., nOfIntervals-1]
		{
			// Compute the distance between the current corner and the j-th delimiter
			double px = (double)m_sudokuCorners[corner].x + (double)delim*dx;
			double py = (double)m_sudokuCorners[corner].y + (double)delim*dy;

			// Create a cvPoint, 'p', at the location of the j-th delimiter
			cv::Point p;
			p.x = (int)px;
			p.y = (int)py;
			// Draw a BLUE circle at the location of the j-th delimiter
			//cv::circle(m_dst, p, 1, cv::Scalar(255, 0, 0), -1, 8, 0);

			// Do sub-pixel detection of the current delimiter
			// Add the computed accurate delimiter to the list of delimiters
			delimiters[delim - 1] = computeAccurateDelimiter(p);

		} // 6 Points in each edge

		  // We now have the array of exact edge centers stored in "delimiters"
		  // We will create a matrix that contains these delimiters
		cv::Mat delimiters_mat(cv::Size(1, nOfIntervals-1), CV_32FC2, delimiters);

		// And now we fit a line through them
		cv::fitLine(delimiters_mat, m_lineParamsMat.col(corner), CV_DIST_L2, 0, 0.01, 0.01);
		// cvFitLine stores the calculated line in lineParams in the following way:
		//   vec1.x,   vec2.x,   vec3.x,   vec4.x,
		//   vec1.y,   vec2.y,   vec3.y,   vec4.y,
		// point1.x, point2.x, point3.x, point4.y
		// point1.y, point2.y, point3.x, point4.y

		// We can draw the line by computing two points that reside on it
		// First, we grab the DIRECTION VECTOR and the ORIGIN POINT of the line
		cv::Point2f dir;
		dir.x = m_lineParams[corner];
		dir.y = m_lineParams[4 + corner];

		cv::Point p0;
		p0.x = (int)m_lineParams[8 + corner];
		p0.y = (int)m_lineParams[12 + corner];

		// Now we can get two points by travelling from p0 down the directions vector
		float lineLength = 50.0;
		cv::Point q1(p0.x - (int)(lineLength * dir.x), p0.y - (int)(lineLength * dir.y));
		cv::Point q2(p0.x + (int)(lineLength * dir.x), p0.y + (int)(lineLength * dir.y));

		// And finally we can draw a line that represents the current EDGE
		cv::line(m_dst, q1, q2, CV_RGB(0, 255, 255), 1, 8, 0);

	} // 4 Edges

	  // So far we have stored the exact line parameters and shown the lines in the image
	  // Now we have to calculate the exact corners by intersecting these lines

	for (int lineIdx = 0; lineIdx < 4; lineIdx++)
	{
		// We will look for the intersection of line 'lineIdx' and line 'nextLineIdx'
		int nextLineIdx = (lineIdx + 1) % 4;

		// Params for 'lineIdx'
		double u0, v0, x0, y0;
		// Params for 'nextLineIdx'
		double u1, v1, x1, y1;

		// Params of line 'lineIdx'
		u0 = m_lineParams[lineIdx];
		v0 = m_lineParams[4 + lineIdx];
		x0 = m_lineParams[8 + lineIdx];
		y0 = m_lineParams[12 + lineIdx];

		// Params of line 'nextLineIdx'
		u1 = m_lineParams[nextLineIdx];
		v1 = m_lineParams[4 + nextLineIdx];
		x1 = m_lineParams[8 + nextLineIdx];
		y1 = m_lineParams[12 + nextLineIdx];

		//////////////////////////////////////////////////////////////////////////////////
		// Solve the intersection
		double a = x1 * u0*v1 - y1 * u0*u1 - x0 * u1*v0 + y0 * u0*u1;
		double b = -x0 * v0*v1 + y0 * u0*v1 + x1 * v0*v1 - y1 * v0*u1;
		double c = v1 * u0 - v0 * u1;

		// Check if lines are parallel
		if (fabs(c) < 0.001) {
			//std::cout << "Lines are parallel!" << std::endl;
			// Skip this iteration
			continue;
		}

		a /= c;
		b /= c;
		//////////////////////////////////////////////////////////////////////////////////

		// Save the exact corner for the intersection of lines 'lineIdx' and 'nextLineIdx'
		m_sudokuCorners[lineIdx].x = a;
		m_sudokuCorners[lineIdx].y = b;

		// Draw the new exact corners
		cv::Point exactPoint;
		exactPoint.x = (int)m_sudokuCorners[lineIdx].x;
		exactPoint.y = (int)m_sudokuCorners[lineIdx].y;
		cv::circle(m_dst, exactPoint, 5, CV_RGB(255, 255, 0), -1);
	}
}

/**
* Defines a stripe (actually, a cvMat) object, which we will then position
* at the location of every delimiter between the curret corner and the one next to it
*/
void SudokuAR::computeStripe(double dx, double dy) {

	double diffLength = sqrt(dx*dx + dy * dy);

	// -- Stripe length (in pixels) -- //
	// We arbitrarily set the length of the stripe to 0.8 times the length of an interval
	m_stripe.length = (int)(0.8 * diffLength);
	if (m_stripe.length < 5)
		m_stripe.length = 5;

	// Make stripe's length odd (0101 | 0001 = 0101 ---- 0100 | 0001 = 0101)
	m_stripe.length |= 1;

	// Find initial and final indices, e.g. stripeLength = 5 --> from -2 to 2
	m_stripe.nStop = m_stripe.length >> 1; // Note that: 0101 >> 1 = 0010
	m_stripe.nStart = -m_stripe.nStop;		// Simply get the negative of the previous value

											// Create OpenCV cv::Size variable
	cv::Size stripeSize;
	// Sample a stripe of width 3 pixels
	stripeSize.width = 3;
	stripeSize.height = m_stripe.length;

	// Create normalized Direction Vectors of the Stripe 
	// vecX
	m_stripe.vecX.x = dx / diffLength;
	m_stripe.vecX.y = dy / diffLength;
	// vecY
	m_stripe.vecY.x = m_stripe.vecX.y;
	m_stripe.vecY.y = -m_stripe.vecX.x;

	// Create the Stripe matrix
	m_iplStripe = cv::Mat(stripeSize, CV_8UC1);
}

/**
* At the location of every delimiter 'p', do sub-pixel edge localization
*/
cv::Point2f SudokuAR::computeAccurateDelimiter(cv::Point p) {

	// We have to go over all sub-pixels of the Stripe, a total 
	// of <width*length> sub-pixels, e.g., 3*5 = 15
	// We will fill in all Stripe's sub-pixels with an average of the gray value
	// of the real pixels that surround each sub-pixel of the Stripe

	// Go over the Stripe's WIDTH
	// Stripes always have a width of 3 pixels, so we go over [-1, 0, 1]
	for (int m = -1; m <= 1; m++)
	{
		// Go over the Stripe's LENGTH
		// Usually it is 5 pixels, so we go over [-2, -1, 0, 1, 2]
		for (int n = m_stripe.nStart; n <= m_stripe.nStop; n++)
		{
			// Compute the location of each sub-pixel of the Stripe (in double first, for accuracy)
			cv::Point2f subPixel;
			subPixel.x = p.x + ((double)m * m_stripe.vecX.x) + ((double)n * m_stripe.vecY.x);
			subPixel.y = p.y + ((double)m * m_stripe.vecX.y) + ((double)n * m_stripe.vecY.y);

			// Convert the location of each sub-pixel of the Stripe to real sub-pixels of the images
			cv::Point p2;
			p2.x = (int)subPixel.x;
			p2.y = (int)subPixel.y;

			// Draw the current Stripe's sub-pixel by using the approximation computed above
			if (m_isFirstStripe)
				cv::circle(m_dst, p2, 1, cv::Scalar(255, 0, 255), -1);
			else
				cv::circle(m_dst, p2, 1, cv::Scalar(0, 255, 255), -1);

			// Get the gray-scale color of the image at the location of the current Stripe's sub-pixel
			int pixel = subpixSampleSafe(m_gray, subPixel);

			// Store the sub-pixel's colors in our iplStripe matrix
			int w = m + 1; // shifts to 0 ... 2
			int h = n + (m_stripe.length >> 1); // shifts to 0 ... stripeLength

												////////////////////////////////////////////////////////////
												// IMPORTANT!!!!! We will save it as a VERTICAL STRIPE !!!!! --> (lengthStripe x 3)
												// (x, x, x)
												// (x, x, x)
												// (x, x, x)
												// (x, x, x)
												// (x, x, x)
												// (x, x, x)
												// (x, x, x)
												////////////////////////////////////////////////////////////
			m_iplStripe.at<uchar>(h, w) = (uchar)pixel;

			// So we end up with a matrix of stripeLength x 3, which is our Stripe
			// We want now to find the exact point at which the edge of the quadrilateral is
		}
	}

	// Finally, compute the Sobel operator at the location of our current delimiter 'p'
	return computeSobel(p);
}

/**
* Returns the color of the subpixel by looking at the four pixels that surround it
*/
int SudokuAR::subpixSampleSafe(const cv::Mat& gray, const cv::Point2f& p) {

	int x = int(floorf(p.x));
	int y = int(floorf(p.y));

	if (x < 0 || x >= gray.cols - 1 ||
		y < 0 || y >= gray.rows - 1)
		return 127;

	int dx = int(256 * (p.x - floorf(p.x)));
	int dy = int(256 * (p.y - floorf(p.y)));

	//////////////////////////////////////////////////////////////////////////////////////////////

	// Pointer to the pixel with the smallest coordinates nearest to point 'p' 
	// Note: gray.step returns the width of image
	unsigned char* i = (unsigned char*)((gray.data + y * gray.step) + x);

	//std::cout << static_cast<unsigned>(i[0]) << " " << static_cast<unsigned>(i[1]) << std::endl;

	// Compute the average gray value of the two UPPER pixels
	// by taking into account how much of them (of their area) belongs to the sub-pixel 
	// whose gray value we are looking for
	int upper_row_avg = i[0] + ((dx * (i[1] - i[0])) >> 8);

	// Set the pointer to the pixel beneath the initial pixel (bottom row of the 4-pixel square)
	i += gray.step;

	// Compute the average gray value of the two LOWER pixels
	// by taking into account how much of them (of their area) belongs to the sub-pixel 
	// whose gray value we are looking for
	int lower_row_avg = i[0] + ((dx * (i[1] - i[0])) >> 8);

	// Compute final average between the UPPER and LOWER rows
	int final_avg = upper_row_avg + ((dy * (lower_row_avg - upper_row_avg)) >> 8);

	return final_avg;
}

cv::Point2f SudokuAR::computeSobel(cv::Point p) {
	// Sobel operator on stripe
	// (-1, -2, -1)
	// ( 0,  0,  0)
	// ( 1,  2,  1)

	// Stripe (stripeLength x 3, e.g., 7)
	// (x, x, x)
	// (x, C, x)
	// (x, C, x)
	// (x, C, x)
	// (x, C, x)
	// (x, C, x)
	// (x, x, x)

	// By convolutionazing our Sobel kernel onto our [stripeLength x 3] matrix,
	// we will get a vector of the same length as our stripe minus 2

	// Result
	// (C', 
	//  C', 
	//  C', 
	//  C', 
	//  C')

	int numSobelValues = m_stripe.length - 2;
	std::vector<double> sobelValues(numSobelValues);

	// Go over al the centers of our stripe (i.e., the C values of the Stripe we drew above)
	// and apply the Sobel operator
	for (int n = 1; n < (m_stripe.length - 1); n++)
	{
		// Get a pointer to the first element of our Stripe
		unsigned char* stripePtr = &(m_iplStripe.at<uchar>(n - 1, 0));

		double row1 = -stripePtr[0] - 2 * stripePtr[1] - stripePtr[2];
		stripePtr += 2 * m_iplStripe.step;

		double row3 = stripePtr[0] + 2 * stripePtr[1] + stripePtr[2];
		sobelValues[n - 1] = row1 + row3;
	}


	// Now go over the Sobel values and find the MAX value and its index

	double maxVal = -1;
	int maxIndex = 0;

	for (int n = 0; n < numSobelValues; ++n)
	{
		if (sobelValues[n] > maxVal)
		{
			maxVal = sobelValues[n];
			maxIndex = n;
		}
	}

	//  Use the maximum from the previous step and its two neighbors as positions 0, 1 and -1 on the x axis
	double y0, y1, y2; // y0 .. y1 .. y2
	y0 = (maxIndex == 0) ? 0 : sobelValues[maxIndex - 1];
	y1 = sobelValues[maxIndex];
	y2 = (maxIndex == (numSobelValues - 1)) ? 0 : sobelValues[maxIndex + 1];

	// Formula for calculating the x-coordinate of the vertex of a parabola, given 3 points with equal distances 
	// (xv means the x value of the vertex, d the distance between the points): 
	// xv = x1 + (d / 2) * (y2 - y0)/(2*y1 - y0 - y2)

	double pos = (y2 - y0) / (4 * y1 - 2 * y0 - 2 * y2); //d = 1 because of the normalization and x1 will be added later
														 // This would be a valid check, too
														 //if (std::isinf(pos)) {
														 //	// value is infinity
														 //	continue;
														 //}

	if (pos != pos) {
		// value is not a number, so return the original
		// "inaccurate" location of the delimiter
		return cv::Point2f(p.x, p.y);
	}

	////////////////////////////////////////
	// Exact point with subpixel accuracy //
	////////////////////////////////////////
	cv::Point2f edgeCenter;
	int maxIndexShift = maxIndex - (m_stripe.length >> 1);

	// Shift the original edgepoint accordingly
	// Recall that vecY is a normalized vector that points in the direction of the Stripe's length
	edgeCenter.x = (double)p.x + (((double)maxIndexShift + pos) * m_stripe.vecY.x);
	edgeCenter.y = (double)p.y + (((double)maxIndexShift + pos) * m_stripe.vecY.y);

	// Draw circle at the new sup-pixel-accurate points
	cv::Point p_tmp;
	p_tmp.x = (int)edgeCenter.x;
	p_tmp.y = (int)edgeCenter.y;
	cv::circle(m_dst, p_tmp, 1, CV_RGB(0, 0, 255), -1);

	// Show a window with the Stripe
	if (m_isFirstStripe)
	{
		cv::Mat iplTmp;
		cv::resize(m_iplStripe, iplTmp, cv::Size(100, 300));
		cv::imshow("we", iplTmp);
		m_isFirstStripe = false;
	}

	return edgeCenter;
}

void SudokuAR::orientSudokuCorners() {
	return;
}