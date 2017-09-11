#include <iostream>
#include <fstream>
#include <vector>
#include <stdlib.h>
#include <time.h>
#include <ctime>
#include <math.h>
#include "PowerlawDegreeSequence.h"
#include <algorithm>
#include <sstream>

#include <string>
#include <string.h>
/*With node changes*/

using namespace std;

/*************************PARAMETERS******************************/
int T = 1000;        //Number of time slots
double lambda = 0.2;//how sharply communities will rise and fall
int N1 = 50000;		//Number of nodes
int N2;             //Number of communities, set in the program
int xmin = 1;		//minimum user-community memberships
int xmax = 50;		//maximum user-community memberships
double beta1 = -2.5;	//community membership exponent
int mmin = 2;		//minimum community size
int mmax = 7500;	    //maximum community size
double beta2 = -2.5;	//community size exponent
double alpha = 2;	//affects intra community edge probability
double gamma_1 = 0.5;	//affects intra community edge probability
double eps = 2;        //inter community edge probability, epsilon = eps/N1
double prob = 4;  //alternative: Intra community edge probability - not used anymore
double probEvent = 0.1; //Probability of an event happening
int timeToMerge = 10;
int timeToSplit = 10;
double minSplitRatio = 0.3; //A community born of split is atleast 0.3 of original community
/*******************************************************************/

/*************************DATA STRUCTURES***************************/
struct edge{
	int sourceId;
	int destId;
	int communityId;
	int startTime;
	int endTime;

	edge(int isSourceId, int isDestId, int isCommunityId){
		this->sourceId = isSourceId;
		this->destId = isDestId;
		this->communityId = isCommunityId;
		this->startTime = -1;
		this->endTime = -1;
	}

	void generateStartExp(int startLimit){
		double z = ((double) rand())/((double) RAND_MAX);
		double cumul_prob = 0;
		for (int i = startLimit; i>=0;i--){
			double p_x = lambda * exp( -1* lambda * (startLimit - i));
			cumul_prob += p_x;
			if (z <= cumul_prob){
				this->startTime = i;
				return;
			}
		}
		this->startTime = 0;
	}

	void generateEndExp(int endLimit){
        if (this->endTime != -1) return;
		double z = ((double) rand())/((double) RAND_MAX);
		double cumul_prob = 0;

		for (int i = endLimit; i<=T;i++){
			double p_x = lambda * exp( -1* lambda * (i - endLimit));
			cumul_prob += p_x;
			if (z <= cumul_prob){
                		//cout << "generateEndExp: Setting end time of " << communityId << ": " << sourceId << ", " << destId << ", " << startTime << ", " << endTime << " to " << i << endl << flush;
                		if (i < this->startTime) i = this->startTime + 1;
				this->endTime = i;

				return;
			}
		}
		this->endTime = T;
	}
};

struct node{
	int nodeId;
	vector<edge*> adj;
	vector<int> communities;
	int startTime;
	int endTime;
	double estimatedDegree;
	node(int isNodeId){
		this->nodeId = isNodeId;
		this->startTime = 0;
		this->endTime = -1;
		this->estimatedDegree = 0;
	}
	node(int isNodeId, int isStartTime){
		this->nodeId = isNodeId;
		this->startTime = isStartTime;
		this->endTime = -1;
		this->estimatedDegree = 0;
	}

	bool addEdge(edge *e){
		if (e->destId == nodeId){
			delete e;
			return false;
		}//no self loops
		bool flag = true;
		for (int i=0;i<adj.size();i++){
			if ((adj[i]->destId == e->destId)){
				if ((adj[i]->endTime == -1)||(adj[i]->endTime > e->startTime)){
					flag = false;
					break;
				}
			}
		}
		if (flag) adj.push_back(e);
		else delete e;  //no multi edges
		return flag;
	}
	/*to be called on destId*/
	edge *findReverseAliveEdge(int srcId){
		for (int i=0;i<adj.size();i++){
			if ((adj[i]->destId == srcId) && (adj[i]->communityId == -1) && (adj[i]->endTime == -1))
				return adj[i];
		}
		return NULL;
	}

	void printCommunities(){
		for (int i=0;i<communities.size();i++) cout << ", " << communities[i];
		cout << endl << flush;
	}
	void printEdgeList(){
		cout << "Edge list of " << nodeId << endl;
		for (int i = 0; i < adj.size(); i++)
			cout << "(" << adj[i]->destId  << ", " << adj[i]->startTime << ", " << adj[i]->endTime << ", " << adj[i]->communityId << ") ";
		cout << endl;
	}
	node(){}

	bool hasEdge(int destId){
		bool flag = false;
		for (int i = 0; i < adj.size(); i++){
			if ((adj[i]->destId == destId))
				flag = true;
		}
		return flag;
	}
};

/*A node attaches to a community via this structure.
 It indicates the joining and leaving time of the node in the
 community.*/
struct nodeInCommunity{
	int nodeId;
	int joinTime;
	int leaveTime;

	nodeInCommunity(int isNodeId){
		this->nodeId = isNodeId;
		this->joinTime = -1;
		this->leaveTime = -1;
	}

	nodeInCommunity(int isNodeId, int isJoinTime){
		this->nodeId = isNodeId;
		this->joinTime = isJoinTime;
		this->leaveTime = -1;
	}
};

struct community{
	vector<nodeInCommunity*> nodeList;
	int birthTime;
	int expansionTime;
	int deathTime;
	int contractionTime;
	int nextAvailableTimeSlot;
	bool isAvailable;
	int originFlag; 	//signifies how the community came into being. 0 for birth, 1 for split, 2 for merge
	int deletionFlag;   //signifies how the community stopped being. 0 for death, 1 for split, 2 for merge

	community(){
		this->isAvailable = true;
		this->originFlag = 0;
		this->deletionFlag = 0;
		this->birthTime = 0;
		this->deathTime = -1;
	}

	int indexOfNode(int nodeId){
		for (int i=0;i<nodeList.size();i++)
			if (nodeList[i]->nodeId == nodeId) return i;
		return -1;
	}

	void swapToEnd(int nodeIndex){
		int lastIndex = nodeList.size() - 1;
		nodeInCommunity *nic = nodeList[lastIndex];
		nodeList[lastIndex] = nodeList[nodeIndex];
		nodeList[nodeIndex] = nic;
	}

	void printNodes(){
		for (int i=0;i<nodeList.size();i++) cout << ", " << nodeList[i]->nodeId;
		cout << endl << flush;
	}
};

struct update{
	int updateType; //0: edge delete; 1: node add; 2: edge add; 3: node delete;
	int u;
	int v;
	int t;

	update(int isUpdateType, int isU, int isV, int isT){
		this->updateType = isUpdateType;
		this->u = isU;
		this->v = isV;
		this->t = isT;
	}
};

vector<node*> graph;
vector<community*> communities;
vector<int> nodeMemberships;
vector<int> communitySizes;


/************PROFILING QUANTITIES************************************/
double averageSplitTime = 0.0;
double averageMergeTime = 0.0;
double averageBirthTime = 0.0;
double averageDeathTime = 0.0;
double averageNodeAddTime = 0.0;
double averageNodeDeleteTime = 0.0;
int nSplits = 0;
int nMerges = 0;
int nBirths = 0;
int nDeaths = 0;
int nAdd = 0;
int nDelete = 0;
/*******************************************************************/

