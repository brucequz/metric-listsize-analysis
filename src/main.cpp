#include <iostream>
#include <fstream>
#include <vector>

#include "../include/mla_consts.h"
#include "../include/mla_types.h"
#include "../include/mla_namespace.h"
#include "../include/feedForwardTrellis.h"
#include "../include/lowRateListDecoder.h"

void ISTC_sim(CodeInformation code);
std::vector<int> generateRandomCRCMessage(CodeInformation code);
std::vector<int> generateTransmittedMessage(std::vector<int> originalMessage, FeedForwardTrellis encodingTrellis, double snr, std::vector<int> puncturedIndices, bool noiseless);
std::vector<double> addAWNGNoise(std::vector<int> transmittedMessage, std::vector<int> puncturedIndices, double snr, bool noiseless);

int main() {

  std::cout << "K: " << K << std::endl;
  std::cout << "N: " << N << std::endl;
  std::cout << "V: " << V << std::endl;
  std::cout << "M: " << M << std::endl;
    
  CodeInformation code;
  code.k = K;         // numerator of the rate
  code.n = N;         // denominator of the rate
  code.v = V;         // number of memory elements
  code.crcDeg = M+1;  // m+1, degree of CRC, # bits of CRC polynomial
  code.crc = CRC;     // CRC polynomial
  code.numInfoBits = NUM_INFO_BITS; // number of information bits
  code.numerators = {POLY1, POLY2};

  ISTC_sim(code);

  return 0;
}

