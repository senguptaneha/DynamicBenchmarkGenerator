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

#ifndef POWERLAWDEGREESEQUENCE_H
#define POWERLAWDEGREESEQUENCE_H


#include <vector>



class PowerlawDegreeSequence {
public:
	/**
	 * Generates a powerlaw degree sequence with the given minimum and maximum degree, the powerlaw exponent gamma.
	 *
	 * @param minDeg   The minium degree
	 * @param maxDeg   The maximum degree
	 * @param gamma    The powerlaw exponent
	 */
	PowerlawDegreeSequence(int minDeg, int maxDeg, double gamma);





	/**
	 * Tries to set the powerlaw exponent gamma such that the specified average degree is expected.
	 *
	 * @param avgDeg The average degree
	 * @param minGamma The minimum gamma to use, default: -1
	 * @param maxGamma The maximum gamma to use, default: -6
	 */
	void setGammaFromAverageDegree(double avgDeg, double minGamma = -1, double maxGamma = -6);

	/**
	 * Sets the minimum degree.
	 *
	 * @param minDeg The degree that shall be set as minimum degree
	 */
	void setMinimumDegree(int minDeg);

	/**
	 * Gets the minimum degree.
	 *
	 * @return The minimum degree.
	 */
	int getMinimumDegree() const;

	/**
	 * Gets the maximum degree.
	 *
	 * @return The maximum degree.
	 */
	 int getMaximumDegree() const { return maxDeg; };

	 /**
	  * Sets the exponent gamma.
	  *
	  * @param gamma The exponent, must be negative.
	  */
	void setGamma(double gamma);

	 /**
	  * Gets the exponent gamma.
	  *
	  * @return gamma
	  */
	double getGamma() const { return gamma; };

	/**
	 * Execute the generation process
	 */
	void run();

	/**
	 * Returns the expected average degree.
	 *
	 * @return The expected average degree.
	 */
	double getExpectedAverageDegree() const;

	/**
	 * Returns a degree sequence of @a numNodes degrees with even degree sum.
	 *
	 * @param numNodes The number of nodes
	 * @return The generated degree sequence.
	 */
	std::vector<int> getDegreeSequence(int numNodes) const;

	/**
	 * Returns a degree drawn at random with a power law distribution
	 *
	 * @return A degree that follows the generated distribution.
	 */
	int getDegree() const;
private:
	int minDeg, maxDeg;
	double gamma;
	bool hasRun;
	std::vector<double> cumulativeProbability;
};



#endif // POWERLAWDEGREESEQUENCE_H
