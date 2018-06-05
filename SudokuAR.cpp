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

const std::string SudokuAR::houghThresholdTrackbarName = "hough thr";
const std::string SudokuAR::minLineLengthTrackbarName = "minLineLength";
const std::string SudokuAR::maxLineGapTrackbarName = "maxLineGap";

const int SudokuAR::blockSizeSliderMax = 1001;
const int SudokuAR::constSliderMax = 100;
const int SudokuAR::markerThresholdSliderMax = 255;

const int SudokuAR::numberOfSides = 4; // Square
const int SudokuAR::nOfIntervals = 7;

const int SudokuAR::MAX_AREA = 30000;
const std::string SudokuAR::minAreaTrackbarName = "min area";
const std::string SudokuAR::maxAreaTrackbarName = "max area";


SudokuAR::SudokuAR() :
	  m_blockSizeSlider(5)
	, m_constSlider(2)
	, m_minArea(4000)
	, m_maxArea(20000)
	, m_houghThreshold(85)
	, m_minLineLength(105)
	, m_maxLineGap(7)
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

	////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////// Trackbars ///////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////

	cv::createTrackbar(blockSizeTrackbarName, trackbarsWindow,
		&m_blockSizeSlider, blockSizeSliderMax, onBlockSizeSlider, this);
	cv::createTrackbar(constTrackbarName, trackbarsWindow,
		&m_constSlider, constSliderMax);

	cv::createTrackbar(houghThresholdTrackbarName, trackbarsWindow,
		&m_houghThreshold, 400);
	cv::createTrackbar(minLineLengthTrackbarName, trackbarsWindow,
		&m_minLineLength, 500);
	cv::createTrackbar(maxLineGapTrackbarName, trackbarsWindow,
		&m_maxLineGap, 500);

	cv::createTrackbar(minAreaTrackbarName, trackbarsWindow,
		&m_minArea, MAX_AREA);
	cv::createTrackbar(maxAreaTrackbarName, trackbarsWindow,
		&m_maxArea, MAX_AREA);

	// Create heap
	m_memStorage = cvCreateMemStorage();

	// Set the contours pointer to NULL
	m_contours = NULL;
}

SudokuAR::~SudokuAR()
{
	m_cap.release();
	cv::destroyAllWindows();
	cvReleaseMemStorage(&m_memStorage); // Release heap
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
	cv::GaussianBlur(m_gray, m_blur, cv::Size(11, 11), 0, 0);

	// A.3 adaptiveThreshold
	cv::adaptiveThreshold(m_blur, m_threshold, 255, cv::ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY, m_blockSizeSlider, m_constSlider);

	// A.4 Bitwise NOT operation
	cv::bitwise_not(m_threshold, m_threshold);
	
	// A.5 Dilation in order to fill up spaces between lines (not strictly necessary)
	cv::Mat kernel = (cv::Mat_<uchar>(3, 3) << 0, 1, 0, 1, 1, 1, 0, 1, 0);
	cv::dilate(m_threshold, m_threshold, kernel);
	
	// A.6. Find contours in our image and find our sudoku grid. We store the corners of 
	//      the sudoku in the atribute ```m_corners``
	findMarkers();

	cv::circle(m_dst, m_square[0], 3, cv::Scalar(0, 255, 0), -1, 8, 0); // green - TOP-LEFT
	cv::circle(m_dst, m_square[1], 3, cv::Scalar(0, 0, 255), -1, 8, 0); // RED - BOTTOM-LEFT
	cv::circle(m_dst, m_square[2], 3, cv::Scalar(255, 255, 255), -1, 8, 0); // WHITE - BOTTOM-RIGHT
	cv::circle(m_dst, m_square[3], 3, cv::Scalar(0, 0, 0), -1, 8, 0); // BLACK - TOP-RIGHT

	perspectiveTransform();

	//////////////////////////////////////////////////


	// 100. Display the resulting frame
	cv::imshow(grayWindow, m_threshold);
	cv::imshow(resultsWindow, m_dst);
	cv::imshow(markerWindow, m_sudoku);

	// Release Mat
	m_src.release();
	m_dst.release();

	// Read input key from user
	char key = (char)cv::waitKey(25);
	if (key == 'q' || key == 'Q')
		return false;

	return true;
}

