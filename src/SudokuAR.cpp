/**
	SudokuAR.cpp
	Purpose:	* Implements all methods necessary to detect a Sodoku grid
				and extract the 81 subimages that define it.
				* Aditionally, calls a ConvNet defined in an external
				python script (implemented by Jose) in order to recognize
				the numbers in the 81 subimages (empty box is marked as 0).
				* Then, makes use of PuzzleSolve.h functions (code by Jose)
				to solve the sudoku.
				* Finally, reproject the solution onto the original image

	@author Pablo Rodriguez Palafox
	@version 1.0 02/07/18
*/

#include "stdafx.h"
#include "SudokuAR.h"
#include "PuzzleSolver.h"

const cv::Scalar SudokuAR::numbersColor = cv::Scalar(0, 100, 255);

const std::string SudokuAR::resultsWindow = "Result";
const std::string SudokuAR::sudokuWindow = "Sudoku";
const std::string SudokuAR::trackbarsWindow = "Trackbars";

const std::string SudokuAR::blockSizeTrackbarName = "block size";
const std::string SudokuAR::constTrackbarName = "C";

const int SudokuAR::blockSizeSliderMax = 1001;
const int SudokuAR::constSliderMax = 100;
const int SudokuAR::markerThresholdSliderMax = 255;

const int SudokuAR::numberOfSides = 4; // Square
const int SudokuAR::nOfIntervals = 9;

const int SudokuAR::MAX_AREA = 30000;
const std::string SudokuAR::minAreaTrackbarName = "min area";
const std::string SudokuAR::maxAreaTrackbarName = "max area";

const int SudokuAR::MIN_NUM_OF_BOXES = 10;

SudokuAR::SudokuAR(double sudokuSize) :
	m_blockSizeSlider(17)
	, m_constSlider(7)
	, m_minArea(5000)
	, m_maxArea(20000)
	, m_sudokuSize(sudokuSize)
	, m_printSolution(false)
	, m_grayFlag(false)
{
	m_playVideo = true;
	m_isFirstStripe = true;
	m_isFirstMarker = true;

	// Initialize the matrix of line parameters
	m_lineParamsMat = cv::Mat(cv::Size(4, 4), CV_32F, m_lineParams);

	////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////// Create windows to display results ////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////

	cv::namedWindow(resultsWindow, CV_WINDOW_AUTOSIZE);
	cv::moveWindow(resultsWindow, 300, 700);

	cv::namedWindow(sudokuWindow, CV_WINDOW_NORMAL);
	cv::resizeWindow(sudokuWindow, 500, 700);
	cv::moveWindow(sudokuWindow, 1200, 600);

	////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////// Trackbars ///////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////

	cv::createTrackbar(blockSizeTrackbarName, sudokuWindow,
		&m_blockSizeSlider, blockSizeSliderMax, onBlockSizeSlider, this);
	cv::createTrackbar(constTrackbarName, sudokuWindow,
		&m_constSlider, constSliderMax);
	cv::createTrackbar(minAreaTrackbarName, sudokuWindow,
		&m_minArea, MAX_AREA);
	cv::createTrackbar(maxAreaTrackbarName, sudokuWindow,
		&m_maxArea, MAX_AREA);

	// Create heap
	//m_memStorage = cvCreateMemStorage();

	// Set the contours pointer to NULL
	//m_contours = NULL;
}

