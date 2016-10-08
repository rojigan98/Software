#include "stdafx.h"
#include "Identifier.h"


Identifier::Identifier(const std::string &imagePath, const std::string &gpsLog, const std::string &outputFolder, ::string *results)
	: results(results)
{
	params.imagePath = imagePath;
	params.gpsLog = gpsLog;
	params.outputFolder = outputFolder;

	// Get the name of the file from the file path
	int indexOfSlash = imagePath.find_last_of("\\");
	if (indexOfSlash == std::string::npos) indexOfSlash = imagePath.find_last_of("/");
	std::string name = imagePath.substr(indexOfSlash + 1);

	params.imageName = name;

	// Make sure the output folder ends in a slash of some sort
	char lastChar = params.outputFolder[params.outputFolder.size() - 1];
	if (lastChar != '\\' && lastChar != '/')
	{
		bool forwardSlash = params.outputFolder.find('/') != std::string::npos;
		if (forwardSlash)
		{
			params.outputFolder.push_back('/');
		}
		else
		{
			params.outputFolder.push_back('\\');
		}
	}
}


Identifier::~Identifier()
{
}


void Identifier::analyze()
{
	// Read from gps log
	readGPSLog();

	// Get image
	cv::Mat image = imread(params.imagePath, CV_LOAD_IMAGE_COLOR);   // Read the file;
	saveCompressedImage(image);

	// Get image info
	int width, height, channels;
	width = image.cols;
	height = image.rows;
	channels = image.channels();

	writeAnalysisParameters(width, height);

	// Get hsv
	cv::Mat hsvImage;
	cv::cvtColor(image, hsvImage, cv::COLOR_RGB2HSV);

	// Apply blur
	cv::blur(hsvImage, hsvImage, cv::Size(6, 6));

	// Create MSER
	cv::Ptr<cv::MSER> mser = cv::MSER::create(5,
		params.minArea / (double)2738 * image.rows,
		params.maxArea / (double)2738 * image.rows,
		0.099, 0.65, 200, 1.01, 0.003, 5);

	// Split hsv channels
	cv::Mat hsv[3];
	cv::split(hsvImage, hsv);

	// Run MSER on each channel
	std::vector<cv::Rect> mserResults;
	for (int i = 0; i < 3; ++i)
	{
		std::vector<std::vector<cv::Point>> msers;
		std::vector<cv::Rect> bboxes;

		mser->detectRegions(hsv[i], msers, bboxes);

		mserResults.insert(mserResults.end(), bboxes.begin(), bboxes.end());
	}

	// Process mserResults
	std::vector<CropResult> cropResults;
	for (int i = 0; i < mserResults.size(); ++i)
	{
		CropResult cropResult;

		double maxDim = mserResults[i].width > mserResults[i].height ?
			mserResults[i].width : mserResults[i].height;

		// Get cropped region
		double centerX = mserResults[i].x + mserResults[i].width / 2;
		double centerY = mserResults[i].y + mserResults[i].height / 2;
		double x1 = centerX - maxDim / 2 - params.cropPadding;
		double y1 = centerY - maxDim / 2 - params.cropPadding;
		double x2 = centerX + maxDim / 2 + params.cropPadding;
		double y2 = centerY + maxDim / 2 + params.cropPadding;
		if (x1 < 0) x1 = 0;
		if (y1 < 0) y1 = 0;
		if (x2 < 0) x2 = 0;
		if (y2 < 0) y2 = 0;
		if (x1 >= image.cols) x1 = image.cols - 1;
		if (y1 >= image.rows) y1 = image.rows - 1;
		if (x2 >= image.cols) x2 = image.cols - 1;
		if (y2 >= image.rows) y2 = image.rows - 1;
		cv::Rect roi(x1, y1, x2 - x1, y2 - y1);

		cropResult.x = centerX;
		cropResult.y = centerY;
		cropResult.size = roi.width > roi.height ? roi.width : roi.height;

		// No area of 0 please
		if (roi.area() == 0) continue;

		// Do crop
		cv::Mat crop = image(roi);

		// Write image
		// Constructor ensures that params.outputFolder ends in a slash
		cropResult.imageName = getCropName(params.imageName, i);
		cv::imwrite(params.outputFolder + cropResult.imageName, crop);

		cropResults.push_back(cropResult);
	}

	// Write results to output file
	writeCropResults(cropResults);

	// Draw the mserResults
	for (int i = 0; i < mserResults.size(); ++i)
	{
		cv::rectangle(image, mserResults[i], CV_RGB(0, 255, 0));
	}

	// Save file
	std::string imageNameNoExtension = params.imageName.substr(0, params.imageName.find_last_of('.'));
	std::ofstream out(params.outputFolder + imageNameNoExtension + " results.ini");
	out << *results;
	out.close();

	// Show result
	//namedWindow("Debug", WINDOW_AUTOSIZE);
	//imshow("Debug", image);
	//waitKey(0);
}

