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
 * Last port: simplify/TopologyPreservingSimplifier.java rev. 1.4 (JTS-1.7)
 *
 **********************************************************************/

#include <geos/simplify/TopologyPreservingSimplifier.h>
#include <geos/simplify/TaggedLinesSimplifier.h>
#include <geos/simplify/LineSegmentIndex.h> // for auto_ptr dtor
#include <geos/simplify/TaggedLineString.h>
#include <geos/simplify/TaggedLineStringSimplifier.h> // for auto_ptr dtor
#include <geos/algorithm/LineIntersector.h> // for auto_ptr dtor
// for LineStringTransformer inheritance
#include <geos/geom/util/GeometryTransformer.h>
// for LineStringMapBuilderFilter inheritance
#include <geos/geom/GeometryComponentFilter.h>
#include <geos/geom/Geometry.h> // for auto_ptr dtor
#include <geos/geom/LineString.h>
#include <geos/geom/LinearRing.h>
#include <geos/util/IllegalArgumentException.h>

#include <memory> // for auto_ptr
#include <map>
#include <cassert>
#include <iostream>

#ifndef GEOS_DEBUG
#define GEOS_DEBUG 0
#endif

#ifdef GEOS_DEBUG
#include <iostream>
#endif

using namespace geos::geom;

namespace geos {
namespace simplify { // geos::simplify

typedef std::map<const geom::Geometry*, TaggedLineString* > LinesMap;


namespace { // module-statics

class LineStringTransformer: public geom::util::GeometryTransformer
{

public:

	friend class TopologyPreservingSimplifier;

	/**
	 * User's constructor.
	 * @param nMap - reference to LinesMap instance.
	 *
	 * \todo XXX - mloskot: I temporarily moved this ctor to public section,
	 * becuase GEOS did not compile at all:
	 * http://logs.qgis.org/postgis/%23postgis.2006-04-21.log
	 */
	LineStringTransformer(LinesMap& simp);
	
protected:

	CoordinateSequence::AutoPtr transformCoordinates(
			const CoordinateSequence* coords,
			const Geometry* parent);
	
private:	

	LinesMap& linestringMap;

};

/*private*/
LineStringTransformer::LineStringTransformer(LinesMap& nMap)
	:
	linestringMap(nMap)
{
}

/*protected*/
CoordinateSequence::AutoPtr
LineStringTransformer::transformCoordinates(
		const CoordinateSequence* coords,
		const Geometry* parent)
{
#if GEOS_DEBUG
	std::cerr << __FUNCTION__ << ": parent: " << parent
	          << std::endl;
#endif
	if ( dynamic_cast<const LineString*>(parent) )
	{
		LinesMap::iterator it = linestringMap.find(parent);
		assert( it != linestringMap.end() );
		
		TaggedLineString* taggedLine = it->second;
#if GEOS_DEBUG
		std::cerr << "LineStringTransformer[" << this << "] "
		     << " getting result Coordinates from "
		     << " TaggedLineString[" << taggedLine << "]"
		     << std::endl;
#endif

		assert(taggedLine);
		assert(taggedLine->getParent() == parent);

		return taggedLine->getResultCoordinates();
	}

	// for anything else (e.g. points) just copy the coordinates
	return GeometryTransformer::transformCoordinates(coords, parent);
}

//----------------------------------------------------------------------

/*
 * This class populates the given LineString=>TaggedLineString map
 * with newly created TaggedLineString objects.
 * Users must take care of deleting the map's values (elem.second).
 * Would be nice if auto_ptr<> worked in a container, but it doesn't :(
 *
 * mloskot: So, let's write our own "shared smart pointer" or better ask
 * PCS about using Boost's shared_ptr.
 *
 */
class LineStringMapBuilderFilter: public geom::GeometryComponentFilter
{

public:

	friend class TopologyPreservingSimplifier;

	void filter_ro(const Geometry* geom);


	/**
	 * User's constructor.
	 * @param nMap - reference to LinesMap instance.
	 *
	 * \todo XXX - mloskot: I temporarily moved this ctor to public section,
	 * becuase GEOS did not compile at all:
	 * http://logs.qgis.org/postgis/%23postgis.2006-04-21.log
	 */
	LineStringMapBuilderFilter(LinesMap& nMap);

private:

