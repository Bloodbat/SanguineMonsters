#pragma once

// Bjorklunds algorithm for euclidean sequences
//
// Modified GIST from https://gist.github.com/unohee/d4f32b3222b42de84a5f

#include <algorithm>
#include <iostream>
#include <vector>
#include <cmath>

struct Bjorklund {

	Bjorklund() {
		lengthOfSeq = 0;
		pulseAmt = 0;
	};
	Bjorklund(int step, int pulse) : lengthOfSeq(step), pulseAmt(pulse) {};
	~Bjorklund() {
		reset();
	};

	void reset() {
		remainder.clear();
		count.clear();
		sequence.clear();
	};

	std::vector<int> remainder;
	std::vector<int> count;
	std::vector<bool> sequence;

	int lengthOfSeq;
	int pulseAmt;

	void init(int step, int pulse) {
		lengthOfSeq = step;
		pulseAmt = pulse;
	}
	int getSequence(int index) { return sequence.at(index); };
	int size() { return static_cast<int>(sequence.size()); };

	void iter() {
		// Bjorklund algorithm
		// Do E[k,n]. k is number of one's in sequence, and n is the length of sequence.
		int divisor = lengthOfSeq - pulseAmt; //initial amount of zero's

		remainder.push_back(pulseAmt);
		// Iteration
		int index = 0; //we start algorithm from first index.

		while (true) {
			count.push_back(std::floor(divisor / remainder[index]));
			remainder.push_back(divisor % remainder[index]);
			divisor = remainder.at(index);
			index += 1; //move to next step.
			if (remainder[index] <= 1) {
				break;
			}
		}
		count.push_back(divisor);
		buildSeq(index); //place one's and zero's
		reverse(sequence.begin(), sequence.end());

		// Position correction. some of result of algorithm is one step rotated.
		int zeroCount = 0;
		if (sequence.at(0) != 1) {
			do {
				zeroCount++;
			} while (sequence.at(zeroCount) == 0);
			std::rotate(sequence.begin(), sequence.begin() + zeroCount, sequence.end());
		}
	}

	void buildSeq(int slot) {
		// Construct a binary sequence of n bits with k one's, such that the k one's are distributed as evenly as possible among the zero's

		if (slot == -1) {
			sequence.push_back(0);
		} else if (slot == -2) {
			sequence.push_back(1);
		} else {
			for (int i = 0; i < count[slot]; i++)
				buildSeq(slot - 1);
			if (remainder[slot] != 0)
				buildSeq(slot - 2);
		}
	}
};
