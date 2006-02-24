/**********************************************************************
 * $Id$
 *
 * GEOS - Geometry Engine Open Source
 * http://geos.refractions.net
 *
 * Copyright (C) 2006 Refractions Research Inc.
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU Lesser General Licence as published
 * by the Free Software Foundation. 
 * See the COPYING file for more information.
 *
 **********************************************************************
 *
 * Last port: noding/snapround/MCIndexSnapRounder.java rev. 1.1 (JTS-1.7)
 *
 **********************************************************************/

#include <functional>
#include "geos/nodingSnapround.h"

namespace geos {
namespace noding { // geos.noding
namespace snapround { // geos.noding.snapround

/*private*/
void
MCIndexSnapRounder::findInteriorIntersections(MCIndexNoder& noder,
		SegmentString::NonConstVect* segStrings,
		vector<Coordinate>& intersections)
{
	IntersectionFinderAdder intFinderAdder(li, intersections);
	noder.setSegmentIntersector(&intFinderAdder);
	noder.computeNodes(segStrings);
}

/* private */
void
MCIndexSnapRounder::computeIntersectionSnaps(vector<Coordinate>& snapPts)
{
	for (vector<Coordinate>::iterator
			it=snapPts.begin(), itEnd=snapPts.end();
			it!=itEnd;
			++it)
	{
		Coordinate& snapPt = *it;
		HotPixel hotPixel(snapPt, scaleFactor, li);
		pointSnapper->snap(hotPixel);
	}
}

/*private*/
void
MCIndexSnapRounder::computeEdgeVertexSnaps(SegmentString* e)
{
	CoordinateSequence& pts0 = *(e->getCoordinates());
	for (unsigned int i=0, n=pts0.size()-1; i<n; ++i)
	{
		HotPixel hotPixel(pts0[i], scaleFactor, li);
		bool isNodeAdded = pointSnapper->snap(hotPixel, e, i);
		// if a node is created for a vertex, that vertex must be noded too
		if (isNodeAdded) {
			e->addIntersection(pts0[i], i);
		}
	}
}

/*public*/
void
MCIndexSnapRounder::computeVertexSnaps(SegmentString::NonConstVect& edges)
{
	for_each(edges.begin(), edges.end(), bind1st(mem_fun(&MCIndexSnapRounder::computeEdgeVertexSnaps), this));
}

/*private*/
void
MCIndexSnapRounder::snapRound(MCIndexNoder& noder, 
		SegmentString::NonConstVect* segStrings)
{
	vector<Coordinate> intersections;
 	findInteriorIntersections(noder, segStrings, intersections);
	computeIntersectionSnaps(intersections);
	computeVertexSnaps(*segStrings);
	
}

/*public*/
void
MCIndexSnapRounder::computeNodes(SegmentString::NonConstVect* inputSegmentStrings)
{
	nodedSegStrings = inputSegmentStrings;
	MCIndexNoder noder;
	pointSnapper.reset(new MCIndexPointSnapper(noder.getIndex()));
	snapRound(noder, inputSegmentStrings);

	// testing purposes only - remove in final version
	//checkCorrectness(inputSegmentStrings);
}

/*private*/
void
MCIndexSnapRounder::checkCorrectness(SegmentString::NonConstVect& inputSegmentStrings)
{
	SegmentString::NonConstVect resultSegStrings;
	SegmentString::getNodedSubstrings(inputSegmentStrings, &resultSegStrings);
	NodingValidator nv(resultSegStrings);
	try {
		nv.checkValid();
	} catch (const exception& ex) {
		cerr<<ex.what()<<endl;
	}
}


} // namespace geos.noding.snapround
} // namespace geos.noding
} // namespace geos

/**********************************************************************
 * $Log$
 * Revision 1.5  2006/02/23 11:54:20  strk
 * - MCIndexPointSnapper
 * - MCIndexSnapRounder
 * - SnapRounding BufferOp
 * - ScaledNoder
 * - GEOSException hierarchy cleanups
 * - SpatialIndex memory-friendly query interface
 * - GeometryGraph::getBoundaryNodes memory-friendly
 * - NodeMap::getBoundaryNodes memory-friendly
 * - Cleanups in geomgraph::Edge
 * - Added an XML test for snaprounding buffer (shows leaks, working on it)
 *
 * Revision 1.4  2006/02/21 16:53:49  strk
 * MCIndexPointSnapper, MCIndexSnapRounder
 *
 * Revision 1.3  2006/02/19 19:46:49  strk
 * Packages <-> namespaces mapping for most GEOS internal code (uncomplete, but working). Dir-level libs for index/ subdirs.
 *
 * Revision 1.2  2006/02/18 21:08:09  strk
 * - new CoordinateSequence::applyCoordinateFilter method (slow but useful)
 * - SegmentString::getCoordinates() doesn't return a clone anymore.
 * - SegmentString::getCoordinatesRO() obsoleted.
 * - SegmentString constructor does not promises constness of passed
 *   CoordinateSequence anymore.
 * - NEW ScaledNoder class
 * - Stubs for MCIndexPointSnapper and  MCIndexSnapRounder
 * - Simplified internal interaces of OffsetCurveBuilder and OffsetCurveSetBuilder
 *
 * Revision 1.1  2006/02/14 13:28:26  strk
 * New SnapRounding code ported from JTS-1.7 (not complete yet).
 * Buffer op optimized by using new snaprounding code.
 * Leaks fixed in XMLTester.
 *
 **********************************************************************/