/************************UTILITY FUNCTIONS**************************/
double expectedPowerLaw(int xmin, int xmax, double beta){
	PowerlawDegreeSequence z(xmin,xmax,beta);
	z.run();

	double meanValue = z.getExpectedAverageDegree();
	return meanValue;
}

vector<int> powerLawDegreeSequence(int xmin, int xmax, double beta, int N){
	PowerlawDegreeSequence z(xmin,xmax,beta);
	z.run();

	vector<int> degreeSequence = z.getDegreeSequence(N);
	return degreeSequence;
}

void permuteFY(int *sequence, int size){
	for (int i=0;i<size-1;i++){
		int j = i + rand()%(size-i);
		int temp = sequence[i];
		sequence[i] = sequence[j];
		sequence[j] = temp;
	}
}

int get_next_edge_distance(const double log_cp) {
	return (int) (1 + floor(log(1.0 - (((double) rand())/((double) RAND_MAX))) / log_cp));
}

bool isInCommunityAtT(int t, int communityId, int nodeId){
	community c = *communities[communityId];
	int pos = c.indexOfNode(nodeId);
	bool startFlag = false, endFlag = false;
	if (c.nodeList[pos]->joinTime <= t) startFlag = true;
	if (c.nodeList[pos]->leaveTime == -1) endFlag = true;
	if (c.nodeList[pos]->leaveTime >= t) endFlag = true;
	return (startFlag && endFlag);
}

bool compareUpdate(update i, update j){
	if (i.t < j.t) return true;
	if (i.t > j.t) return false;
	if ((i.u == j.u) && (i.v == j.v)){
		if (i.updateType < j.updateType) return false;
		if (i.updateType > j.updateType) return true;
	}
	if (i.updateType < j.updateType) return true;
	if (i.updateType > j.updateType) return false;

	return false;
}
/*******************************************************************/

/************************STATIC STRUCTURE***************************/
void generateBigraph(){
	double expectedMemberships = expectedPowerLaw(xmin,xmax,beta1);
	double expectedCommunitySize = expectedPowerLaw(mmin,mmax,beta2);
	double M0 = N1 * expectedMemberships;	//Number of edges to be in bigraph
	double N2d = M0/ expectedCommunitySize;	//Therefore, number of communities

	/*Generate power law degree sequences*/
	nodeMemberships = powerLawDegreeSequence(xmin,xmax,beta1,N1);
	communitySizes = powerLawDegreeSequence(mmin,mmax,beta2,N2d);

	N2 = communitySizes.size();
	int maxCommSize = 0;
	for (int i=0;i<N2;i++)
		if (maxCommSize < communitySizes[i]) maxCommSize = communitySizes[i];

	for (int i=0;i<N2;i++)
		communities.push_back(new community());

	for (int i=0;i<N2;i++){
		communities[i]->birthTime = 0;
	}
	int sizeA=0, sizeB=0;
	for (int i=0;i<N1;i++)
		sizeA += nodeMemberships[i];

	for (int i=0;i<N2;i++)
		sizeB += communitySizes[i];

	/*what do we do if sizeA and sizeB do not agree?
	 One (and not the best) solution is to randomly 
	 select a community size and change it by 1*/
	while (sizeA > sizeB){	//increment community sizes
		int randomIndex = rand()%N2;
	if (communitySizes[randomIndex] == mmax) continue;
		communitySizes[randomIndex] += 1;
		sizeB += 1;
	}
	while (sizeA < sizeB){
		int randomIndex = rand()%N2;
        if (communitySizes[randomIndex] == mmin) continue;
		communitySizes[randomIndex] -= 1;
		sizeB -= 1;
	}

	int *A = new int[sizeA];
	int *B = new int[sizeB];
	int l=0;
	for (int i=0;i<N1;i++)
		for (int j=0;j<nodeMemberships[i];j++){
			A[l] = i;
			l++;
		}
	l = 0;
	for (int i=0;i<N2;i++)
		for (int j=0;j<communitySizes[i];j++){
			B[l] = i;
			l++;
		}

	permuteFY(A,sizeA);
	permuteFY(B,sizeB);
	int numEdges = sizeA;
	if (numEdges > sizeB) numEdges = sizeB;

	/*Generate Edges in Bigraph*/
	for (int i=0;i<numEdges;i++){
		int nodeIndex = A[i];
		int commIndex = B[i];

		int index = communities[commIndex]->indexOfNode(nodeIndex);
		if (index == -1){
			graph[nodeIndex]->communities.push_back(commIndex);
			(communities[commIndex]->nodeList).push_back(new nodeInCommunity(nodeIndex,0));
		}
		else{
		/*Attempt rewiring only if communitySize is mmin, because otherwise,
		we will end up with constraint violating community size. 
		We do not do rewiring otherwise*/
		/*select a node at random that is not in this community*/
			if (communitySizes[commIndex] == mmin){

				int nodeId = nodeIndex;
				while ((index != -1) || (graph[nodeId]->communities.size() == 0)){
					nodeId = rand()%N1;
					index = communities[commIndex]->indexOfNode(nodeId);
					if (index != -1) continue;
					bool x = false;
					for (int ci = 0; ci < graph[nodeId]->communities.size(); ci++){
						int newCommunityId = graph[nodeId]->communities[ci];
						int tempIndex = communities[newCommunityId]->indexOfNode(nodeIndex);
						if (tempIndex == -1){
							//assign nodeIndex to newCommunityId 
							graph[nodeIndex]->communities.push_back(newCommunityId);
							(communities[newCommunityId]->nodeList).push_back(new nodeInCommunity(nodeIndex,0));
							//remove nodeId from newCommunityId, assign nodeId to commIndex
							graph[nodeId]->communities[ci] = commIndex;
							(communities[commIndex]->nodeList).push_back(new nodeInCommunity(nodeId,0));
							community *ctemp = communities[newCommunityId];
							for (int ni = 0; ni < ctemp->nodeList.size(); ni++){
								if ((ctemp->nodeList[ni])->nodeId == nodeId){
									ctemp->nodeList.erase(ctemp->nodeList.begin() + ni);
									x = true;
									break;
								}
							}
							if (x) break;
						}
					}
					if (x) break;
				}
            		}
        	}
	}
	for (int i=0;i<N1;i++){
		nodeMemberships[i] = (int) (floor(1.2*nodeMemberships[i]));
	}

	maxCommSize = 0;
	for (int i=0;i<N2;i++)
		if (maxCommSize < communitySizes[i]) maxCommSize = communitySizes[i];
}

void generateEdgesForCommunity(int commIndex){
	vector<int> nodesInCommunity;
	for (int i=0;i<N1;i++){
		for (int j=0;j<graph[i]->communities.size();j++)
			if (graph[i]->communities[j] == commIndex){
				nodesInCommunity.push_back(i);
				break;
			}
	}
	int numberOfNodes = nodesInCommunity.size();
	if (numberOfNodes <= 1) return;

	double probNew = alpha/pow(numberOfNodes, gamma_1);

	if (numberOfNodes == 2) probNew = 1;
	if (probNew > 1.0) probNew = 1.0;
	/*Copied from Networkit implementation of Batagelj Brandes*/
	const double log_cp = log(1.0 - probNew); // log of counter probability
	// create edges
	int curr = 1;
	int next = -1;
	while (curr < numberOfNodes) {
		// compute new step length
		next += get_next_edge_distance(log_cp);
		// check if at end of row
		while ((next >= curr) && (curr < numberOfNodes)) {
			// adapt to next row
			next = next - curr;
			curr++;
		}
		// insert edge
		if (curr < numberOfNodes) {
			int a = nodesInCommunity[curr];
			int b = nodesInCommunity[next];

			edge *fwd = new edge(a,b,commIndex);
			fwd->startTime = 0;
			bool flagR = graph[a]->addEdge(fwd);
			if (flagR){
				edge *bwd = new edge(b,a,-1);    //communityId = -1 for a reverse edge
				bwd->startTime = 0;
				flagR = graph[b]->addEdge(bwd);
				if (!flagR){
					cout << "1.ERROR HERE!" << endl << flush;
					exit(0);
				}
			}

		}
	}

	double expectedDegree = (numberOfNodes-1)*probNew;
	for (int i=0;i<nodesInCommunity.size();i++)
		graph[i]->estimatedDegree += ((int)(ceil(expectedDegree)));

}