void SudokuAR::perspectiveTransform() {

	int maxLength = sqrt((m_square[0].x - m_square[1].x)*(m_square[0].x - m_square[1].x) + (m_square[0].y - m_square[1].y)*(m_square[0].y - m_square[1].y));

	cv::Point2f sourceCorners[4];
	cv::Point2f targetCorners[4];
	sourceCorners[0] = m_square[0];         targetCorners[0] = cv::Point2f(0, 0);
	sourceCorners[1] = m_square[3];		    targetCorners[1] = cv::Point2f(maxLength - 1, 0);
	sourceCorners[2] = m_square[2];         targetCorners[2] = cv::Point2f(maxLength - 1, maxLength - 1);
	sourceCorners[3] = m_square[1];         targetCorners[3] = cv::Point2f(0, maxLength - 1);

	// Create and compute matrix of perspective transform
	cv::Mat projMat(cv::Size(3, 3), CV_32FC1);
	projMat = cv::getPerspectiveTransform(sourceCorners, targetCorners);

	// Initialize image for the sudoku
	m_sudoku = cv::Mat(cv::Size(maxLength, maxLength), CV_8UC1);

	// Change the perspective in the marker image using the previously calculated matrix
	cv::warpPerspective(m_gray, m_sudoku, projMat, cv::Size(maxLength, maxLength));
}

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