void Identifier::readGPSLog()
{
	// Parameters to fill
	double latitude, longitude, altitude, heading;
	std::string latitudeString, longitudeString, altitudeString, headingString, headingEnglishString;
	std::string date;

	results->append("[Position]\n");
	
	std::string line;
	std::ifstream file(params.gpsLog);
	
	bool firstLine = true;

	if (file.is_open())
	{
		while (getline(file, line))
		{
			// Get past the header
			if (line.find("Latitude") != std::string::npos)
			{
				firstLine = false;
			}
			if (firstLine)
			{
				continue;
			}

			// Split string
			std::vector<std::string> splitLine;
			std::stringstream ss(line);
			std::string token;
			while (std::getline(ss, token, ','))
			{
				splitLine.push_back(token);
			}

			// There are 5 items
			// Image name, latitude, longitude, altitude, heading
			if (splitLine.size() >= 5 && splitLine[0] == params.imageName)
			{
				latitude = std::stod(splitLine[1]);
				latitudeString = splitLine[1];

				longitude = std::stod(splitLine[2]);
				longitudeString = splitLine[2];

				altitude = std::stod(splitLine[3]);
				altitudeString = splitLine[3];

				heading = std::stod(splitLine[4]);
				headingString = splitLine[4];
				headingEnglishString = headingToEnglish(heading);

				if (splitLine.size() >= 6)
				{
					date = splitLine[5];
				}
			}
		}

		file.close();
	}

	results->append("heading=" + headingEnglishString + "\n");
	results->append("headingDegrees=" + headingString + "\n");
	results->append("latitude=" + latitudeString + "\n");
	results->append("longitude=" + longitudeString + "\n");
	results->append("altitude=" + altitudeString + "\n");
}

std::string Identifier::headingToEnglish(double headingDegrees)
{
	if (headingDegrees >= 45 && headingDegrees < 135)
		return "East";
	if (headingDegrees >= 135 && headingDegrees < 225)
		return "South";
	if (headingDegrees >= 225 && headingDegrees < 315)
		return "West";
	if (headingDegrees < 45 || headingDegrees >= 315)
		return "North";

	return "";
}

std::string Identifier::getCropName(std::string imageName, int index)
{
	imageName = imageName.substr(0, imageName.find_last_of('.'));
	return imageName + "roi" + std::to_string(index) + ".jpg";
}

void Identifier::writeCropResults(const std::vector<CropResult> &cropResults)
{
	results->append("\n[Crop Info]\n");
	results->append("Number of Crops=" + std::to_string(cropResults.size()) + "\n");
	for (int i = 0; i < cropResults.size(); ++i)
	{
		results->append("\n[Crop " + std::to_string(i + 1) + "]\n");
		results->append("Image Name=" + cropResults[i].imageName + "\n");
		results->append("X=" + std::to_string(cropResults[i].x) + "\n");
		results->append("Y=" + std::to_string(cropResults[i].y) + "\n");
		results->append("Size=" + std::to_string(cropResults[i].size) + "\n");
	}
}

void Identifier::writeAnalysisParameters(int width, int height)
{
	std::string tempGPSLog = params.gpsLog;
	std::replace(tempGPSLog.begin(), tempGPSLog.end(), '\\', '/');

	std::string tempImagePath = params.outputFolder + params.imageName;
	std::replace(tempImagePath.begin(), tempImagePath.end(), '\\', '/');

	results->append("\n[Analysis Parameters]\n");
	results->append("Width=" + std::to_string(width) + "\n");
	results->append("Height=" + std::to_string(height) + "\n");
	results->append("GPS_LOG=" + tempGPSLog + "\n");
	results->append("IMAGE=" + tempImagePath + "\n");
}

void Identifier::saveCompressedImage(const Mat &image)
{
	Mat resized;
	cv::resize(image, resized, cv::Size(700, (double)700 / image.cols*image.rows));

	cv::imwrite(params.outputFolder + params.imageName, resized);
}