void generateEpsCommunity(){
	double epsilon = eps/N1;
	int numEdges = (int) floor(epsilon * N1 * (N1-1) * 0.5);
	for (int i=0;i<numEdges;i++){
		int sourceNode1 = rand()%N1;
		int destNode1 = rand()%N1;
		int sourceNode2 = rand()%N1;
		int destNode2 = rand()%N1;

		int switchTime = T/2 + (rand()%200 - 100);
		if (switchTime < 0) switchTime = 0;
		if (sourceNode1 != destNode1){
			edge *fwd = new edge(sourceNode1,destNode1,-2);    //communityId = -2 for an external edge
			fwd->startTime = 0;
			fwd->endTime = switchTime;
			bool flagR = graph[sourceNode1]->addEdge(fwd);
			if (flagR){
				edge *bwd = new edge(destNode1,sourceNode1,-4);
				bwd->startTime = 0;
				bwd->endTime = switchTime;
				flagR = graph[destNode1]->addEdge(bwd);
				if (!flagR){
					cout << "2.ERROR HERE!" << endl << flush;
					exit(0);
				}
			}
		}
		if (sourceNode2 != destNode2){
			edge *fwd = new edge(sourceNode2,destNode2,-2);    //communityId = -2 for an external edge
			fwd->startTime = switchTime + 1;
			fwd->endTime = -1;
			bool flagR = graph[sourceNode2]->addEdge(fwd);
			if (flagR){
				edge *bwd = new edge(destNode2,sourceNode2,-4);
				bwd->startTime = switchTime + 1;
				bwd->endTime = -1;
				flagR = graph[destNode2]->addEdge(bwd);
				if (!flagR){
					cout << "3.ERROR HERE!" << endl << flush;
					exit(0);
				}
			}
		}
	}
}

bool isSanity = true;
void sanityCheck(){
	for (int i=0;i<N1;i++){
		for (int j=0; j< graph[i]->communities.size(); j++){
			int cIndex = graph[i]->communities[j];
			community *c = communities[cIndex];
			if (c->indexOfNode(i) == -1) isSanity = false;
		}
	}
}

bool sanityCheck2(){
	for (int i=0;i<N1;i++){
		for (int j = 0; j< graph[i]->adj.size(); j++){
			edge *e = graph[i]->adj[j];
			if ((e->startTime > e->endTime) && (e->startTime != -1) && (e->endTime != -1))
				return false;
		}
	}
	return true;
}

bool sanityCheck3(){
	for (int i=0;i<N1;i++){
		for (int j = 0; j < graph[i]->adj.size(); j++){
			int dj = graph[i]->adj[j]->destId, sj = graph[i]->adj[j]->startTime, ej = graph[i]->adj[j]->endTime;
			for (int k = j+1; k < graph[i]->adj.size(); k++){
				int dk = graph[i]->adj[k]->destId, sk = graph[i]->adj[k]->startTime, ek = graph[i]->adj[k]->endTime;
				if (dj == dk){
					//intervals should be disjoint
					bool flag = false;
					if ((sj >= ek) && (ek != -1)) flag = true;
					if ((sk >= ej) && (ej != -1)) flag = true;
					if (!flag){
						cout << "( "<< sj << ", " << ej << " ), ( " << sk << ", " << ek << " )" << ", " << graph[i]->adj[j]->communityId << ", " << graph[i]->adj[k]->communityId << endl;
						return false;
					}
				}
			}
		}
	}
	return true;
}

void generateStaticStructure(){
	for (int i=0;i<N1;i++) graph.push_back(new node(i));

	generateBigraph();

	int numCommunities = communitySizes.size();
	for (int i=0;i<numCommunities;i++){
		generateEdgesForCommunity(i);
	}
	generateEpsCommunity();
	sanityCheck();
}
/*******************************************************************/

/*************************COMMUNITY EVENTS***************************/
void deathCommunity(int commIndex, int timeslot){
	community *c = communities[commIndex];
	c->isAvailable = false;
	c->nextAvailableTimeSlot = -1;
	c->contractionTime = timeslot;
	c->deathTime = timeslot + rand()%10 + 5; //contraction time between 5 and 14 s
	if (c->contractionTime < 0) c->contractionTime = 0;
	int P = c->nodeList.size();
	int coreSize = (int)(0.1*P);
	if (coreSize < mmin) coreSize = mmin;
	for (int i=0 ; i < c->nodeList.size() ; i++){
        	if (c->nodeList[i]->leaveTime > -1) continue;
		if (i < coreSize) c->nodeList[i]->leaveTime = c->contractionTime;
		else c->nodeList[i]->leaveTime = (int) floor(c->contractionTime + (((double) i)/P)*(c->deathTime - c->contractionTime));
	}

	for (int i=0;i<c->nodeList.size();i++){
		int u = c->nodeList[i]->nodeId;
		if (graph[u]->endTime > -1) continue;   //this vertex is already dead, and so has left the community
			nodeMemberships[u] -= 1;

		for (int j=0; j< graph[u]->adj.size();j++){
			if (graph[u]->adj[j]->communityId == commIndex){
				//generate end time with exponential
				int p_u = i;
				int p_v = c->indexOfNode(graph[u]->adj[j]->destId);
				int uLeaveTime = c->nodeList[p_u]->leaveTime;
				int vLeaveTime = c->nodeList[p_v]->leaveTime;
				int minLeaveTime = uLeaveTime;
				if (minLeaveTime > vLeaveTime) minLeaveTime = vLeaveTime;
				int endLimit = minLeaveTime;
				graph[u]->adj[j]->generateEndExp(endLimit);
				//set end time of reverse edge here
				int dj = graph[u]->adj[j]->destId;
				edge *reverseEdge = graph[dj]->findReverseAliveEdge(u);
				if ((reverseEdge)&&(reverseEdge->endTime == -1)) reverseEdge->endTime = graph[u]->adj[j]->endTime;
			}
		}
	}
}

