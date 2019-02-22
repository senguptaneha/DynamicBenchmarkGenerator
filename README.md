Compile using :
```
make
```

Run using :
```
./ckbDynamicNodeSet <numberOfNodes> <minCommunitySize> <maxCommunitySize> <minCommunityMembership> <maxCommunityMembership> <eventProbability> <intraCommunityEdgeProbability> <epsilon>
```

## Output files:
* ckbDynamicInitialGraphEdgeList : Graph G_0, one edge per line <src>\t<dst>
* ckbDynamicInitialCoverEdgeList : Cover of G_0. One edge of the node community bigraph per line <nodeId>\t<communityId>
* ckbDynamicStream : Stream of events to the dynamic graph. One event per line <eventType>,<src>,<dst>,<timestamp>
	* eventType  - 0: edge delete; 1: node add; 2: edge add; 3: node delete;
	* src - source node
	* dst - destination node (-1 if this is a node add or node delete event
	* timestamp - 1 <= timestamp <= T
*ckbDynamicGraphByNode: Returns all the incident edges of a node. Lines following a line of the form Node : i indicate edges incident on i, and are of the form
(dst, start, end, communityId)
	* dst - is the other node involved in the edge
	* start - start time of the edge
	* end - end time of the edge (-1 if infinity)
	* communityId -   community Id in which edge was generated. When intra-community edge (u,v) is generated, the edge (v,u) is inserted as a reverse edge with community Id -1 since this is an undirected graph. When an epsilon edge (u,v) is generated, its community Id is -2, and the corresponding reverse edge has community Id -4.
* ckbDynamicCoverByCommunity : Lists all nodes in a community. Lines are of the form
	a. communityId:(nodeId, startTime, endTime) or
	b. (nodeId, startTime, endTime) : Here the nodeId belongs to the community identified in the latest line of type (a)
* ckbDynamicCoverByNode : Lists all communities of a node. Lines are of the form
	a. nodeId:(communityId, startTime, endTime) or
	b. (communityId, startTime, endTime) : Here the communityId contains the node identified in the latest line of type (a)