void ISTC_sim(CodeInformation code){
	
	/* - Output files setup - */
	std::ofstream RRVtoTransmitted_MetricFile;
  RRVtoTransmitted_MetricFile.open("output/RRV/transmitted_metric.txt");
  if (!RRVtoTransmitted_MetricFile.is_open()) {
      std::cerr << "Error: Could not open the file output/RRV/transmitted_metric.txt" << std::endl;
      return;
  }

	std::ofstream RRVtoTransmitted_ListSizeFile;
  RRVtoTransmitted_ListSizeFile.open("output/RRV/transmitted_listsize.txt");
  if (!RRVtoTransmitted_ListSizeFile.is_open()) {
      std::cerr << "Error: Could not open the file output/RRV/transmitted_listsize.txt" << std::endl;
      return;
  }

	std::ofstream RRVtoDecoded_MetricFile;
  RRVtoDecoded_MetricFile.open("output/RRV/decoded_metric.txt");
  if (!RRVtoDecoded_MetricFile.is_open()) {
      std::cerr << "Error: Could not open the file output/RRV/decoded_metric.txt" << std::endl;
      return;
  }

	std::ofstream RRVtoDecoded_ListSizeFile;
  RRVtoDecoded_ListSizeFile.open("output/RRV/decoded_listsize.txt");
  if (!RRVtoDecoded_ListSizeFile.is_open()) {
      std::cerr << "Error: Could not open the file output/RRV/decoded_listsize.txt" << std::endl;
      return;
  }

	std::ofstream RRVtoDecoded_DecodeTypeFile;
  RRVtoDecoded_DecodeTypeFile.open("output/RRV/decoded_type.txt");
  if (!RRVtoDecoded_DecodeTypeFile.is_open()) {
      std::cerr << "Error: Could not open the file output/RRV/decoded_type.txt" << std::endl;
      return;
  }



	std::cout << "running the ISTC sim" << std::endl;
	srand(42);
	if ((code.numInfoBits + code.crcDeg - 1) % code.k != 0) {
		std::cout << "invalid msg + crc length" << std::endl;
		return;
	}

	std::vector<double> EbN0 = EBN0;
  std::vector<int> puncturedIndices = PUNCTURING_INDICES;

	std::vector<double> SNR;
	double offset = 10 * log10((double)N/K *NUM_INFO_BITS / (138));
	for (int i=0; i< EbN0.size(); i++)
		SNR.push_back(EbN0[i] + offset);

	std::cout << "code.v = " << code.v << std::endl;
	FeedForwardTrellis encodingTrellis(code.k, code.n, code.v, code.numerators);

	// Raw Received Value (RRV) List Decoder
	LowRateListDecoder listDecoder(encodingTrellis, MAX_LISTSIZE, code.crcDeg, code.crc, STOPPING_RULE);
	std::vector<double> RRVtoTransmitted_Metric(MC_ITERS);
	std::vector<int> 		RRVtoTransmitted_ListSize(MC_ITERS);
	std::vector<double> RRVtoDecoded_Metric(MC_ITERS);
	std::vector<int> 		RRVtoDecoded_ListSize(MC_ITERS);
	std::vector<int>		RRV_DecodedType(MC_ITERS);

	/* ==== SIMULATION begins ==== */
	for(int snrIndex = 0; snrIndex < SNR.size(); snrIndex++) {
		double snr = SNR[snrIndex];
		double standardMeanListSize = 0;
		int standardNumErrors = 0;
		int standardListSizeExceeded = 0;



		for(int numTrials = 0; numTrials < MC_ITERS; numTrials++) {

			if (numTrials % 1000 == 0) { std::cout << "currently at " << numTrials << std::endl; }
			
			std::vector<int> originalMessage = generateRandomCRCMessage(code);
			std::vector<int> transmittedMessage = generateTransmittedMessage(originalMessage, encodingTrellis, snr, puncturedIndices, NOISELESS);
			std::vector<double> receivedMessage = addAWNGNoise(transmittedMessage, puncturedIndices, snr, NOISELESS);
			std::vector<int> zero_point(receivedMessage.size(), 0);
			
			
			int sum_abs = std::accumulate(transmittedMessage.begin(), transmittedMessage.end(), 0, [](int sum, int x) {
        return sum + std::abs(x);
			});
			
			// Normalize the received signals to 128 and decode
			double origial_mag_sqrt = utils::euclidean_distance(receivedMessage, zero_point, puncturedIndices);
			
			std::vector<double> normalized_receivedMessage(receivedMessage.size(), 0);
			for (int i = 0; i < receivedMessage.size(); i++) {
				if (std::find(puncturedIndices.begin(), puncturedIndices.end(), i) == puncturedIndices.end()) {
					normalized_receivedMessage[i] = sqrt(128) * receivedMessage[i] / origial_mag_sqrt;
				}
			}

		

			// TRANSMITTED
			RRVtoTransmitted_Metric[numTrials] = (utils::sum_of_squares(receivedMessage, transmittedMessage, puncturedIndices));
			
			// DECODED
      MessageInformation standardDecoding = listDecoder.decode(receivedMessage, puncturedIndices);
			

			// RRV
			if (standardDecoding.message == originalMessage) {
				// correct decoding
				RRV_DecodedType[numTrials] 				= 0;
				RRVtoDecoded_ListSize[numTrials] 	= standardDecoding.listSize;
				RRVtoDecoded_Metric[numTrials] 		= standardDecoding.metric;
			} else if(standardDecoding.listSizeExceeded) {
				// list size exceeded
				RRV_DecodedType[numTrials] = 1;
				standardListSizeExceeded++;
			} else { 
				// incorrect decoding
				RRV_DecodedType[numTrials] 				= 2;
				RRVtoDecoded_ListSize[numTrials] 	= standardDecoding.listSize;
				RRVtoDecoded_Metric[numTrials] 		= standardDecoding.metric;
				standardNumErrors++;
				standardMeanListSize += (double)standardDecoding.listSize;
				// std::cout << "decoding error:  ----------"  << std::endl; 
				// std::cout << "standardDecoding.listSize: " << standardDecoding.listSize << std::endl;
				// std::cout << "standardDecoding.metric: " << standardDecoding.metric << std::endl;
			}

			
		} // for(int numTrials = 0; numTrials < MC_ITERS; numTrials++)

		std::cout << std::endl << "****--- Simulation Concluded ---****" << std::endl;
		std::cout << "at Eb/N0 = " << EbN0[snrIndex] << std::endl;
		std::cout << "number of erasures: " << standardListSizeExceeded << std::endl;
		std::cout << "number of errors: " << standardNumErrors << std::endl;
		std::cout << "Erasures Error Rate: " << std::scientific << (double)standardListSizeExceeded/MC_ITERS << std::endl;
		std::cout << "Undetected Error Rate: " << std::scientific << (double)standardNumErrors/MC_ITERS << std::endl;
		std::cout << "TFR: " << (double)(standardNumErrors + standardListSizeExceeded)/MC_ITERS << std::endl;

		// RRV Write to file
		for (int i = 0; i < RRVtoTransmitted_Metric.size(); i++) {
			RRVtoTransmitted_MetricFile << RRVtoTransmitted_Metric[i] << std::endl;
		}

		for (int i = 0; i < RRVtoDecoded_Metric.size(); i++) {
			RRVtoDecoded_MetricFile << RRVtoDecoded_Metric[i] << std::endl;
		}
		for (int i = 0; i < RRVtoDecoded_ListSize.size(); i++) {
			RRVtoDecoded_ListSizeFile << RRVtoDecoded_ListSize[i] << std::endl;
		}
		for (int i = 0; i < RRV_DecodedType.size(); i++) {
			RRVtoDecoded_DecodeTypeFile << RRV_DecodedType[i] << std::endl;
		}


	} // for(int snrIndex = 0; snrIndex < SNR.size(); snrIndex++)

	// RRV
  RRVtoTransmitted_MetricFile.close();
	RRVtoTransmitted_ListSizeFile.close();
	RRVtoDecoded_MetricFile.close();
	RRVtoDecoded_ListSizeFile.close();
	RRVtoDecoded_DecodeTypeFile.close();

	std::cout << "concluded simulation" << std::endl;
}