SudokuAR::~SudokuAR()
{
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

bool SudokuAR::processNextFrame(cv::Mat &img_bgr, float resultMatrix[16])
{
	m_src = img_bgr.clone();

	// If the frame is empty, break immediately
	if (m_src.empty())
		return false;

	// 0. Rotate image (and maybe resize)
	//cv::resize(m_src, m_src, cv::Size(960, 540));
	////std::cout << m_src.size << std::endl;
	//cv::rotate(m_src, m_src, cv::ROTATE_90_CLOCKWISE);

	// 0. Make copies of the source image for later use
	m_dst = m_src.clone();

	///////////////////////////////////////////////////////////////////////////////////
	///////////////////// A. Detect sudoku grid ///////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////
	// A.1 Converto to gray scale
	cv::cvtColor(m_src, m_gray, CV_BGR2GRAY); // BGR to GRAY

	// A.2 Apply Gaussian blurring (smoothing)
	//cv::GaussianBlur(m_gray, m_blur, cv::Size(3, 3), 0, 0);

	// A.3 adaptiveThreshold
	cv::adaptiveThreshold(m_gray, m_threshold, 255, cv::ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY, m_blockSizeSlider, m_constSlider);

	// A.4 Bitwise NOT operation
	cv::bitwise_not(m_threshold, m_threshold);

	// A.5 Dilation in order to fill up spaces between lines (not strictly necessary)
	/*cv::Mat kernel = (cv::Mat_<uchar>(3, 3) << 0, 1, 0, 1, 1, 1, 0, 1, 0);
	cv::dilate(m_threshold, m_threshold, kernel);*/

	// A.6. Find contours in our image and find our sudoku grid. We store the corners of 
	//      the sudoku in the atribute ```m_sudokuCorners``
	m_sudokuCorners = findSudoku();

	if (m_sudokuCorners != NULL) {

		// Subpixel accuracy
		processCorners(); // modifies m_exactSudokuCorners

		orderCorners(); // modifies m_sudokuCorners

		cv::circle(m_dst, m_exactSudokuCorners[0], 3, cv::Scalar(0, 255, 0), -1, 8, 0); // green - top-left
		cv::circle(m_dst, m_exactSudokuCorners[1], 3, cv::Scalar(0, 0, 255), -1, 8, 0); // red - bottom-left
		cv::circle(m_dst, m_exactSudokuCorners[2], 3, cv::Scalar(255, 255, 255), -1, 8, 0); // white - bottom-right
		cv::circle(m_dst, m_exactSudokuCorners[3], 3, cv::Scalar(0, 0, 0), -1, 8, 0); // black - top-right	

		// Warping
		bool isPerspectiveOK;	
		cv::Mat projMatInv(cv::Size(3, 3), CV_32FC1);
		isPerspectiveOK = perspectiveTransform(m_exactSudokuCorners, projMatInv);

		if (!isPerspectiveOK)
		{
			//std::cout << "Until next time!" << std::endl;
			return true;
		}

		m_saveSubimages = false;
		key = (char)cv::waitKey(50);
		if (key == 's' || key == 'S') {
			std::cout << "CIAO PESCAO" << std::endl;
			m_saveSubimages = true;
		}

		// Extract the 81 subimages of the sudoku
		extractSubimagesAndSaveToFolder(m_saveSubimages);

		if (m_saveSubimages) {

			// Solve puzzle
			bool isSolved = solve();
			if (isSolved)
			{
				std::cout << "\nDifference between Solved and Unsolved:" << std::endl; PuzzleSolver::print1D(m_differenceRow); //std::cout << "\n\n";
				m_printSolution = true;
			}
			m_saveSubimages = false;
		}

		if (m_printSolution) {
			cv::cvtColor(m_sudoku, m_sudoku, CV_GRAY2BGR);
			drawSolution();
			//cv::waitKey();
			// Reproject solution (Unwarping)
			reprojectSolution(projMatInv, img_bgr);
		}

		// Estimate Marker Pose
		estimateSudokuPose(resultMatrix);
		cv::imshow(sudokuWindow, m_sudoku);
	}

	//////////////////////////////////////////////////

	// 100. Display the resulting frame
	cv::imshow(resultsWindow, m_dst);

	// Release Mat
	m_src.release();
	m_dst.release();
	m_sudoku.release();

	// Read input key from user
	key = (char)cv::waitKey(50);
	if (key == 'q' || key == 'Q') {
		std::cout << "ciao" << std::endl;
		return false;
	}
		
	return true;
}

void SudokuAR::orderCorners() {
	cv::Point2f tmpCorners[4];
	tmpCorners[0] = m_exactSudokuCorners[0];
	tmpCorners[1] = m_exactSudokuCorners[1];
	tmpCorners[2] = m_exactSudokuCorners[2];
	tmpCorners[3] = m_exactSudokuCorners[3];

	if (tmpCorners[1].x > tmpCorners[3].x)
	{
		////std::cout << "Modifying order!" << std::endl;
		// Green point is on the upper-right corner instead of on the upper-left corner
		m_exactSudokuCorners[0] = tmpCorners[3]; // 3
		m_exactSudokuCorners[1] = tmpCorners[0]; // 0
		m_exactSudokuCorners[2] = tmpCorners[1]; // 1
		m_exactSudokuCorners[3] = tmpCorners[2]; // 2
	}
}

cv::Point* SudokuAR::findSudoku()
{
	///////////////////////////////////////////////////
	// B.1 From the linear segments image, find the contours(cv::findContours)
	cv::findContours(m_threshold, m_contours, m_hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE);

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
			for (int i = 0; i < 81; i++)
			{
				if (m_hierarchy[prevChild][0] >= 0)
				{
					nextChild = m_hierarchy[prevChild][0];
					cv::approxPolyDP(m_contours[nextChild], m_approxChild, cv::arcLength(m_contours[nextChild], true) * 0.02, true);

					//////std::cout << "Size " << m_approxChild.size() << std::endl;

					if (m_approxChild.size() < 3 && m_approxChild.size() > 6) continue;
					m_boundingBox = cv::boundingRect(m_contours[nextChild]);

					m_square = (cv::Point*) &m_approxChild[0];

					cv::polylines(m_dst, &m_square, &numberOfSides, 1, true, cv::Scalar(0, 255, 0), 2, 8, 0);
					iArea = m_boundingBox.area();
					sArea = std::to_string(iArea);
					if (iArea < 100) continue;

					prevChild = nextChild;
					numChildren++;
				}
			}

			//std::cout << numChildren << std::endl;
			if (numChildren < MIN_NUM_OF_BOXES) continue;

			cv::Point* points(&m_approx[0]);
			cv::polylines(m_dst, &points, &numberOfSides, 1, true, cv::Scalar(0, 0, 255), 2, 8, 0);
			return points;

		} // Contours	
	}
	return NULL;
}

