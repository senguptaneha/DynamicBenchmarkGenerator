/*
Copyright (c) 2013 Christian Staudt, Henning Meyerhenke

MIT License (http://opensource.org/licenses/MIT)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <cmath>
#include <algorithm>
#include <time.h>
#include <stdlib.h>
#include <iostream>
#include "PowerlawDegreeSequence.h"


PowerlawDegreeSequence::PowerlawDegreeSequence(int minDeg, int maxDeg, double gamma) :
	minDeg(minDeg), maxDeg(maxDeg), gamma(gamma), hasRun(false) {
	/*if (minDeg > maxDeg) throw std::runtime_error("Error: minDeg must not be larger than maxDeg");
	if (gamma > -1) throw std::runtime_error("Error: gamma must be lower than -1");*/
}


void PowerlawDegreeSequence::setMinimumDegree(int minDeg) {
	this->minDeg = minDeg;
	hasRun = false;
}

void PowerlawDegreeSequence::setGamma(double gamma) {
	this->gamma = gamma;
	hasRun = false;
}

void PowerlawDegreeSequence::setGammaFromAverageDegree(double avgDeg, double minGamma, double maxGamma) {
	double gamma_l = maxGamma;
	double gamma_r = minGamma;
	setGamma(gamma_l); run();
	double average_l = getExpectedAverageDegree();
	setGamma(gamma_r); run();
	double average_r = getExpectedAverageDegree();

	// Note: r is the larger expected average degree!
	if (avgDeg > average_r) {
		setGamma(gamma_r);
		return;
	}

	if (avgDeg < average_l) {
		setGamma(gamma_l);
		return;
	}

	while (gamma_l + 0.001 < gamma_r) {
		setGamma((gamma_r + gamma_l) * 0.5); run();

		double avg = getExpectedAverageDegree();

		if (avg > avgDeg) {
			average_r = avg;
			gamma_r = gamma;
		} else {
			average_l = avg;
			gamma_l = gamma;
		}
	}

	if (avgDeg - average_l < average_r - avgDeg) {
		setGamma(gamma_l);
	} else {
		setGamma(gamma_r);
	}
}


int PowerlawDegreeSequence::getMinimumDegree() const {
	return minDeg;
}

void PowerlawDegreeSequence::run() {
	cumulativeProbability.clear();
	cumulativeProbability.reserve(maxDeg - minDeg + 1);

	double sum = 0;

	for (double d = maxDeg; d >= minDeg; --d) {
		sum += std::pow(d, gamma);
		cumulativeProbability.push_back(sum);
	}
	for (int i=0;i<cumulativeProbability.size();i++){
		cumulativeProbability[i] /= sum;
//	for (double & prob : cumulativeProbability) {
		//prob /= sum;
	}

	cumulativeProbability.back() = 1.0;
	hasRun= true;
}

double PowerlawDegreeSequence::getExpectedAverageDegree() const {
	//if (!hasRun) throw std::runtime_error("Error: run needs to be called first");

	double average = cumulativeProbability[0] * maxDeg;
	for (int i = 1; i < cumulativeProbability.size(); ++i) {
		average += (cumulativeProbability[i] - cumulativeProbability[i-1]) * (maxDeg - i);
	}

	return average;
}

std::vector< int > PowerlawDegreeSequence::getDegreeSequence(int numNodes) const {
	std::vector<int> degreeSequence;

	//if (!hasRun) throw std::runtime_error("Error: run needs to be called first");

	degreeSequence.reserve(numNodes);
	int degreeSum = 0;

	for (int i = 0; i < numNodes; ++i) {
		degreeSequence.push_back(getDegree());
		degreeSum += degreeSequence.back();
	}

	if (degreeSum % 2 != 0) {
		(*std::max_element(degreeSequence.begin(), degreeSequence.end()))--;
	}

	return degreeSequence;
}

int PowerlawDegreeSequence::getDegree() const {
	//if (!hasRun) throw std::runtime_error("Error: run needs to be called first");
	double randomValue = ((double)rand())/((double)RAND_MAX);
	return maxDeg - std::distance(cumulativeProbability.begin(), std::lower_bound(cumulativeProbability.begin(), cumulativeProbability.end(), randomValue));
}
