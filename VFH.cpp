#include "VFH.h"
#include <iomanip>

using namespace Eigen;
namespace vfh_star
{

VFH::VFH(const envire::Grid< Traversability >* trGrid)
{
    traversabillityGrid = trGrid;
    
    senseRadius = 0.9;
    histogramSize = 90;
    narrowThreshold = 10;
}



void addDir(std::vector< std::pair<double, double> > &drivableDirections, double angularResoultion, int narrowThreshold, int start, int end, int histSize)
{
    std::cout << "adding area " << start << " end " << end << " is narrow " << (abs(end-start) < narrowThreshold) <<  std::endl;
    
    int gapSize = end-start;
    
    if(gapSize < 0)
	gapSize += histSize;
	
    if(gapSize < narrowThreshold) {
	double middle = (start + gapSize / 2.0);
	if(middle > histSize)
	    middle -= histSize;
	
	double dir = middle * angularResoultion;
	drivableDirections.push_back(std::make_pair(dir, dir));
    } else {
	double left = start * angularResoultion;
	double right = end * angularResoultion;
	drivableDirections.push_back(std::make_pair(left, right));
    }    
}

std::vector< std::pair<double, double> > VFH::getNextPossibleDirections(const base::Pose& curPose, const double& obstacleSafetyDist, const double& robotWidth, vfh_star::VFHDebugData* dd) const
{
    std::vector< std::pair<double, double> > drivableDirections;
    std::vector<double> histogram;
    std::vector<bool> bHistogram;

    //4 degree steps
    histogram.resize(histogramSize);

    double angularResoultion = 2*M_PI / histogram.size();
    
    generateHistogram(histogram, curPose, senseRadius, obstacleSafetyDist, robotWidth / 2.0);

    //we ignore one obstacle
    getBinaryHistogram(histogram, bHistogram, 2, 3);
    
    int start = -1;
    int end = -1;
    int firstEnd = bHistogram.size();
    for(unsigned int i = 0; i < bHistogram.size(); i++) 
    {
	if(bHistogram[i] && start < 0)   
	{	    
	    std::cout << "found start " << i << std::endl; 
	    start = i;
	}
	
	if(!bHistogram[i] && start >= 0)
	{
	    std::cout << "found end " << i << std::endl; 

	    if(start == 0)
	    {
		std::cout << "found end of first segement " << i << std::endl; 
		firstEnd = i-1;
		start = -1;
		continue;
	    }
	    
	    end = i-1;
	    
	    addDir(drivableDirections, angularResoultion, narrowThreshold, start, end, histogramSize);
	    start = -1;
	}   
    }
    
    if(start >= 0)
    {
	std::cout << "have start " << start << std::endl; 

	addDir(drivableDirections, angularResoultion, narrowThreshold, start, firstEnd, histogramSize);
    }
    else 
    {
	if(firstEnd != bHistogram.size())
	{
	    addDir(drivableDirections, angularResoultion, narrowThreshold, 0, firstEnd, histogramSize);
	}
    }
    
    std::cout << "Resulting Areas: " << std::endl;
    for(std::vector<std::pair<double, double> >::const_iterator it = drivableDirections.begin(); it != drivableDirections.end(); it++)
    {
	std::cout << "Start " << it->first / M_PI * 180 << " end " << it->second  / M_PI * 180<< std::endl;
    }
    
    if(dd) 
    {
	dd->histogram = bHistogram;
	dd->obstacleSafetyDist = obstacleSafetyDist;
	dd->pose = curPose;
	dd->robotWidth = robotWidth;
	dd->senseRadius = senseRadius;
    }
    
    return drivableDirections;
}

double normalize(double ang)
{
    if(ang < 0)
	return ang + 2*M_PI;
    
    if(ang > 2*M_PI)
	return ang - 2*M_PI;
    
    return ang;
}

void VFH::generateHistogram(std::vector< double >& histogram, const base::Pose& curPose, double senseRadius, double obstacleSafetyDist, double robotRadius) const
{
    const envire::FrameNode *gridPos = traversabillityGrid->getFrameNode();
    
    int nrDirs = histogram.size();
    
    double dirResolution = 2*M_PI / nrDirs;
    
    const double a = 2.0;
    const double b = 1.0/ (senseRadius * senseRadius);

    
    std::vector<double> &dirs(histogram);    
    
    //calculate robot pos in grid coordinates
    const Vector3d toCorner(traversabillityGrid->getWidth() / 2 * traversabillityGrid->getScaleX(), traversabillityGrid->getHeight() / 2 * traversabillityGrid->getScaleY(), 0);   
    const Vector3d cornerPos = gridPos->getTransform().translation() - toCorner;
    const Vector3d robotPosInGrid = curPose.position - cornerPos;
    const envire::Grid<Traversability>::ArrayType &gridData = traversabillityGrid->getGridData();
    const int robotX = robotPosInGrid.x() / traversabillityGrid->getScaleX();
    const int robotY = robotPosInGrid.y() / traversabillityGrid->getScaleY();
    
    std::cout << "robotPos " << curPose.position.transpose() << std::endl;
    std::cout << "robotPos in Grid" << robotPosInGrid.transpose() << std::endl;
    std::cout << "RobotX " << robotX << " Y " << robotY << std::endl;
    
    //TODO what happens if scale x an scale y differ...
    int senseSize = senseRadius / traversabillityGrid->getScaleX();
    
    std::cout << "senseSize " << senseSize << std::endl;
    
/*  Debug code for printing local obstacle map  

    std::cout <<  std::endl;

    for(int y = senseSize; y >= -senseSize; y--)
    {
	std::cout << std::setw(4) << y;
	for(int x = -senseSize; x <= senseSize; x++)
	{
	    int rx = robotX + x;
	    int ry = robotY + y;
	    
	    if(!traversabillityGrid->inGrid(rx, ry))
		continue;

	    //go safe, if we do not know anything about something, it is an obstacle
	    if(gridData[rx][ry] == OBSTACLE)
		std::cout <<  "O";		
	    else
		std::cout <<  "T";
		
	}
	std::cout <<  std::endl;
    }*/
    
    //walk over area of grid within of circle with radius senseRadius around the robot
    for(int x = -senseSize; x <= senseSize; x++)
    {
	for(int y = -senseSize; y <= senseSize; y++)
	{
	    double distToRobot = sqrt(x*x + y*y) * traversabillityGrid->getScaleX();
	    
// 	    std::cout << "distToRobot " << distToRobot << std::endl;
	    
	    //check if outside circle
	    if(distToRobot > senseRadius)
		continue;
	  
	    int rx = robotX + x;
	    int ry = robotY + y;
	    
// 	    std::cout << "rx " << rx << " ry " << ry << std::endl;

	    //go safe, if we do not know anything about something, it is an obstacle
	    if(!traversabillityGrid->inGrid(rx, ry) || (gridData[rx][ry] != TRAVERSABLE && gridData[rx][ry] != UNCLASSIFIED))
	    {
		if(!traversabillityGrid->inGrid(rx, ry))
		{
		    std::cout << "not in Grid exit" <<std::endl;
		    exit(0);
		}
		
		double angleToObstace = atan2(y, x);
		
		//convert to ENU
		angleToObstace -= M_PI / 2.0;
		
		//move to range 0 to 2*M_PI
		angleToObstace = normalize(angleToObstace);
		
		//calculate magnitude m = c*(a-b*d²)
		//ci is 1
		double magnitude = a - b * distToRobot*distToRobot;

// 		std::cout << "Magnitude is " << magnitude << std::endl;
		
		//in case we are allready hit the obstacle, we set distToRobot
		//to (robotWidth + obstacleSafetyDist) which results in a 90 degree masking
		//of the area where the collided obstable is
		if((robotRadius + obstacleSafetyDist) > distToRobot)
		    distToRobot = (robotRadius + obstacleSafetyDist);
		
		//boundary of obstacle including robot width and safety distance
		double y_t = asin((robotRadius + obstacleSafetyDist) / distToRobot);
		
		//add to histogramm
		int s = (angleToObstace - y_t) / dirResolution;
		int e = (angleToObstace + y_t) / dirResolution;
		for(int a = s; a <= e; a++) {
		    int ac = a;
		    if(ac < 0)
			ac += nrDirs;
		    
		    if(ac >= nrDirs)
			ac -=nrDirs;
		    
		    dirs[ac] += magnitude;
		}
	    }
	}
    }
}


void VFH::getBinaryHistogram(const std::vector< double >& histogram, std::vector< bool >& binHistogram, double lowThreshold, double highThreshold) const
{
    binHistogram.clear();
    binHistogram.reserve(histogram.size());
    for(std::vector<double>::const_iterator it = histogram.begin(); it != histogram.end(); it++) {
	bool drivable = false;
	
	if(*it <= lowThreshold)
	    drivable = true;
	
	//FIXME we are missing out the last binary histogramm
	
	binHistogram.push_back(drivable);
    }
}
}