bool SudokuAR::perspectiveTransform(cv::Point2f* corners, cv::Mat& projMatInv) {

	m_maxHeight = (int)sqrt((corners[0].x - corners[1].x)*(corners[0].x - corners[1].x) + (corners[0].y - corners[1].y)*(corners[0].y - corners[1].y));
	m_maxWidth = (int)sqrt((corners[2].x - corners[1].x)*(corners[2].x - corners[1].x) + (corners[2].y - corners[1].y)*(corners[2].y - corners[1].y));

	if (m_maxWidth < 0 || m_maxHeight < 0)
	{
		return false;
	}

	cv::Point2f sourceCorners[4];
	cv::Point2f targetCorners[4];

	sourceCorners[0] = corners[0];         targetCorners[0] = cv::Point2f(0, 0);
	sourceCorners[1] = corners[1];		   targetCorners[1] = cv::Point2f(0, m_maxHeight - 1);
	sourceCorners[2] = corners[2];         targetCorners[2] = cv::Point2f(m_maxWidth - 1, m_maxHeight - 1);
	sourceCorners[3] = corners[3];         targetCorners[3] = cv::Point2f(m_maxWidth - 1, 0);

	// Create and compute matrix of perspective transform
	cv::Mat projMat(cv::Size(3, 3), CV_32FC1);
	projMat = cv::getPerspectiveTransform(sourceCorners, targetCorners);

	// Store inverse transform
	projMatInv = projMat.inv();

	// Initialize image for the sudoku
	m_sudoku = cv::Mat(cv::Size(m_maxWidth, m_maxHeight), CV_8UC1);

	// Change the perspective in the marker image using the previously calculated matrix
	if (m_grayFlag)
	{
		cv::warpPerspective(m_gray, m_sudoku, projMat, cv::Size(m_maxWidth, m_maxHeight));
	}
	else
	{
		cv::warpPerspective(m_threshold, m_sudoku, projMat, cv::Size(m_maxWidth, m_maxHeight));
	}
	
	return true;
}


void SudokuAR::reprojectSolution(const cv::Mat& projMatInv, cv::Mat& img_bgr) {

	// Inverse the transform
	//cv::Mat sol(m_dst.cols, m_dst.rows, CV_64FC4);
	cv::Mat sol(m_dst.cols, m_dst.rows, CV_8UC4);

	cv::resize(m_sudoku, m_sudoku, cv::Size(m_maxWidth, m_maxHeight));
	cv::warpPerspective(m_sudoku, sol, projMatInv, cv::Size(m_dst.cols, m_dst.rows));

	if (sol.channels() == 3) {

		cv::cvtColor(sol, sol, CV_BGR2BGRA);
		cv::cvtColor(m_src, m_src, CV_BGR2BGRA);

		 // Now go over all pixels of the solution image and set those with
		 // black pixels to transparent (alpha value of 0)
		for (int v = 0; v < sol.rows; v++) {
			for (int u = 0; u < sol.cols; u++) {
				cv::Vec4b & pixel = sol.at<cv::Vec4b>(v, u);

				if ( !(pixel[0] == numbersColor[0] && pixel[1] == numbersColor[1] == pixel[2] != numbersColor[2]) )
				{
					pixel[0] = 0;
					pixel[1] = 0;
					pixel[2] = 0;
				}
			}
		}

		cv::add(sol, m_src, img_bgr);

		// Finally, remove the last channel from this img_bgr
		// Otherwise, we won't be able to use the image as a background image
		// for more AR stuff
		std::vector<cv::Mat> matChannels3;
		cv::split(img_bgr, matChannels3);
		matChannels3.pop_back(); // remove channel
		cv::merge(matChannels3, img_bgr);
	}
}