void birthCommunity(int timeslot){
	//draw community size
	PowerlawDegreeSequence z(mmin,mmax,beta2);
	z.run();
	int commSize = z.getDegree();

	//draw community members
	double *nodeProbs = new double[N1];
	int numCandidateNodes = 0;
	for (int i=0;i<N1;i++){
		nodeProbs[i] = nodeMemberships[i] - graph[i]->communities.size();
		if (nodeProbs[i] <= 0) nodeProbs[i] = 0;
		else numCandidateNodes += 1;
	}

	//normalize nodeProbs
	if (numCandidateNodes < commSize) return;   //could not birth community
	double sumNodeProbs = 0;
	for (int i=0;i<N1;i++) sumNodeProbs += nodeProbs[i];
	for (int i=0;i<N1;i++) nodeProbs[i] /= sumNodeProbs;
	//for (int i=1;i<N1;i++) nodeProbs[i] += nodeProbs[i-1];

	int commIndex = communities.size();
	//cout << "Birthing community " << commIndex << " at " << timeslot << endl;
	//cout << "N2 = " << N2 << endl;
	communities.push_back(new community());
	N2 += 1;
	community *c = communities[commIndex];
	c->birthTime = timeslot;
	c->expansionTime = timeslot + rand()%10 + 5; //contraction time between 5 and 14 s
	int currSize = 0;
	bool *nodeSelected = new bool[N1];  //keeps track of which nodes have been selected in the community
	for (int i=0;i<N1;i++) nodeSelected[i] = false;

	int *res = new int[commSize];
	double *keys = new double[N1];
	for (int i=0;i<N1;i++){
		if (nodeProbs[i]==0) continue;
		double r = ((double) rand())/((double) RAND_MAX);
		keys[i] = pow(r,nodeProbs[i]);
	}
	double minValue = 1;
	int minIndex = -1;
	int i;
	for (i=0;i<N1;i++){
		if (currSize == commSize) break;
		if (nodeProbs[i] > 0){
			res[currSize] = i;

			if (minValue >= keys[i]){
				minValue = keys[i];
				minIndex = currSize;
			}
			currSize += 1;
		}
	}

	for (;i<N1;i++){
		if (nodeProbs[i]>0){
			if (keys[i] > minValue){
				res[minIndex] = i;
				minValue = 1;
				minIndex = -1;
				for (int j=0;j<commSize;j++){
					if (minValue >= keys[res[j]]){
						minValue = keys[res[j]];
						minIndex = j;
					}
				}
			}
		}
	}
	if (commSize < mmin) return;

	for (i=0;i<commSize;i++){
		c->nodeList.push_back(new nodeInCommunity(res[i]));
		graph[res[i]]->communities.push_back(commIndex);
	}


	//generate internal edges
	vector<int> nodesInCommunity;
	for (int i=0;i<c->nodeList.size();i++)
		nodesInCommunity.push_back(c->nodeList[i]->nodeId);

	int numberOfNodes = nodesInCommunity.size();

	//double probNew = 2*prob/(numberOfNodes-1);
	double probNew = alpha/pow(numberOfNodes,gamma_1);
	if ((probNew > 1.0) || (numberOfNodes == 2)) probNew = 1;
	int coreSize = (int)(0.1*numberOfNodes);
	if (coreSize < mmin) coreSize = mmin;
	for (int i=0; i<numberOfNodes; i++){
		if (i < coreSize) c->nodeList[i]->joinTime = c->birthTime;
		else c->nodeList[i]->joinTime = (int) (floor(c->birthTime + (((double) i)/numberOfNodes)*(c->expansionTime - c->birthTime)));
	}

	/*Copied from Networkit implementation of Batagelj Brandes*/
	const double log_cp = log(1.0 - probNew); // log of counter probability
	// create edges
	int curr = 1;
	int next = -1;
	c->nextAvailableTimeSlot = -1;
	while (curr < numberOfNodes) {
		// compute new step length
		next += get_next_edge_distance(log_cp);
		// check if at end of row
		while ((next >= curr) && (curr < numberOfNodes)) {
			// adapt to next row
			next = next - curr;
			curr++;
		}
		// insert edge
		if (curr < numberOfNodes) {
			int a = nodesInCommunity[curr];
			int b = nodesInCommunity[next];
			bool flag = true;
			for (int l=0;l < graph[a]->adj.size();l++){
				if ((graph[a]->adj[l])->destId == b){
					flag = false;
					break;
				}
			}
			if (!flag) continue;
			edge *fwd = new edge(a,b,commIndex);
			//generate start time
			int p_u = c->indexOfNode(a);
			int p_v = c->indexOfNode(b);
			int uJoinTime = c->nodeList[p_u]->joinTime;
			int vJoinTime = c->nodeList[p_v]->joinTime;
			int maxJoinTime = uJoinTime;
			if (maxJoinTime < vJoinTime) maxJoinTime = vJoinTime;
			int startLimit = maxJoinTime;
		}
	}
	//mark busy
	c->isAvailable = false;
}

void generateEdgesForSplitCommunity(int commIndex, int timeslot){
	community *c = communities[commIndex];
	int numberOfNodes = c->nodeList.size();
	if (numberOfNodes <= 1) return;

	double p1 = alpha/pow(numberOfNodes, gamma_1);
	double p0 = 0;
	for (int i=0; i<numberOfNodes; i++){
		int u = c->nodeList[i]->nodeId;
		for (int j=0; j < graph[u]->adj.size(); j++){
			if (graph[u]->adj[j]->communityId == commIndex)
				p0 += 1;
		}
	}
	p0 /= (numberOfNodes*(numberOfNodes-1))/2.0;
	if (p1 <= p0) return;
	double probNew = (p1-p0)*(1+p0);
	if (numberOfNodes == 2) probNew = 1;
	if (probNew > 1.0) probNew = 1.0;
	/*Copied from Networkit implementation of Batagelj Brandes*/
	const double log_cp = log(1.0 - probNew); // log of counter probability
	// create edges
	int curr = 1;
	int next = -1;
	while (curr < numberOfNodes) {
		// compute new step length
		next += get_next_edge_distance(log_cp);
		// check if at end of row
		while ((next >= curr) && (curr < numberOfNodes)) {
			// adapt to next row
			next = next - curr;
			curr++;
		}
		// insert edge
		if (curr < numberOfNodes) {
			int a = c->nodeList[curr]->nodeId;
			int b = c->nodeList[next]->nodeId;
			edge *fwd = new edge(a,b,commIndex);
			fwd->startTime = timeslot;
			bool flag = graph[a]->addEdge(fwd);
			if (flag){
				edge *bwd = new edge(b,a,-1);    //communityId = -1 for a reverse edge
				bwd->startTime = timeslot;
				flag = graph[b]->addEdge(bwd);
				if (!flag){
					cout << "5.ERROR HERE!" << endl << flush;
					graph[a]->printEdgeList();
					graph[b]->printEdgeList();
					exit(0);
				}
			}
		}
	}
}

