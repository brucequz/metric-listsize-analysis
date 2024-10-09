#ifndef LOWRATELISTDECODER_H
#define LOWRATELISTDECODER_H

#include "feedForwardTrellis.h"
#include "minHeap.h"
#include "mla_types.h"

class LowRateListDecoder{
public:
	LowRateListDecoder(FeedForwardTrellis FT, int listSize, int crcDegree, int crc);
	LowRateListDecoder(FeedForwardTrellis FT, int listSize, int crcDegree, int crc, std::vector<std::vector<int>> neighboring_cwds, std::vector<std::vector<int>> neighboring_msgs, std::vector<std::vector<int>> path_ie_state);
	MessageInformation lowRateDecoding(std::vector<double> receivedMessage, std::vector<int> punctured_indices);


private:
	int numForwardPaths;
	int listSize;
	int crcDegree;
	int crc;
	int n;

	std::vector<std::vector<int>> lowrate_nextStates;
	std::vector<std::vector<int>> lowrate_outputs;
	std::vector<std::vector<int>> neighboring_cwds; // ${listSize} x 516 matrix
	std::vector<std::vector<int>> neighboring_msgs;  // ${listSize} x 43 matrix
	std::vector<std::vector<int>> path_ie_state;
	int lowrate_numStates;
	int lowrate_symbolLength;
	int lowrate_pathLength;

	struct cell {
		int optimalFatherState = -1;
		int suboptimalFatherState = -1;
		double pathMetric = INT_MAX;
		double suboptimalPathMetric = INT_MAX;
		bool init = false;
	};

  std::vector<int> pathToMessage(std::vector<int>); 
  std::vector<int> pathToCodeword(std::vector<int>); 
	std::vector<std::vector<cell>> constructLowRateTrellis(std::vector<double> receivedMessage);
  std::vector<std::vector<cell>> constructLowRateTrellis_Punctured(std::vector<double> receivedMessage, std::vector<int> punctured_indices);
	std::vector<std::vector<std::vector<cell>>> constructLowRateMultiTrellis(std::vector<double> receivedMessage);
	std::vector<std::vector<cell>> constructMinimumLikelihoodLowRateTrellis(std::vector<double> receivedMessage);
};


#endif