void SudokuAR::processCorners()
{
	// Go through the 4 corners of the found quadrilateral
	for (int corner = 0; corner < numberOfSides; corner++)
	{
		// Draw circles on the corners of the rectangle
		cv::circle(m_dst, m_square[corner], 1, cv::Scalar(0, 255, 0), -1, 8, 0);

		// For every corner
		// get the 7 intervals between the current corner and the one next to it
		double dx = (double)(m_square[(corner + 1) % 4].x - m_square[corner].x) / 7.0;
		double dy = (double)(m_square[(corner + 1) % 4].y - m_square[corner].y) / 7.0;

		// For every side of the quadrilateral, compute a Stripe object (a cvMat, actually)
		// which we will position at the location of every delimiter (there are 6) between
		// the current corner and the one next to it
		computeStripe(dx, dy);

		// Array for accurate delimiters
		cv::Point2f delimiters[6];

		// Go through every delimiter (6 delimiters)
		for (int delim = 1; delim < 7; ++delim) // [1, 2, 3, 4, 5, 6]
		{
			// Compute the distance between the current corner and the j-th delimiter
			double px = (double)m_square[corner].x + (double)delim*dx;
			double py = (double)m_square[corner].y + (double)delim*dy;

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
		cv::Mat delimiters_mat(cv::Size(1, 6), CV_32FC2, delimiters);

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
		m_corners[lineIdx].x = a;
		m_corners[lineIdx].y = b;

		// Draw the new exact corners
		cv::Point exactPoint;
		exactPoint.x = (int)m_corners[lineIdx].x;
		exactPoint.y = (int)m_corners[lineIdx].y;
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

void SudokuAR::findMarkerCenter() {
	for (int i = 0; i < 4; i++)
	{
		m_markerCenter.x += (int)floorf(m_corners[i].x);
		m_markerCenter.y += (int)floorf(m_corners[i].y);
	}

	m_markerCenter.x /= 4;
	m_markerCenter.y /= 4;
}

void SudokuAR::identifyMarker() {

	// Transform the perspective of the current marker
	perspectiveTransform();

	if (!isBorderBlack()) {
		// If border is NOT black, return and discard the current contour
		return;
	}

	// Compute the ID of the current marker
	computeMarkerId();
}



bool SudokuAR::isBorderBlack() {

	if (m_isFirstMarker) {
		cv::imshow(markerWindow, m_iplMarker);
		m_isFirstMarker = false;
	}
	cv::threshold(m_iplMarker, m_iplMarker, m_markerThresholdSlider,
		markerThresholdSliderMax, CV_THRESH_BINARY);

	unsigned char black = 0;

	int upper_row = 0;
	int lower_row = 5;

	int left_column = 0;
	int right_column = 5;

	int upper_row_pixel_color, lower_row_pixel_color;
	int left_column_pixel_color, right_column_pixel_color;

	// Check if there is a 1-pixel wide border around our marker
	for (int i = 0; i < 6; i++)
	{
		upper_row_pixel_color = m_iplMarker.at<uchar>(upper_row, i);
		lower_row_pixel_color = m_iplMarker.at<uchar>(lower_row, i);

		left_column_pixel_color = m_iplMarker.at<uchar>(i, left_column);
		right_column_pixel_color = m_iplMarker.at<uchar>(i, right_column);

		if ((upper_row_pixel_color   > black) || (lower_row_pixel_color    > black) ||
			(left_column_pixel_color > black) || (right_column_pixel_color > black))
		{
			return false;
		}
	}

	return true;
}

void SudokuAR::computeMarkerId() {

	// Copy the BW values into a 4x4 matrix (we remove the black borders)
	int cP[4][4];

	for (int row = 0; row < 4; row++)
	{
		for (int col = 0; col < 4; col++)
		{
			int tmp = m_iplMarker.at<uchar>(row + 1, col + 1);
			cP[row][col] = (tmp == 0) ? 1 : 0; // if pixel is black, then 1; else 0
		}
	}

	// Save the Marker's ID in all 4 directions
	// We will look for the code encoded in all 4 directions
	// Then, we will keep the smallest value as our code
	int codesInAll4Directions[4] = { 0, 0, 0, 0 };

	for (int row = 0; row < 4; row++)
	{
		for (int col = 0; col < 4; col++)
		{
			// Code when reading the marker along the 0º axis
			codesInAll4Directions[0] <<= 1;
			codesInAll4Directions[0] |= cP[row][col];

			// Code when reading the marker along the 90º axis
			codesInAll4Directions[1] <<= 1;
			codesInAll4Directions[1] |= cP[3 - col][row];

			// Code when reading the marker along the 180º axis
			codesInAll4Directions[2] <<= 1;
			codesInAll4Directions[2] |= cP[3 - row][3 - col];

			// Code when reading the marker along the 270º axis
			codesInAll4Directions[3] <<= 1;
			codesInAll4Directions[3] |= cP[col][3 - row];
		}
	}

	// Check if Marker is all black or all white
	if (codesInAll4Directions[0] == 0 || codesInAll4Directions[0] == 0xffff) {
		return;
	}

	// Now account for symmetry, and keep only the smallest code value as our final code
	m_markerCode = codesInAll4Directions[0];
	m_markerAngle = 0;

	for (int i = 1; i < 4; i++)
	{
		if (codesInAll4Directions[i] < m_markerCode)
		{
			m_markerCode = codesInAll4Directions[i];
			m_markerAngle = i;
		}
	}

	//std::cout << "Found marker: " << std::hex << m_code << std::endl;
	//--std::cout << "Found marker: " << m_markerCode << std::endl;
	//--printf("Found: %04x\n", m_markerCode);

	cv::imshow(markerWindow, m_iplMarker);

	if (m_isFirstMarker) {
		cv::imshow(markerWindow, m_iplMarker);
		m_isFirstMarker = false;
	}
}

void SudokuAR::estimateMarkerPose() {

	// Correct the order of the corners
	if (m_markerAngle != 0) {
		cv::Point2f corrected_corners[4];
		for (int i = 0; i < 4; i++)
			corrected_corners[(i + m_markerAngle) % 4] = m_corners[i];
		for (int i = 0; i < 4; i++)
			m_corners[i] = corrected_corners[i];
	}

	// Transfer screen coords to camera coords
	for (int i = 0; i < 4; i++) {
		//std::cout << m_corners[i].x << " " << m_corners[i].y << std::endl;
		m_corners[i].x -= m_src.cols;
		m_corners[i].y -= m_src.rows;
		//std::cout << m_corners[i].x << " " << m_corners[i].y << std::endl;
	}

	float resultMatrix[16];
	estimateSquarePose(resultMatrix, (cv::Point2f*)m_corners, 0.045);

	// This part is only for printing
	//for (int i = 0; i<4; ++i) {
	//	for (int j = 0; j<4; ++j) {
	//		std::cout << std::setw(6);
	//		std::cout << std::setprecision(4);
	//		std::cout << resultMatrix[4 * i + j] << " ";
	//	}
	//	std::cout << std::endl;
	//}
	//std::cout << std::endl;

	float x, y, z;
	x = resultMatrix[3];
	y = resultMatrix[7];
	z = resultMatrix[11];
	m_distanceToMarkerInCm = (int)(sqrt(x*x + y * y + z * z) * 100); // in cm
																	 //std::cout << "Distance to marker: " << sqrt(x*x + y * y + z * z) << "\n";
																	 //std::cout << "Distance to marker: " << m_distanceToMarkerInCm << std::endl;
}