//void SudokuAR::reprojectSolution(const cv::Mat& projMatInv, cv::Mat& img_bgr) {
//
//	// Inverse the transform
//	//cv::Mat sol(m_dst.cols, m_dst.rows, CV_64FC4);
//	cv::Mat sol(m_dst.cols, m_dst.rows, CV_8UC4);
//
//	cv::resize(m_sudoku, m_sudoku, cv::Size(m_maxWidth, m_maxHeight));
//	cv::warpPerspective(m_sudoku, sol, projMatInv, cv::Size(m_dst.cols, m_dst.rows));
//
//	if (sol.channels() == 3) {
//		// Get the current three channels 
//		std::vector<cv::Mat> matChannels;
//		cv::split(sol, matChannels);
//
//		// add alpha channel (4th channel - transparency)
//		cv::Mat alpha = matChannels.at(0) + matChannels.at(1) + matChannels.at(2);
//
//		cv::imshow("aplha", alpha);
//
//		//alpha = 0; // Initially, set the channel of all pixels to opaque (255)
//
//		cv::imshow("asdfasdf", alpha);
//
//		matChannels.push_back(alpha);
//		cv::merge(matChannels, sol);
//
//		cv::imshow("sol", sol);
//
//		// Now go over all pixels of the solution image and set those with
//		// black pixels to transparent (alpha value of 0)
//		for (int v = 0; v < sol.rows; v++) {
//			for (int u = 0; u < sol.cols; u++) {
//				cv::Vec4b & pixel = sol.at<cv::Vec4b>(v, u);
//
//				//if (pixel[0] != numbersColor[0] && pixel[1] != numbersColor[1] && pixel[2] != numbersColor[2])
//				//{
//				//	//std::cout << static_cast<unsigned>(pixel[0]) << " " << static_cast<unsigned>(pixel[1]) << " " << static_cast<unsigned>(pixel[2]) << " " << std::endl;
//				//	// set alpha to zero:
//				//	pixel[3] = 0;
//				//}
//			}
//		}
//
//		// Get the 3 channels of the destination image
//		std::vector<cv::Mat> matChannels2;
//		cv::split(m_src, matChannels2);
//
//		// Create alpha channel for the destination image (on which we will 
//		// overlay the solution of the sudoku)
//		cv::Mat alpha2 = matChannels2.at(0) + matChannels2.at(1) + matChannels2.at(2);
//		alpha2 = 0;
//		matChannels2.push_back(alpha2);
//		cv::merge(matChannels2, m_src);
//
//		// Add the destination image and the sudoku solution and save them 
//		// into the image that came from main.cpp, on which we will do more
//		// AR stuff afterwards
//
//		std::cout << sol.channels() << std::endl;
//		std::cout << m_src.channels() << std::endl;
//		std::cout << img_bgr.channels() << std::endl;
//
//		cv::add(sol, m_src, img_bgr);
//
//		cv::imshow("ehaa", img_bgr);
//
//		// Finally, remove the last channel from this img_bgr
//		// Otherwise, we won't be able to use the image as a background image
//		// for more AR stuff
//		std::vector<cv::Mat> matChannels3;
//		cv::split(img_bgr, matChannels3);
//		matChannels3.pop_back(); // remove channel
//		cv::merge(matChannels3, img_bgr);
//	}
//}