// this generates a random binary string of length code.numInfoBits, and appends the appropriate CRC bits
std::vector<int> generateRandomCRCMessage(CodeInformation code){
	std::vector<int> message;
	for(int i = 0; i < code.numInfoBits; i++)
		message.push_back(rand()%2);
	// compute the CRC
	crc::crc_calculation(message, code.crcDeg, code.crc);
	return message;
}

// this takes the message bits, including the CRC, and encodes them using the trellis
std::vector<int> generateTransmittedMessage(std::vector<int> originalMessage, FeedForwardTrellis encodingTrellis, double snr, std::vector<int> puncturedIndices, bool noiseless){
	// encode the message
	std::vector<int> encodedMessage = encodingTrellis.encode(originalMessage);

	return encodedMessage;
}

// this takes the transmitted message and adds AWGN noise to it
// it also punctures the bits that are not used in the trellis
std::vector<double> addAWNGNoise(std::vector<int> transmittedMessage, std::vector<int> puncturedIndices, double snr, bool noiseless){
	std::vector<double> receivedMessage;
	if(noiseless){
		for(int i = 0; i < transmittedMessage.size(); i++)
			receivedMessage.push_back((double)transmittedMessage[i]);
	} else {
		receivedMessage = awgn::addNoise(transmittedMessage, snr);
	}

	// puncture the bits. it is more convenient to puncture on this side than on the 
	// decoder, so we insert zeros which provide no information to the decoder
	for(int index = 0; index < puncturedIndices.size(); index++) {
		if (puncturedIndices[index] > receivedMessage.size()) {
			std::cout << "out of bounds index: " << puncturedIndices[index] << std::endl;
			std::cerr << "Puncturing index out of bounds" << std::endl;
			exit(1);
		}
		receivedMessage[puncturedIndices[index]] = 0;
	}

	return receivedMessage;
}