	LinesMap& linestringMap;

};

/*private*/
LineStringMapBuilderFilter::LineStringMapBuilderFilter(LinesMap& nMap)
	:
	linestringMap(nMap)
{
}

/*public*/
void
LineStringMapBuilderFilter::filter_ro(const Geometry* geom)
{
	TaggedLineString* taggedLine;

	if ( const LinearRing* lr =
			dynamic_cast<const LinearRing*>(geom) )
	{
		taggedLine = new TaggedLineString(lr, 4);

	}
	else if ( const LineString* ls = 
			dynamic_cast<const LineString*>(geom) )
	{
		taggedLine = new TaggedLineString(ls, 2);
	}
	else
	{
		return;
	}

	// Duplicated Geometry pointers shouldn't happen
	if ( ! linestringMap.insert(std::make_pair(geom, taggedLine)).second )
	{
		std::cerr << __FILE__ << ":" << __LINE__ 
		     << "Duplicated Geometry components detected"
		     << std::endl;

		delete taggedLine;
	}
}


} // end of module-statics

/*public static*/
std::auto_ptr<geom::Geometry>
TopologyPreservingSimplifier::simplify(
		const geom::Geometry* geom,
		double tolerance)
{
	TopologyPreservingSimplifier tss(geom);
        tss.setDistanceTolerance(tolerance);
	return tss.getResultGeometry();
}

/*public*/
TopologyPreservingSimplifier::TopologyPreservingSimplifier(const Geometry* geom)
	:
	inputGeom(geom),
	lineSimplifier(new TaggedLinesSimplifier())
{
}

/*public*/
void
TopologyPreservingSimplifier::setDistanceTolerance(double d)
{
	using geos::util::IllegalArgumentException;

	if ( d < 0.0 )
		throw IllegalArgumentException("Tolerance must be non-negative");

	lineSimplifier->setDistanceTolerance(d);
}

/*public*/
std::auto_ptr<geom::Geometry> 
TopologyPreservingSimplifier::getResultGeometry()
{
	LinesMap linestringMap;

	std::auto_ptr<geom::Geometry> result;

	try {
		LineStringMapBuilderFilter lsmbf(linestringMap);
		inputGeom->apply_ro(&lsmbf);

#if GEOS_DEBUG
	std::cerr << "LineStringMapBuilderFilter applied, "
	          << " lineStringMap contains "
	          << linestringMap.size() << " elements"
	          << std::endl;
#endif


		for (LinesMap::iterator
			it=linestringMap.begin(), itEnd=linestringMap.end();
			it != itEnd;
			++it)
		{
			lineSimplifier->simplifyLine(it->second); 
		}

#if GEOS_DEBUG
	std::cerr << "all TaggedLineString simplified"
	          << std::endl;
#endif

		LineStringTransformer trans(linestringMap);
		result = trans.transform(inputGeom);

#if GEOS_DEBUG
	std::cerr << "inputGeom transformed"
	          << std::endl;
#endif

	} catch (...) {
		for (LinesMap::iterator
				it = linestringMap.begin(),
				itEnd = linestringMap.end();
				it != itEnd;
				++it)
		{
			delete it->second;
		}

		throw;
	}

	for (LinesMap::iterator
			it = linestringMap.begin(),
			itEnd = linestringMap.end();
			it != itEnd;
			++it)
	{
		delete it->second;
	}

#if GEOS_DEBUG
	std::cerr << "returning result"
	          << std::endl;
#endif

	return result;
}

} // namespace geos::simplify
} // namespace geos

/**********************************************************************
 * $Log$
 * Revision 1.5  2006/04/22 17:16:31  mloskot
 * Temporar fix of Bug #100. This report requires deeper analysis!.
 *
 * Revision 1.4  2006/04/13 21:52:35  strk
 * Many debugging lines and assertions added. Fixed bug in TaggedLineString class.
 *
 * Revision 1.3  2006/04/13 16:04:10  strk
 * Made TopologyPreservingSimplifier implementation successfully build
 *
 * Revision 1.2  2006/04/13 14:25:17  strk
 * TopologyPreservingSimplifier initial port
 *
 **********************************************************************/