void splitCommunity(int commIndex, int timeslot){

	//cout << "N2 = " << N2 << endl << flush;
	community *c = communities[commIndex];

	c->isAvailable = false;
	c->nextAvailableTimeSlot = -1;  //community dead, will never be available again
	c->deletionFlag = 1;
	double splitPoint = minSplitRatio + (((double) rand())/((double) RAND_MAX))*(1.0 - 2.0*minSplitRatio);
	int splitBorder = (floor) (splitPoint * c->nodeList.size());
	int L = splitBorder+1;
	int R = c->nodeList.size() - L;


	//create the two smaller communities
	int c1Index = communities.size();
	communities.push_back(new community());
	community *c1 = communities[c1Index];
	c1->originFlag = 1;
	c1->isAvailable = false;


	int c2Index = communities.size();
	communities.push_back(new community());
	community *c2 = communities[c2Index];
	c2->originFlag = 1;
	c2->isAvailable = false;
	//cout << "Splitting community " << commIndex << ", of size = " << c->nodeList.size() << ", into " << c1Index << " and " << c2Index << " at " << timeslot << ", originFlag = " << c->originFlag << endl << flush;

	int *leaveTimes = new int[c->nodeList.size()];
	int numNodesInvolved = 0;
	for (int i=0;i < (c->nodeList.size());i++){
		int u = (c->nodeList[i])->nodeId;
		if (graph[u]->endTime > -1){
			leaveTimes[i] = -1;
			continue;
		}
		numNodesInvolved += 1;
		c->nodeList[i]->leaveTime = timeslot;
		nodeInCommunity *nic = new nodeInCommunity(u,timeslot);
		if (i>splitBorder){
			leaveTimes[i] = (int) floor(timeslot + (((double) (R+L-i))/R)*timeToSplit);
			c2->nodeList.push_back(nic);
			graph[u]->communities.push_back(c2Index);
		}
		else{
			leaveTimes[i] = (int) floor(timeslot + (((double) i)/L)*timeToSplit);
			c1->nodeList.push_back(nic);
			graph[u]->communities.push_back(c1Index);
		}
	}
	//generate edge end times
	int maxEndTime = 0;

	for (int i=0;i<=splitBorder;i++){	//for all nodes in c1
		int u = c->nodeList[i]->nodeId;
		for (int j=0;j<graph[u]->adj.size();j++){
			if (graph[u]->adj[j]->communityId != commIndex) continue;    //is a different edge, no need to generate end time

			int v = graph[u]->adj[j]->destId;
			if (graph[v]->endTime > -1) continue;
			int indexOfv = c->indexOfNode(v);
			if (indexOfv > splitBorder){	//if v is in c2
				int endLimit = (int) floor(0.5*leaveTimes[i] + 0.5*leaveTimes[indexOfv]);
				if (endLimit < timeslot){
					cout << "1.Now this problem is there!" << endl; 
				}
				graph[u]->adj[j]->generateEndExp(endLimit);
				//set endTime of reverse edge here
				int dj = graph[u]->adj[j]->destId;
				edge *reverseEdge = graph[dj]->findReverseAliveEdge(u);
				if (reverseEdge){
					if (reverseEdge->endTime == -1)
						reverseEdge->endTime = graph[u]->adj[j]->endTime;
				}
				if (maxEndTime < graph[u]->adj[j]->endTime) maxEndTime = graph[u]->adj[j]->endTime;
			}
			else graph[u]->adj[j]->communityId = c1Index;
		}
	}
	/*This loop is required because we are checking if the edge processed has community id = commIndex*/
	for (int i=splitBorder+1;i<numNodesInvolved;i++){	//for all nodes in c2
		int u = c->nodeList[i]->nodeId;
		for (int j=0;j<graph[u]->adj.size();j++){
			if (graph[u]->adj[j]->communityId != commIndex) continue;    //is a different edge, no need to generate end time
			int v = graph[u]->adj[j]->destId;
			if (graph[v]->endTime > -1) continue;   //v is already dead
			int indexOfv = c->indexOfNode(v);
			if (indexOfv <= splitBorder && indexOfv >=0){ //if v is in c1
				int endLimit = (int) floor(0.5*leaveTimes[indexOfv] + 0.5*leaveTimes[i]);
				if (endLimit < timeslot){
					cout << "2. Now this problem is there!" << endl;
				}
				graph[u]->adj[j]->generateEndExp(endLimit);
				//set end time of reverse edge here
				int dj = graph[u]->adj[j]->destId;
				edge *reverseEdge = graph[dj]->findReverseAliveEdge(u);
				if ((reverseEdge)&&(reverseEdge->endTime==-1)) 
					reverseEdge->endTime = graph[u]->adj[j]->endTime;
				if (maxEndTime < graph[u]->adj[j]->endTime) maxEndTime = graph[u]->adj[j]->endTime;
			}
			else graph[u]->adj[j]->communityId = c2Index;
		}
	}

	c1->birthTime = timeslot + 1;
	c2->birthTime = timeslot + 1;
	c->deathTime = timeslot;
	c1->nextAvailableTimeSlot = maxEndTime+1;
	c2->nextAvailableTimeSlot = maxEndTime+1;
	generateEdgesForSplitCommunity(c1Index,timeslot);
	generateEdgesForSplitCommunity(c2Index,timeslot);
	N2 += 2;
	delete [] leaveTimes;
}

void mergeCommunities(int c1Index, int c2Index, int timeslot){
	community *c1 = communities[c1Index];
	community *c2 = communities[c2Index];

	c1->isAvailable = false;
	c1->nextAvailableTimeSlot = -1; //community dead, will never be available again
	c1->deletionFlag = 2;
	c2->isAvailable = false;
	c2->nextAvailableTimeSlot = -1;
	c2->deletionFlag = 2;
	int cIndex = communities.size();
	communities.push_back(new community());
	community *c = communities[cIndex];
	//cout << "Merging communities " << c1Index << " and " << c2Index << " into " << cIndex << " at " << timeslot << endl;
	//cout << "N2 = " << N2 << endl << flush;
	c->originFlag = 2;
	c->isAvailable = false;
	int L = c1->nodeList.size();
	int R = c2->nodeList.size();

	for (int i=0;i<c1->nodeList.size();i++){
		int u = c1->nodeList[i]->nodeId;
		if (graph[u]->endTime > -1) continue;
		c1->nodeList[i]->leaveTime = timeslot + timeToMerge;
		nodeInCommunity *nic = new nodeInCommunity(u, timeslot + timeToMerge + 1);
		c->nodeList.push_back(nic);
		graph[u]->communities.push_back(cIndex);

		nodeInCommunity *nic1 = new nodeInCommunity(u, (int) floor(timeslot + (1-(((double) i)/c1->nodeList.size()))*timeToMerge));
		nic1->leaveTime = timeslot + timeToMerge;
		c2->nodeList.push_back(nic1);
		graph[u]->communities.push_back(c2Index);
		//cout << "Inserted " << u << " to c2" << endl;
	}
	for (int i=0;i<R;i++){
		int u = c2->nodeList[i]->nodeId;
		if (graph[u]->endTime > -1) continue;
		c2->nodeList[i]->leaveTime = timeslot + timeToMerge;
		int posU = c->indexOfNode(u);
		if (posU != -1) continue;
		nodeInCommunity *nic = new nodeInCommunity(u, timeslot + timeToMerge + 1);
		c->nodeList.push_back(nic);
		graph[u]->communities.push_back(cIndex);

		nodeInCommunity *nic1 = new nodeInCommunity(u, (int) floor(timeslot + (((double) i)/c2->nodeList.size())*timeToMerge));
		nic1->leaveTime = timeslot + timeToMerge;
		c1->nodeList.push_back(nic1);
		graph[u]->communities.push_back(c1Index);
		//cout << "Inserted " << u << " to c1" << endl;
	}

	/*number of edges to be inserted between nodes of c1 and c2
	* depends on the balance number of edges that remains from 
	* the required number in c minus the existing number in c1 + c2*/
	int maxStartTime=0;
	int numberOfNodes = c->nodeList.size();
	double prob = alpha/pow(numberOfNodes, gamma_1);
	double m1 = 0, m2 = 0, m = prob*(numberOfNodes)*(numberOfNodes-1)*0.5;
	for (int i=0; i< c1->nodeList.size();i++)
		for (int j=0;j<graph[i]->adj.size();j++)
			if (graph[i]->adj[j]->communityId == c1Index){
				m1 += 1;
				graph[i]->adj[j]->communityId = cIndex;
			}
	for (int i=0; i< c2->nodeList.size();i++)
		for (int j=0;j<graph[i]->adj.size();j++)
			if (graph[i]->adj[j]->communityId == c2Index){
				m2 += 1;
				graph[i]->adj[j]->communityId = cIndex;
			}
	double balanceEdges = m - m1 - m2;
	double probNew = balanceEdges/(L*R);
	//generate edges and their start times
	if ((probNew > 1.0) || (numberOfNodes == 2)) probNew = 1;
	if (probNew < 0.0) probNew = 0;
	//cout << "L = " << L << ", R = " << R << ", timeToMerge = " << timeToMerge << endl << flush;
	for (int i=0;i<L;i++){
		int u = c1->nodeList[i]->nodeId;
		if (graph[u]->endTime > -1) continue;
		int posU = c2->indexOfNode(u);
		for (int j=0;j<R;j++){
			int v = c2->nodeList[j]->nodeId;
			if (graph[v]->endTime > -1) continue;
			bool flag = true;
			for (int l=0;l<graph[u]->adj.size();l++)
				if (graph[u]->adj[l]->destId == v){
					flag = false;
					break;
				}
			if (!flag) continue;
			double r = ((double) rand())/((double) RAND_MAX);
			if (r<=probNew){
				edge *fwd = new edge(u,v,cIndex);
				int posV = c1->indexOfNode(v);
				int startLimit = (int) floor(0.5*c1->nodeList[posV]->joinTime + 0.5*c2->nodeList[posU]->joinTime);
				if (startLimit < timeslot){	//this can happen because of a pre-existing overlap
					//cout << "i = " << i << ", j = " << j << ", posU = " << posU << ", posV = " << posV << ", startLimit = " << startLimit << ", c1->nodeList[posV].joinTime = " << c1->nodeList[posV].joinTime << ", c2->nodeList[posU].joinTime = " << c2->nodeList[posU].joinTime <<  endl;
					startLimit = timeslot;
				}
				fwd->generateStartExp(startLimit);
				if (maxStartTime < fwd->startTime) maxStartTime = fwd->startTime;
				bool flag = graph[u]->addEdge(fwd);
				if (flag){
					edge *bwd = new edge(v,u,-1);
					bwd->startTime = fwd->startTime;
					flag = graph[v]->addEdge(bwd);
					if (!flag){
						cout << "6.ERROR HERE!" << endl << flush;
						exit(0);
					}
				}
			}
		}
	}

	c->birthTime = timeslot + timeToMerge + 1;
	c1->deathTime = timeslot + timeToMerge;
	c2->deathTime = timeslot + timeToMerge;

	c->nextAvailableTimeSlot = maxStartTime+1;
	N2 += 1;

	/*cout << "End: c2s nodeList" << endl;
	for (int i=0;i<R;i++)
		cout << "id: " << c2->nodeList[i].nodeId << ", join = " << c2->nodeList[i].joinTime << ", leave = " << c2->nodeList[i].leaveTime << endl;

	cout << "Finished Merging" << endl;
	*/

}
/*********************************************************************/