void SudokuAR::extractSubimagesAndSaveToFolder(bool saveSubimages) {

	cv::Mat sudokuResized;
	cv::resize(m_sudoku, sudokuResized, cv::Size(360, 360));

	int width = sudokuResized.cols;
	int height = sudokuResized.rows;

	int subWidth = (width / 9); // 40 due to 360
	int subHeight = (height / 9); // 40 due to 360s

	cv::Size subSize(subWidth, subHeight);

	int idx = 0;
	for (unsigned v = 0; v < height; v += subHeight)
	{
		for (unsigned u = 0; u < width; u += subWidth)
		{
			// Get subimage
			cv::Mat subimage = cv::Mat(sudokuResized, cv::Rect(u, v, subWidth, subHeight)).clone();

			// Crop the image in order to only have the number, no borders at all! --> CNNs don't like that
			/* Set Region of Interest */
			int offset_x = floor(0.05 * subWidth);
			int offset_y = floor(0.05 * subHeight);

			cv::Rect roi;
			roi.x = offset_x;
			roi.y = offset_y;
			roi.width = subimage.size().width - (offset_x * 2);
			roi.height = subimage.size().height - (offset_y * 2);

			/* COARSE-Crop the original image to the defined ROI */
			subimage = subimage(roi);

			/* FINE-Crop the original image to the defined ROI */
			if (m_grayFlag)
				subimage = fineCropGray(subimage);
			else
				subimage = fineCropBinary(subimage);

			// Resize the image to 28 x 28
			cv::resize(subimage, subimage, cv::Size(28, 28));

			// Push it into the vector of subimages
			m_subimages[idx] = subimage;

			// Now we need to check whether there's a number or not in the image
			cv::Point min_loc, max_loc;
			double min, max;
			cv::minMaxLoc(subimage, &min, &max, &min_loc, &max_loc);

			if (saveSubimages) {
				std::string subimageName = "./scripts/gray_imgs/" + std::to_string(idx) + ".png";
				cv::imwrite(subimageName, subimage);
			}

			//cv::waitKey();

			idx++;
		}
	}

}

bool SudokuAR::solve() {

	// Predict the numbers
	system("C:/Anaconda/python.exe ./scripts/use_cnn.py");

	// Read the file where the predictions have been saved
	std::ifstream ifile("./scripts/predictions/results.txt");
	while (!ifile.is_open()) {
		ifile.open("./scripts/predictions/results.txt");
	}

	std::vector<int> sudokuVector;
	int number;
	while (ifile >> number)
		sudokuVector.push_back(number);

	int sudokuMatrix[N][N];
	for (unsigned row = 0; row < N; row++)
	{
		for (unsigned col = 0; col < N; col++)
		{
			int idx = row * N + col;
			sudokuMatrix[row][col] = sudokuVector[idx];
		}
	}
	
	PuzzleSolver::printGrid(sudokuMatrix);

	std::cout << "hello" << std::endl;

	// Solve it
	return PuzzleSolver::solve_puzzle(sudokuMatrix, m_differenceRow);
}

void SudokuAR::drawSolution() {
	for (unsigned row = 0; row < N; row++)
	{
		for (unsigned col = 0; col < N; col++)
		{
			unsigned idx = N * row + col;
			if (m_differenceRow[idx] != 0)
				drawNumber(m_differenceRow[idx], row, col);
		}
	}
}

void SudokuAR::drawNumber(int number, unsigned row, unsigned col) {

	int width = m_sudoku.cols;
	int height = m_sudoku.rows;

	int subWidth = (width / 9); // 40 due to 360
	int subHeight = (height / 9); // 40 due to 360

	cv::Point org((col + 0.3) * subWidth, (row + 1 - 0.25) * subHeight);
	cv::putText(m_sudoku, std::to_string(number), org, cv::FONT_ITALIC, 0.7, numbersColor, 2);
}

