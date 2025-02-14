#include <iostream>
#include <fstream>
#include <vector>

#include "../include/mla_consts.h"
#include "../include/mla_types.h"
#include "../include/mla_namespace.h"
#include "../include/feedForwardTrellis.h"
#include "../include/lowRateListDecoder.h"

void Noise_injection_sim(CodeInformation code, double injected_power);
std::vector<int> generateRandomCRCMessage(CodeInformation code, bool noiseless);
std::vector<int> generateTransmittedMessage(std::vector<int> originalMessage, FeedForwardTrellis encodingTrellis);
std::vector<double> addAWNGNoiseAndPuncture(std::vector<int> transmittedMessage, std::vector<int> puncturedIndices, double snr, bool noiseless);
std::vector<double> addArtificialNoiseAndPuncture(std::vector<int> transmittedMessage, std::vector<double> artificialNoise, std::vector<int> puncturedIndices);

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

  Noise_injection_sim(code, TARGET_NOISE_ENERGY);

  return 0;
}

void Noise_injection_sim(CodeInformation code, double injected_power) {
	/** Injecting noise with certain power to a fixed codeword and observe list size
	 * 
	 * Input:
	 * 	- code: code information such as K, N, V, etc
	 * 	- injected_noise_power: 
	 */

	/* - Output files setup - */
	std::ofstream hamming_distance_file;
  hamming_distance_file.open("output/hamming_and_listsize/hamming_distance.txt");
  if (!hamming_distance_file.is_open()) {
      std::cerr << "Error: Could not open the file output/hamming_and_listsize/hamming_distance.txt" << std::endl;
      return;
  }

	std::ofstream list_size_file;
  list_size_file.open("output/hamming_and_listsize/list_size.txt");
  if (!list_size_file.is_open()) {
      std::cerr << "Error: Could not open the file output/hamming_and_listsize/list_size.txt" << std::endl;
      return;
  }

	std::ofstream decoding_correctness_file;
  decoding_correctness_file.open("output/hamming_and_listsize/correctness.txt");
  if (!decoding_correctness_file.is_open()) {
      std::cerr << "Error: Could not open the file output/hamming_and_listsize/correctness.txt" << std::endl;
      return;
  }

	/* - Recorder Setup - */
	std::vector<int> hamming_distance_recorder( MC_ITERS );
	std::vector<int> list_size_recorder( MC_ITERS );
	std::vector<int> decoding_correctness_recorder( MC_ITERS );

	FeedForwardTrellis encodingTrellis(code.k, code.n, code.v, code.numerators);

	std::vector<int> message_with_crc = generateRandomCRCMessage( code, NOISELESS );
	std::vector<int> transmitted_message = generateTransmittedMessage( message_with_crc, encodingTrellis );

	/* - Simulation Begin - */
	for ( int iter = 0; iter < MC_ITERS; iter++ ) {

		if (iter % 10 == 0) {std::cout << "Logging: " << iter << " iterations" << std::endl;}

		std::vector<double> standard_noise = awgn::generateStandardNormalNoise( (N/K) * NUM_INFO_BITS );
		std::vector<double> scaled_noise 	 = awgn::scaleNoise(standard_noise, injected_power);

		std::vector<double> noisy_message = addArtificialNoiseAndPuncture(transmitted_message, scaled_noise, PUNCTURING_INDICES);

		double sum = 0.0;
		for (int i = 0; i < scaled_noise.size(); i++) {
			sum += scaled_noise[i];
		}

		// hard-decoding
		std::vector<int> hard_decoding;
		int hamming_distance = 0;

		for (int i = 0; i < noisy_message.size(); i++) {
			if (noisy_message[i] == 0.0) {
				hard_decoding.push_back(0);
				continue;
			}
			if (noisy_message[i] > 0.0) { hard_decoding.push_back(1); }
			if (noisy_message[i] < 0.0) { hard_decoding.push_back(-1); }
		}

		assert(hard_decoding.size() == transmitted_message.size());
		hamming_distance = utils::compute_hamming_distance_with_puncturing(hard_decoding, transmitted_message, PUNCTURING_INDICES);
		hamming_distance_recorder[iter] = hamming_distance;

		// comparing with listsize using soft-decoding
		LowRateListDecoder listDecoder(encodingTrellis, MAX_LISTSIZE, code.crcDeg, code.crc, STOPPING_RULE);

		MessageInformation b_hat = listDecoder.decode(noisy_message, PUNCTURING_INDICES);
		if (b_hat.listSize == 68) {std::cout << "special case with iter = " << iter << std::endl;}
		list_size_recorder[iter] = b_hat.listSize;

		// evaluate correctness
		assert(b_hat.message.size() == message_with_crc.size());
		if (b_hat.message == message_with_crc) {decoding_correctness_recorder[iter] = 1; }
		if (b_hat.message != message_with_crc) {decoding_correctness_recorder[iter] = 0; }
		
	} // for ( int iter = 0; iter < 1; iter++ )


	/* - Write values in recorders to file - */
	for (size_t i = 0; i < hamming_distance_recorder.size(); i++ ) {
		hamming_distance_file << hamming_distance_recorder[i] << std::endl;
	}

	for (size_t i = 0; i < list_size_recorder.size(); i++ ) {
		list_size_file << list_size_recorder[i] << std::endl;
	}

	for (size_t i = 0; i < decoding_correctness_recorder.size(); i++ ) {
		decoding_correctness_file << decoding_correctness_recorder[i] << std::endl;
	}

	/* - close files - */
	hamming_distance_file.close();
	list_size_file.close();
	decoding_correctness_file.close();
}

// this generates a random binary string of length code.numInfoBits, and appends the appropriate CRC bits
std::vector<int> generateRandomCRCMessage(CodeInformation code, bool noiseless){
	
	std::vector<int> message ( code.numInfoBits );

	if (noiseless == 0) {
		for(int i = 0; i < code.numInfoBits; i++)
			message[i] = (rand()%2);
	} else {
		std::fill( message.begin(), message.end(), 0 ); // noiseless
	}
	// compute the CRC
	crc::crc_calculation(message, code.crcDeg, code.crc);
	return message;
}

// this takes the message bits, including the CRC, and encodes them using the trellis
std::vector<int> generateTransmittedMessage(std::vector<int> originalMessage, FeedForwardTrellis encodingTrellis){
	// encode the message
	std::vector<int> encodedMessage = encodingTrellis.encode(originalMessage);

	return encodedMessage;
}

// this takes the transmitted message and adds AWGN noise to it
// it also punctures the bits that are not used in the trellis
std::vector<double> addAWNGNoiseAndPuncture(std::vector<int> transmittedMessage, std::vector<int> puncturedIndices, double snr, bool noiseless){
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


std::vector<double> addArtificialNoiseAndPuncture(std::vector<int> transmittedMessage, std::vector<double> artificialNoise, std::vector<int> puncturedIndices) {
	/** Noise injection experiment ONLY
	 * Input: 
	 * 	- transmittedMessage: 
	 *  - artificialNoise:
	 *  - puncturedIndices: 
	 * 
	 */

	std::vector<double> out(transmittedMessage.size(), 0.0);
	int j = 0;

	for ( int i = 0; i < out.size(); i++ ) {

		auto result = std::find( puncturedIndices.begin(), puncturedIndices.end(), i );

		if (result != puncturedIndices.end()) {
			continue; // if this symbol is punctured
		} else {
			out[i] = transmittedMessage[i] + artificialNoise[j];
			j++;
		}
	}

	assert(j == transmittedMessage.size() - puncturedIndices.size());

	return out;
}