/**************************VERTEX EVENTS****************************/
void addNode(int timeslot){
	int u = N1;
	N1 += 1;
	PowerlawDegreeSequence z(xmin,xmax,beta1); //get community membership
	z.run();
	int nodeMembership = z.getDegree();
	nodeMemberships.push_back((int)(floor(1.2*nodeMembership)));
	graph.push_back(new node(u,timeslot));
	for (int i=0;i<eps;i++){
		int v = rand()%(N1-1);
		while (graph[v]->endTime != -1)
			v = rand()%(N1-1);
		edge *fwd = new edge(u,v,-2);
		fwd->startTime = timeslot;
		bool flag = graph[u]->addEdge(fwd);
		if (flag){
			edge *bwd = new edge(v,u,-1);    //communityId = -1 for a reverse edge
			bwd->startTime = timeslot;
			flag = graph[v]->addEdge(bwd);
			if (!flag){
				cout << "7.ERROR HERE!" << endl << flush;
				exit(0);
			}
		}
	}
}

void deleteNode(int nodeIndex, int timeslot){
	nodeMemberships[nodeIndex] = 0; //no future births will involve this node
	for (int i=0;i<graph[nodeIndex]->communities.size();i++){
		int communityId = graph[nodeIndex]->communities[i];
		community c = *communities[communityId];
		int pos = c.indexOfNode(nodeIndex);
		if (c.nodeList[pos]->leaveTime == -1)
			c.nodeList[pos]->leaveTime = timeslot;
		int numNodesAlive = c.nodeList.size();
		for (int j=0;j<c.nodeList.size();j++)
			if (c.nodeList[j]->leaveTime > -1) numNodesAlive -= 1;
		if (numNodesAlive < mmin){
			for (int j=0;j<c.nodeList.size();j++)
				c.nodeList[j]->leaveTime = timeslot;
			c.isAvailable = false;
			c.nextAvailableTimeSlot = -1;
			c.deathTime = timeslot;    
		}
	}
	for (int i=0;i<graph[nodeIndex]->adj.size();i++){
		if (graph[nodeIndex]->adj[i]->endTime == -1) graph[nodeIndex]->adj[i]->endTime = timeslot;
	}
	graph[nodeIndex]->endTime = timeslot;
}

