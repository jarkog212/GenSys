#pragma once
#include <string>

struct GensysParameters
{
	int ValueNoiseOctaves = 1;
	double BlurPixelRadius = 2;
	double Granularity = 0;
	int RiverGenerationIterations = 1;
	float RiverResolution = 0.99;
	int RiverThickness = 1;
	bool RiverAllowNodeMismatch = false;
	bool RiversOnGivenFeatures = false;
	float RiverStrengthFactor = 0;
	int NumberOfTerrainLayers = 1;
	int NumberOfFoliageLayers = 1;
	float FoliageWholeness = 0;
	float MinUnitFoliageHeight = 0;
	std::string User_TerrainOutlineMap = "";
	std::string User_TerrainFeatureMap = "";
	std::string User_RiverOutline = "";
};