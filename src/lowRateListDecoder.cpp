#include "../include/lowRateListDecoder.h"
#include "../include/mla_types.h"


MessageInformation LowRateListDecoder::lowRateDecoding(std::vector<double> receivedMessage){
	// trellisInfo is indexed [state][stage]
	std::vector<std::vector<cell>> trellisInfo;
	trellisInfo = constructLowRateTrellis(receivedMessage);

	// start search
	messageInformation output;
	//RBTree detourTree;
	MinHeap detourTree;
	std::vector<std::vector<int>> previousPaths;
	

	// create nodes for each valid ending state with no detours
	// std::cout<< "end path metrics:" <<std::endl;
	for(int i = 0; i < lowrate_numStates; i++){
		DetourObject detour;
		detour.startingState = i;
		detour.pathMetric = trellisInfo[i][lowrate_pathLength - 1].pathMetric;
		detourTree.insert(detour);
	}

	int numPathsSearched = 0;
	int TBPathsSearched = 0;
	while(numPathsSearched < this->listSize){
		DetourObject detour = detourTree.pop();
		std::vector<int> path(lowrate_pathLength);

		int newTracebackStage = lowrate_pathLength - 1;
		double forwardPartialPathMetric = 0;
		int currentState = detour.startingState;

		// if we are taking a detour from a previous path, we skip backwards to the point where we take the
		// detour from the previous path
		if(detour.originalPathIndex != -1){
			forwardPartialPathMetric = detour.forwardPathMetric;
			newTracebackStage = detour.detourStage;

			// while we only need to copy the path from the detour to the end, this simplifies things,
			// and we'll write over the earlier data in any case
			path = previousPaths[detour.originalPathIndex];
			currentState = path[newTracebackStage];

			double suboptimalPathMetric = trellisInfo[currentState][newTracebackStage].suboptimalPathMetric;

			currentState = trellisInfo[currentState][newTracebackStage].suboptimalFatherState;
			newTracebackStage--;
			
			double prevPathMetric = trellisInfo[currentState][newTracebackStage].pathMetric;

			forwardPartialPathMetric += suboptimalPathMetric - prevPathMetric;
			
		}
		path[newTracebackStage] = currentState;

		// actually tracing back
		for(int stage = newTracebackStage; stage > 0; stage--){
			double suboptimalPathMetric = trellisInfo[currentState][stage].suboptimalPathMetric;
			double currPathMetric = trellisInfo[currentState][stage].pathMetric;

			// if there is a detour we add to the detourTree
			if(trellisInfo[currentState][stage].suboptimalFatherState != -1){
				DetourObject localDetour;
				localDetour.detourStage = stage;
				localDetour.originalPathIndex = numPathsSearched;
				localDetour.pathMetric = suboptimalPathMetric + forwardPartialPathMetric;
				localDetour.forwardPathMetric = forwardPartialPathMetric;
				localDetour.startingState = detour.startingState;
				detourTree.insert(localDetour);
			}
			currentState = trellisInfo[currentState][stage].optimalFatherState;
			double prevPathMetric = trellisInfo[currentState][stage - 1].pathMetric;
			forwardPartialPathMetric += currPathMetric - prevPathMetric;
			path[stage - 1] = currentState;
		}
		
		previousPaths.push_back(path);

		std::vector<int> message = pathToMessage(path);
		std::vector<int> codeword = pathToCodeword(path);
		
		// one trellis decoding requires both a tb and crc check
		if(path[0] == path[lowrate_pathLength - 1] && crc_check(message, crcDegree, crc)){
			output.message = message;
			output.codeword = codeword;
			output.path = path;
		 	output.listSize = numPathsSearched + 1;
			output.metric = forwardPartialPathMetric;
			output.TBListSize = TBPathsSearched + 1;
		 	return output;
		}

		numPathsSearched++;
		if(path[0] == path[lowrate_pathLength - 1])
			TBPathsSearched++;
	}
	output.listSizeExceeded = true;
	return output;
}