cv::Mat SudokuAR::fineCropGray(const cv::Mat& img) {

	int width = img.cols;
	int height = img.rows;

	cv::Rect roi;
	unsigned char THRESHOLD = 100;

	unsigned int u = 0, v = 0;

	/* Top border */
	u = width / 2;
	v = 0;
	while ((img.at<uchar>(v, u) < THRESHOLD) &&  (v < (int)(height / 5))) 
	{ 
		v++; 
	}
	v++;
	roi.y = v;
	////std::cout << "top index " << v << std::endl;

	/* Left border */
	u = 0;
	v = (int)(height / 2);
	while ((img.at<uchar>(v, u) < THRESHOLD) && (u < (int)(width / 5)))
	{ 
		u++; 
	}
	u++;
	roi.x = u;
	////std::cout << "left index " << u << std::endl;

	/* Bottom side */
	u = (int)(width / 2);
	v = height - 1;
	while ((img.at<uchar>(v, u) < THRESHOLD) && (v > (int)(4 * height / 5)))
	{ 
		v--; 
	}
	v--;
	roi.height = v - roi.y;
	////std::cout << "bottom index " << v << std::endl;

	/* Right side */
	u = width - 1;
	v = (int)(height / 2);
	while ((img.at<uchar>(v, u) < THRESHOLD) && (u > (int)(4 * width / 5)))
	{ 
		u--; 
	}
	u--;
	roi.width = u - roi.x;
	////std::cout << "right index " << u << std::endl;

	cv::Mat cropped;
	cropped = img(roi);

	return cropped;
}

cv::Mat SudokuAR::fineCropBinary(const cv::Mat& img) {

	int width = img.cols;
	int height = img.rows;

	cv::Rect roi;
	unsigned char THRESHOLD = 180;

	unsigned int u = 0, v = 0;

	/* Top border */
	u = width / 2;
	v = 0;
	while ((img.at<uchar>(v, u) > THRESHOLD) && (v < (int)(height / 5)))
	{
		//std::cout << "TOP " << static_cast<unsigned>(img.at<uchar>(v, u)) << std::endl;
		v++;
	}
	v += 1;
	roi.y = v;
	////std::cout << "top index " << v << std::endl;

	/* Left border */
	u = 0;
	v = (int)(height / 2);
	while ((img.at<uchar>(v, u) > THRESHOLD) && (u < (int)(width / 5)))
	{
		//std::cout << "LEFT " << static_cast<unsigned>(img.at<uchar>(v, u)) << std::endl;
		u++;
	}
	u += 1;
	roi.x = u;
	////std::cout << "left index " << u << std::endl;

	/* Bottom side */
	u = (int)(width / 2);
	v = height - 1;
	while ((img.at<uchar>(v, u) > THRESHOLD) && (v > (int)(4 * height / 5)))
	{
		//std::cout << "BOTTOM " << static_cast<unsigned>(img.at<uchar>(v, u)) << std::endl;
		v--;
	}
	v -= 1;
	roi.height = v - roi.y;
	////std::cout << "bottom index " << v << std::endl;

	/* Right side */
	u = width - 1;
	v = (int)(height / 2);
	while ((img.at<uchar>(v, u) > THRESHOLD) && (u > (int)(4 * width / 5)))
	{
		//std::cout << "RIGHT " << static_cast<unsigned>(img.at<uchar>(v, u)) << std::endl;
		u--;
	}
	u -= 1;
	roi.width = u - roi.x;
	////std::cout << "right index " << u << std::endl;

	cv::Mat cropped;
	cropped = img(roi);

	return cropped;
}

void SudokuAR::estimateSudokuPose(float resultMatrix[16]) { //CHANGED
	// Transfer screen coords to camera coords
	for (int i = 0; i < 4; i++) {
		m_exactSudokuCorners[i].x -= m_src.cols*0.5; //here you have to use your own camera resolution (x) * 0.5
		m_exactSudokuCorners[i].y = -m_exactSudokuCorners[i].y + m_src.rows*0.5; //here you have to use your own camera resolution (y) * 0.5
	}

	cv::Point2f tmp[4] = { m_exactSudokuCorners[0], m_exactSudokuCorners[3], m_exactSudokuCorners[2], m_exactSudokuCorners[1] };

	//float resultMatrix[16];
	estimateSquarePose(resultMatrix, (cv::Point2f*)tmp, m_sudokuSize);

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
	m_distanceToSudokuInCm = (int)(sqrt(x * x + y * y + z * z) * 100); // we print it in cm
	//std::cout << "Distance to marker: " << m_distanceToSudokuInCm << " cm" << std::endl;
}

