#pragma once

#include "stdafx.h"

class Identifier
{
public:
	Identifier(const std::string &imagePath, const std::string &gpsLog, const std::string &outputFolder, std::string *results);
	virtual ~Identifier();

	struct Params
	{
		Params()
		{
			sizeOfROI = 300;
			minPointsInCluster = 5;
			maxArea = 300000;
			minArea = 2650;
			cropPadding = 3;
		}

		std::string imagePath;
		std::string imageName;
		std::string gpsLog;
		std::string outputFolder; 
		unsigned sizeOfROI;
		unsigned minPointsInCluster;
		unsigned maxArea;
		unsigned minArea;
		unsigned cropPadding;
	};

	struct CropResult
	{
		std::string imageName;
		int x;
		int y;
		int size;
	};

	void analyze();

private:
	Params params;
	std::string *results;

	void readGPSLog();
	std::string headingToEnglish(double headingDegrees);
	std::string getCropName(std::string imageName, int index);
	void writeCropResults(const std::vector<CropResult> &cropResults);
	void writeAnalysisParameters(int width, int height);
	void saveCompressedImage(const Mat &image);
	double getEdgeScore(const Mat &image);     // score increases with quantity of sharp edges
};