/*******************************************************************/
void generateEvent(int timeslot){
	struct timespec start,finish;
	int z;
	z = rand()%50 + 1;
	if (z==0){
		z = rand()%4;
		if (z>0){
			//insert node
			//cout << "Starting event add node" << endl;
			clock_gettime(CLOCK_MONOTONIC,&start);
			addNode(timeslot);
			clock_gettime(CLOCK_MONOTONIC,&finish);
			averageNodeAddTime += (finish.tv_sec - start.tv_sec) + (finish.tv_nsec - start.tv_nsec)/pow(10,9);
			nAdd += 1;
			//cout << "Finished event add node" << endl;
		}
		else{
			clock_gettime(CLOCK_MONOTONIC, &start);
			int u = rand()%N1;
			while (graph[u]->endTime > -1) u = rand()%N1;
			deleteNode(u,timeslot);
			clock_gettime(CLOCK_MONOTONIC,&finish);
			averageNodeDeleteTime += (finish.tv_sec - start.tv_sec) + (finish.tv_nsec - start.tv_nsec)/pow(10,9);
			nDelete += 1;
		}
		return;
	}
	z = rand()%4;
	if (z==0){
		//draw an available community at random
		clock_gettime(CLOCK_MONOTONIC,&start);
		int commToDelete = -1, numCandidates = 0;
		for (int i=0;i<N2;i++){
			if (communities[i]->isAvailable){
				numCandidates += 1;
				if (rand()%numCandidates == 0)
					commToDelete = i;
			}
		}
		if (commToDelete != -1) deathCommunity(commToDelete,timeslot); 
		//cout << "Deleting Community " << commToDelete << " at " << timeslot << endl;
		clock_gettime(CLOCK_MONOTONIC,&finish);
		averageDeathTime += (finish.tv_sec - start.tv_sec) + (finish.tv_nsec - start.tv_nsec)/pow(10,9);
		nDeaths += 1;
	}
	else if (z==1){
		clock_gettime(CLOCK_MONOTONIC,&start);
		//cout << "Birthing Community " << N2 << " at " << timeslot << endl;
		birthCommunity(timeslot);
		clock_gettime(CLOCK_MONOTONIC,&finish);
		averageBirthTime += (finish.tv_sec - start.tv_sec) + (finish.tv_nsec - start.tv_nsec)/pow(10,9);
		nBirths += 1;
	}
	else if (z==2){
		int minSplitSize = (int)(ceil(mmin/minSplitRatio));
		clock_gettime(CLOCK_MONOTONIC,&start);
		int commToSplit = -1, numCandidates = 0;
		for (int i=0;i<N2;i++){
			if (communities[i]->isAvailable && (communities[i]->nodeList).size() > (minSplitSize)){
				numCandidates += 1;
				if (rand()%numCandidates == 0)
					commToSplit = i;
			}
		}
		if (commToSplit !=-1){
			splitCommunity(commToSplit,timeslot);
		}
		//else cout << "Could not find suitable community to split!" << endl;
		clock_gettime(CLOCK_MONOTONIC,&finish);
		averageSplitTime += (finish.tv_sec - start.tv_sec) + (finish.tv_nsec - start.tv_nsec)/pow(10,9);
		nSplits += 1;
	}
	else if (z==3){ //merge
		clock_gettime(CLOCK_MONOTONIC,&start);
		int comm[2], numCandidates = 0;
		for (int i=0;i<N2;i++){
			if (!communities[i]->isAvailable) continue;
			if (((communities[i]->nodeList).size()) < mmax/2){
				numCandidates += 1;
				int r = rand()%numCandidates;
				if (r==0) comm[0] = i;
			}
		}
		numCandidates = 0;
		int commSize = (communities[comm[0]]->nodeList).size();
		for (int j=0; j<N2;j++){
			if (comm[0]==j) continue;
			if (!communities[j]->isAvailable) continue;
			if (((communities[j]->nodeList).size() + commSize)<=mmax){
				numCandidates += 1;
				int r = rand()%numCandidates;
				if (r==0){
					comm[1] = j;
				}
			}
		}
		if (numCandidates >= 2){
			mergeCommunities(comm[0],comm[1],timeslot);
		}
		clock_gettime(CLOCK_MONOTONIC,&finish);
		averageMergeTime += (finish.tv_sec - start.tv_sec) + (finish.tv_nsec - start.tv_nsec)/pow(10,9);
		nMerges += 1;
	}
}

void randomlyPerturb(community* c, int timeslot, int commIndex){
	//sample a node
	int numberOfNodes = c->nodeList.size();
	int nodeIndex = rand() % numberOfNodes;
	int nodeId = c->nodeList[nodeIndex]->nodeId;
	//sample an edge of this node - to be deleted
	edge *eToD = NULL;
	int numEdges = 0;
	for (int i = 0; i < graph[nodeId]->adj.size(); i++){
		edge *e = graph[nodeId]->adj[i];
		if ((e->endTime == -1)&&(e->communityId == commIndex)&&(e->startTime < timeslot)){
			numEdges += 1;
			int r = rand()%numEdges;
			if (r==0) eToD = e;
		}
	}
	if (eToD){
		if ((eToD->startTime > eToD->endTime) && (eToD->startTime != -1) && (eToD->endTime != -1)){
			cout << "communityId = " << eToD->communityId << ", startTime = " << eToD->startTime << ", endTime = " << eToD->endTime << endl;
			exit(0);
		}
	}
	else return;

	//sample a node pair to be inserted as edge
	int sNode = eToD->sourceId;
	int dNode = -1;
	int numCandidateNodes = 0;
	for (int i = 1; i < numberOfNodes; i++){
		int d = c->nodeList[i]->nodeId;
		if (d == sNode) continue;
		if (graph[sNode]->hasEdge(d) == false){
			numCandidateNodes += 1;
			int r = rand()%(numCandidateNodes);
			if (r==0) dNode = d;
		}
	}
	if (dNode == -1) return;

	//cout << "Inserting Edge " << commIndex << ":" << sNode << ", " << dNode << ", " << timeslot << endl << flush;
	edge *fwd = new edge(sNode,dNode,commIndex);
	fwd->startTime = timeslot;
	bool flagR = graph[sNode]->addEdge(fwd);
	if (flagR){
		edge *bwd = new edge(dNode, sNode, -1);
		bwd->startTime = timeslot;
		flagR = graph[dNode]->addEdge(bwd);
		if (!flagR){
			cout << "8.ERROR HERE!" << endl << flush;
			graph[sNode]->printEdgeList();
			graph[dNode]->printEdgeList();
			exit(0);
		}
	}
	eToD->endTime = timeslot;
	edge *reverseEdge = graph[eToD->destId]->findReverseAliveEdge(eToD->sourceId);
	if ((reverseEdge)&&(reverseEdge->endTime == -1)) reverseEdge->endTime = timeslot;
}

void generateTimeslot(int timeslot){
	double z = ((double) rand())/RAND_MAX;  //10 % probability of an event happening
	for (int i=0;i<N2;i++){
		if (communities[i]->nextAvailableTimeSlot == timeslot) communities[i]->isAvailable = true;
		if (communities[i]->isAvailable){
			int perturb = rand()%100;
			if (perturb == 0) randomlyPerturb(communities[i], timeslot, i);
		}
		if ((communities[i]->originFlag==2) && (communities[i]->birthTime >= timeslot - 100)){
			for (int j=0;j<10;j++) randomlyPerturb(communities[i], timeslot,i);
		}
	}

	if (z<=probEvent){
		generateEvent(timeslot);
	}
}
/********************************************************************/


void printGraph(){
	ofstream myfile;
	myfile.open("ckbDynamicGraphByNode");
	for (int i=0;i<N1;i++){
		myfile << "Node: " << i << endl;
		for (int j=0;j<graph[i]->adj.size();j++){
			myfile << "(" << graph[i]->adj[j]->destId << ", " << graph[i]->adj[j]->startTime << ", " << graph[i]->adj[j]->endTime << ", " << graph[i]->adj[j]->communityId << ")" << endl;
			if (graph[i]->adj[j]->startTime <= -1 && graph[i]->adj[j]->communityId != -1) cout << "Yep, trouble " << graph[i]->adj[j]->startTime << ", " << graph[i]->adj[j]->communityId <<endl;
		}
	}
	myfile.close();
}

void printCommunity(){
	ofstream myfile;
	myfile.open("ckbDynamicCoverByNode");
	/*int *numNodesTouched = new int[N2];
	for (int i=0;i<N2;i++) numNodesTouched[i] = (communodeInCommunitynities[i].nodeList).size();*/
	for (int i=0;i<N1;i++){
		myfile << i << ":";
		for (int j=0;j<graph[i]->communities.size();j++){
			int communityId = graph[i]->communities[j];
			community c = *communities[communityId];
			int pos = c.indexOfNode(i);
			if (pos==-1){
				cout << "Trouble with " << c.originFlag << endl << flush;
			}
			//numNodesTouched[communityId] -= 1;
			myfile << "(" << graph[i]->communities[j] << ", " << c.nodeList[pos]->joinTime << ", " << c.nodeList[pos]->leaveTime << ")" << endl;
		}
		myfile << endl;
	}
	myfile.close();
	ofstream myfile1;
	myfile1.open("ckbDynamicCoverByCommunity");
	/*int *numNodesTouched = new int[N2];
	for (int i=0;i<N2;i++) numNodesTouched[i] = (communities[i].nodeList).size();*/
	for (int i=0;i<N2;i++){
		myfile1 << i << ":";
		for (int j=0;j< (communities[i]->nodeList).size();j++){
			myfile1 << "(" << (communities[i]->nodeList[j])->nodeId << ", " << (communities[i]->nodeList[j])->joinTime << ", " << (communities[i]->nodeList[j])->leaveTime << ")" << endl;
		}
		myfile1 << endl;
	}
	myfile1.close();
}