void SudokuAR::processCorners()
{
	// Go through the 4 corners of the found quadrilateral
	for (int c = 0; c < numberOfSides; c++)
	{
		// Draw circles on the corners of the rectangle
		cv::circle(m_dst, m_sudokuCorners[c], 1, cv::Scalar(0, 255, 0), -1, 8, 0);

		// For every corner
		// get the measure of the sub intervals between the current corner and the one next to it
		double dx = (double)(m_sudokuCorners[(c + 1) % 4].x - m_sudokuCorners[c].x) / float(nOfIntervals);
		double dy = (double)(m_sudokuCorners[(c + 1) % 4].y - m_sudokuCorners[c].y) / float(nOfIntervals);

		// For every side of the quadrilateral, compute a Stripe object (a cvMat, actually)
		// which we will position at the location of every delimiter (there are 6) between
		// the current corner and the one next to it
		computeStripe(dx, dy);

		// Array for accurate delimiters
		cv::Point2f delimiters[nOfIntervals - 1];

		// Go through every delimiter
		for (int delim = 1; delim < nOfIntervals; ++delim) // [1, 2, 3, 4, 5, 6, ..., nOfIntervals-1]
		{
			// Compute the distance between the current corner and the j-th delimiter
			double px = (double)m_sudokuCorners[c].x + (double)delim*dx;
			double py = (double)m_sudokuCorners[c].y + (double)delim*dy;

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
		cv::Mat delimiters_mat(cv::Size(1, nOfIntervals - 1), CV_32FC2, delimiters);

		// And now we fit a line through them
		cv::fitLine(delimiters_mat, m_lineParamsMat.col(c), CV_DIST_L2, 0, 0.01, 0.01);
		// cvFitLine stores the calculated line in lineParams in the following way:
		//   vec1.x,   vec2.x,   vec3.x,   vec4.x,
		//   vec1.y,   vec2.y,   vec3.y,   vec4.y,
		// point1.x, point2.x, point3.x, point4.y
		// point1.y, point2.y, point3.x, point4.y

		// We can draw the line by computing two points that reside on it
		// First, we grab the DIRECTION VECTOR and the ORIGIN POINT of the line
		cv::Point2f dir;
		dir.x = m_lineParams[c];
		dir.y = m_lineParams[4 + c];

		cv::Point p0;
		p0.x = (int)m_lineParams[8 + c];
		p0.y = (int)m_lineParams[12 + c];

		// Now we can get two points by travelling from p0 down the directions vector
		float lineLength = 50.0;
		cv::Point q1(p0.x - (int)(lineLength * dir.x), p0.y - (int)(lineLength * dir.y));
		cv::Point q2(p0.x + (int)(lineLength * dir.x), p0.y + (int)(lineLength * dir.y));

		// And finally we can draw a line that represents the current EDGE
		cv::line(m_dst, q1, q2, CV_RGB(0, 255, 0), 1, 8, 0);

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
			////std::cout << "Lines are parallel!" << std::endl;
			// Skip this iteration
			continue;
		}

		a /= c;
		b /= c;
		//////////////////////////////////////////////////////////////////////////////////

		// Save the exact corner for the intersection of lines 'lineIdx' and 'nextLineIdx'
		m_exactSudokuCorners[lineIdx].x = a;
		m_exactSudokuCorners[lineIdx].y = b;

		// Draw the new exact corners
		cv::Point exactPoint;
		exactPoint.x = (int)m_exactSudokuCorners[lineIdx].x;
		exactPoint.y = (int)m_exactSudokuCorners[lineIdx].y;
		cv::circle(m_dst, exactPoint, 5, CV_RGB(255, 0, 0), -1);
	}
}

/**
* Defines a stripe (actually, a cvMat) object, which we will then position
* at the location of every delimiter between the curret corner and the one next to it
*/
void SudokuAR::computeStripe(double dx, double dy) {

	double diffLength = sqrt(dx * dx + dy * dy);

	// -- Stripe length (in pixels) -- //
	// We arbitrarily set the length of the stripe to 0.8 times the length of an interval
	m_stripe.length = (int)(0.8 * diffLength);
	if (m_stripe.length < 5)
		m_stripe.length = 5;

	// Make stripe's length odd (ex1: 0101 | 0001 = 0101 , ex2 = 0100 | 0001 = 0101)
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
			cv::circle(m_dst, p2, 1, cv::Scalar(255, 255, 255), -1);

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

	////std::cout << static_cast<unsigned>(i[0]) << " " << static_cast<unsigned>(i[1]) << std::endl;

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

	//// Show a window with the Stripe
	//if (m_isFirstStripe)
	//{
	//	cv::Mat iplTmp;
	//	cv::resize(m_iplStripe, iplTmp, cv::Size(100, 300));
	//	cv::imshow("we", iplTmp);
	//	m_isFirstStripe = false;
	//}

	return edgeCenter;
}