void printGraphStream(){
	vector<update> stream;

	for (int i=0;i<N1;i++){
		if (graph[i]->startTime > 0)
			stream.push_back(update(1, i, -1, graph[i]->startTime));
		if (graph[i]->endTime > 0)
			stream.push_back(update(3, i, -1, graph[i]->endTime));
		for (int j=0;j<(graph[i]->adj).size();j++){
			if ((graph[i]->adj[j]->communityId == -1)||(graph[i]->adj[j]->communityId == -4))
				continue;
			if ((graph[i]->adj[j]->startTime > graph[i]->adj[j]->endTime) && (graph[i]->adj[j]->startTime != -1) && (graph[i]->adj[j]->endTime != -1)){
				cout << "THIS IS A PROBLEM! " << graph[i]->adj[j]->communityId << ", " << graph[i]->adj[j]->startTime << ", " << graph[i]->adj[j]->endTime << endl << flush;
				cout << "Edge: " << graph[i]->adj[j]->sourceId << ", " << graph[i]->adj[j]->destId << endl << flush;
				exit(1);
			}
			if (graph[i]->adj[j]->startTime == graph[i]->adj[j]->endTime) continue;
			if (graph[i]->adj[j]->startTime > 0)
				stream.push_back(update(2, i, graph[i]->adj[j]->destId, graph[i]->adj[j]->startTime));
			if (graph[i]->adj[j]->endTime > 0)
				stream.push_back(update(0, i, graph[i]->adj[j]->destId, graph[i]->adj[j]->endTime));
		}
	}
	sort(stream.begin(),stream.end(),compareUpdate);
	ofstream myfile;
	myfile.open("ckbDynamicStream");
	for (int i=0;i<stream.size();i++)
	if (stream[i].t <= T)
		myfile << stream[i].updateType << "," << stream[i].u << "," << stream[i].v << "," << stream[i].t << endl;
	myfile.close();
}

void printInitialGraph(){
	ofstream myfile, myfileC;
	myfile.open("ckbDynamicInitialGraphEdgeList");
	myfileC.open("ckbDynamicInitialCoverEdgeList");
	for (int i=0;i<N1;i++){
		for (int j=0; j < (graph[i]->adj).size(); j++){
			if (((graph[i]->adj[j])->communityId == -1)||((graph[i]->adj[j])->communityId == -4)) continue;
			if (((graph[i]->adj[j])->startTime <=0) && ((graph[i]->adj[j])->endTime >= -1 )){
				myfile << i << "\t" << graph[i]->adj[j]->destId << endl;
			}
		}
		for (int j=0;j<graph[i]->communities.size();j++){
			int communityId = graph[i]->communities[j];
			community c = *communities[communityId];
			int pos = c.indexOfNode(i);
			if (pos!=-1){
				if ((c.nodeList[pos]->joinTime >= 0) && (c.nodeList[pos]->leaveTime >= -1))
					myfileC << i << "\t" << communityId << endl;
			}
		}
	}
	myfile.close();
	myfileC.close();
}

void printAFOCSGraphAndStream(){
	ofstream myfile;
	myfile.open("AFOCS/tempGraph_0.txt");
	for (int i=0;i<N1;i++){
		for (int j=0; j < (graph[i]->adj).size(); j++){
			if (((graph[i]->adj[j])->communityId == -1)||((graph[i]->adj[j])->communityId == -4)) continue;
			if (((graph[i]->adj[j])->startTime <=0) && ((graph[i]->adj[j])->endTime >= -1 )){
				myfile << (i + 1) << " " << ((graph[i]->adj[j])->destId + 1) << endl;
			}
		}
	}
	myfile.close();
	for (int t = 0; t <= T; t++){
		std::ostringstream strs;
		strs << "AFOCS/tempGraph_";
		strs << t;
		strs << ".txt";
		std::string outfilename = strs.str();
		//string outfilename =  to_string(alpha);
		const char* conv_outfilename = outfilename.c_str();
		myfile.open(conv_outfilename);
		for (int i = 0; i < N1; i++){
			for (int j = 0; j < (graph[i]->adj).size(); j++){
				if (((graph[i]->adj[j])->communityId == -1)||((graph[i]->adj[j])->communityId == -4)) continue;
				if ((graph[i]->adj[j])->startTime == t){
					myfile << (i + 1) << " " << ((graph[i]->adj[j])->destId + 1) << endl;
				}
				if ((graph[i]->adj[j])->endTime == t){
					myfile << "-" << (i + 1) << " " << ((graph[i]->adj[j])->destId + 1) << endl;
				}
			}
		}
		myfile.close();
	}
	myfile.open("AFOCS/tempGraphComm.txt");
	for (int i = 0; i < N2; i++){
		community c = *communities[i];
		bool flag = false;
		for (int j = 0; j < c.nodeList.size(); j++){
			if ((c.nodeList[j]->joinTime <= T) && (c.nodeList[j]->leaveTime == -1)){
				flag = true;
				myfile << (i + 1) << " ";
			}
			if (flag) myfile << endl;
		}
	}
	myfile.close();
}

int main(int argc, char *argv[]){
	if (argc < 9){
		cout << "Usage: ./ckbDynamicNodeSet numberOfNodes minCommunitySize maxCommunitySize minCommunityMembership ";
		cout << "maxCommunityMembership eventProbability intraCommunityEdgeProbability epsilon" << endl;
		exit(0);
	}
	N1 = atoi(argv[1]);
	mmin = atoi(argv[2]);
	mmax = atoi(argv[3]);
	xmin = atoi(argv[4]);
	xmax = atoi(argv[5]);
	probEvent = atof(argv[6]);
	alpha = atof(argv[7]);
	eps = atof(argv[8]);

	srand(time(NULL));
	struct timespec start, finish, mid;
	double elapsedS = 0, elapsedD = 0;
	clock_gettime(CLOCK_MONOTONIC, &start);
	generateStaticStructure();
	clock_gettime(CLOCK_MONOTONIC, &mid);

	for (int t = 0; t< T;t++){
		generateTimeslot(t);
	}
	clock_gettime(CLOCK_MONOTONIC, &finish);
	elapsedS = (mid.tv_sec - start.tv_sec) + (mid.tv_nsec - start.tv_nsec)/pow(10,9);
	elapsedD = (finish.tv_sec - start.tv_sec) + (finish.tv_nsec - start.tv_nsec)/pow(10,9);
	printGraph();
	printCommunity();

	printInitialGraph();
	printGraphStream();
	//printAFOCSGraphAndStream();
	averageBirthTime /= nBirths;
	averageDeathTime /= nDeaths;
	averageSplitTime /= nSplits;
	averageMergeTime /= nMerges;
	averageNodeAddTime /= nAdd;
	averageNodeDeleteTime /= nDelete;
	cout << "#<NumberOfNodes> <ProbEvent> <Alpha> <Eps> <StaticGenerationTime> <DynamicGenerationTime>" << endl;
	cout << N1 << "\t" << probEvent << "\t" << alpha << "\t" << eps << "\t" << elapsedS << "\t" << elapsedD << endl;
}
