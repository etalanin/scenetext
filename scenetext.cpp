#include "opencv2/highgui/highgui.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "opencv2/contrib/contrib.hpp"

#include <iostream>
#include <fstream>
#include <sstream>

using namespace cv;
using namespace std;

int GroundTruth(Mat& _originalImage, bool showImage = false)
{
    double t = (double)getTickCount();

    Mat originalImage(_originalImage.rows + 2, _originalImage.cols + 2, _originalImage.type());
    copyMakeBorder(_originalImage, originalImage, 1, 1, 1, 1, BORDER_CONSTANT, Scalar(255, 255, 255));

    Mat bwImage(originalImage.size(), CV_8UC1);

    uchar thresholdValue = 100;
    uchar maxValue = 255;
    uchar middleValue = 192;
    uchar zeroValue = 0;
    Scalar middleScalar(middleValue);
    Scalar zeroScalar(zeroValue);

    static int neigborsCount = 4;
    static int dx[] = {-1,  0, 0, 1};
    static int dy[] = { 0, -1, 1, 0};
    int di, rx, ry;
    int perimeter;

    cvtColor(originalImage, bwImage, CV_RGB2GRAY);
    threshold(bwImage, bwImage, thresholdValue, maxValue, THRESH_BINARY_INV);

    int regionsCount = 0;
    int totalPixelCount = bwImage.rows * bwImage.cols;
    Point seedPoint;
    Rect rectFilled;
    int valuesSum, q1, q2, q3;
    bool p00, p10, p01, p11;

    for(int i = 0; i < totalPixelCount; i++)
    {
        if (bwImage.data[i] == maxValue)
        {
            seedPoint.x = i % bwImage.cols;
            seedPoint.y = i / bwImage.cols;

            if ((seedPoint.x == 0) || (seedPoint.y == 0) || (seedPoint.x == bwImage.cols - 1) || (seedPoint.y == bwImage.rows - 1))
            {
                continue;
            }

            regionsCount++;

            size_t pixelsFilled = floodFill(bwImage, seedPoint, middleScalar, &rectFilled);
            printf("New region: %d\n", regionsCount);
            // We use -1 here since image was expanded by 1 pixel
            printf("Start point: (%d; %d)\n", seedPoint.x - 1, seedPoint.y - 1);
            printf("Area: %d\n", (int)pixelsFilled);
            printf("Bounding box (%d; %d) + (%d; %d)\n", rectFilled.x - 1, rectFilled.y - 1, rectFilled.width, rectFilled.height);

            perimeter = 0;
            q1 = 0; q2 = 0; q3 = 0;

            int crossings[rectFilled.height];
            for(int j = 0; j < rectFilled.height; j++)
            {
                crossings[j] = 0;
            }

            for(ry = rectFilled.y - 1; ry <= rectFilled.y + rectFilled.height; ry++)
            {
                for(rx = rectFilled.x - 1; rx <= rectFilled.x + rectFilled.width; rx++)
                {
                    if ((bwImage.at<uint8_t>(ry, rx - 1) != bwImage.at<uint8_t>(ry, rx)) && (bwImage.at<uint8_t>(ry, rx - 1) + bwImage.at<uint8_t>(ry, rx) == middleValue + zeroValue))
                    {
                        crossings[ry - rectFilled.y]++;
                    }

                    if (bwImage.at<uint8_t>(ry, rx) == middleValue)
                    {
                        for(di = 0; di < neigborsCount; di++)
                        {
                            int xNew = rx + dx[di];
                            int yNew = ry + dy[di];

                            if (bwImage.at<uint8_t>(yNew, xNew) == zeroValue)
                            {
                                perimeter++;
                            }
                        }
                    }

                    p00 = bwImage.at<uint8_t>(ry, rx) == middleValue;
                    p01 = bwImage.at<uint8_t>(ry, rx + 1) == middleValue;
                    p10 = bwImage.at<uint8_t>(ry + 1, rx) == middleValue;
                    p11 = bwImage.at<uint8_t>(ry + 1, rx + 1) == middleValue;
                    valuesSum = p00 + p01 + p10 + p11;

                    if (valuesSum == 1) q1++; else
                    if (valuesSum == 3) q2++; else
                    if ((valuesSum == 2) && (p00 == p11)) q3++;
                }
            }

            q1 = q1 - q2 + 2 * q3;
            if (q1 % 4 != 0)
            {
                printf("Non-integer Euler number");
                exit(0);
            }
            q1 /= 4;

            printf("Perimeter: %d\n", (int)perimeter);
            printf("Euler number: %d\n", q1);
            printf("Crossings: ");
            for(int j = 0; j < rectFilled.height; j++)
            {
                printf("%d ", crossings[j]);
            }

            printf("\n=====\n\n");

            floodFill(bwImage, seedPoint, zeroScalar);

            rectangle(originalImage, rectFilled, zeroScalar);
        }
    }

    t = (double)getTickCount() - t;
    printf("Working time: %g ms\n", t * 1000. / getTickFrequency());

    if (showImage)
    {
        imshow("Truth", originalImage);
        waitKey();
    }
}












class Region
{
private:
    Point start;
    Rect bounds;
    int area;
    int perimeter;
    int euler;
    int imageh;
    int* crossings;

public:
    Region()
    {

    }

    Region(Point _p, int h)
    {
        start = _p;
        bounds.x = _p.x;
        bounds.y = _p.y;
        bounds.width = 1;
        bounds.height = 1;
        area = 1;
        perimeter = 4;
        euler = 0;
        imageh = h;
        crossings = new int[imageh];
        for(int i = 0; i < imageh; i++)
        {
            crossings[i] = 0;
        }
        crossings[_p.y] = 2;
    }

    void Attach(Region* _extra, int _borderLength, int* _crossings = NULL)
    {
        if (start != _extra->start)
        {
            bounds |= _extra->bounds;
            area += _extra->area;
            perimeter += _extra->perimeter - 2 * _borderLength;
            euler += _extra->euler;
            if (_crossings)
            {
                for(int i = 0; i < imageh; i++)
                {
                    crossings[i] += _extra->crossings[i];
                    crossings[i] += _crossings[i];
                }
            }
        }
    }

    void CorrectEuler(int _delta)
    {
        euler += _delta;
    }

    Rect Bounds()
    {
        return bounds;
    }

    Point Start()
    {
        return start;
    }

    int Area()
    {
        return area;
    }

    int Perimeter()
    {
        return perimeter;
    }

    int Euler()
    {
        return euler;
    }

    int* Crossings()
    {
        return crossings;
    }

    int BoundsArea()
    {
        return bounds.width * bounds.height;
    }
};

Point uf_Find(Point _x, Point** _parents)
{
    if (_parents[_x.x][_x.y].x == -1)
    {
        return Point(-1, -1);
    }

    if (_parents[_x.x][_x.y] != _x)
    {
        return uf_Find(_parents[_x.x][_x.y], _parents);
    }

    return _x;
}

void MatasLike(Mat& originalImage, bool showImage = false)
{
    double t = (double)getTickCount();

    Mat bwImage(originalImage.size(), CV_8UC1);

    map<int, vector<Point> > pointLevels;
    Point pc;

    static int neighborsCount = 4;
    static int dx[] = {-1,  0, 0, 1};
    static int dy[] = { 0, -1, 1, 0};
    int di;

    int i, j, k;

    cvtColor(originalImage, bwImage, CV_RGB2GRAY);

    int** ranksArray;
    ranksArray = new int*[bwImage.cols];
    for(i = 0; i < bwImage.cols; i++)
    {
        ranksArray[i] = new int[bwImage.rows];
    }

    Point** parentsArray;
    parentsArray = new Point*[bwImage.cols];
    for(i = 0; i < bwImage.cols; i++)
    {
        parentsArray[i] = new Point[bwImage.rows];
        for(j = 0; j < bwImage.rows; j++)
        {
            parentsArray[i][j] = Point(-1, -1);
        }
    }

    Region*** regionsArray;
    regionsArray = new Region**[bwImage.cols];
    for(i = 0; i < bwImage.cols; i++)
    {
        regionsArray[i] = new Region*[bwImage.rows];
        for(j = 0; j < bwImage.rows; j++)
        {
            regionsArray[i][j] = NULL;
        }
    }



    // Filling pointLevels
    for(i = 0; i < bwImage.cols; i++)
    {
        for(j = 0; j < bwImage.rows; j++)
        {
            pc.x = i;
            pc.y = j;
            pointLevels[bwImage.at<uint8_t>(pc)].push_back(pc);
        }
    }

    printf("pointLevels size: %d.\n", (int)pointLevels.size());

    static int thresh_start = 0;
    static int thresh_end = 101;
    static int thresh_step = 1;

    for(int thresh = thresh_start; thresh < thresh_end; thresh += thresh_step)
    {
        for(k = 0; k < pointLevels[thresh].size(); k++)
        {
            Point p0 = pointLevels[thresh][k];

            // Surely point when accessed for the first time is not in any region
            // Setting parent, rank, creating region (uf_makeset)
            parentsArray[p0.x][p0.y] = p0;
            ranksArray[p0.x][p0.y] = 0;

            Region _r(pointLevels[thresh][k], originalImage.rows);
            regionsArray[p0.x][p0.y] = &_r;
            // Surely find will be successful since region we are searching for was just created
            Region* point_region = regionsArray[p0.x][p0.y];
            Point proot = p0;

            bool changed = false;

            bool is_any_neighbor[3][3];
            int q1 = 0, q2 = 0, q3 = 0;
            int q10 = 0, q20 = 0, q30 = 0;
            int qtemp = 0;
            is_any_neighbor[1][1] = false;
            for(int ddx = -1; ddx <= 1; ddx++)
            {
                for(int ddy = -1; ddy <= 1; ddy++)
                {
                    if ((ddx != 0) || (ddy != 0))
                    {
                        is_any_neighbor[ddx+1][ddy+1] = uf_Find(Point(p0.x + ddx, p0.y + ddy), parentsArray) != Point(-1, -1);
                    }

                    if ((ddx >= 0) && (ddy >= 0))
                    {
                        is_any_neighbor[1][1] = false;
                        qtemp = is_any_neighbor[ddx+1][ddy+1] + is_any_neighbor[ddx+1][ddy] + is_any_neighbor[ddx][ddy+1] + is_any_neighbor[ddx][ddy];
                        if (qtemp == 1) q10++; else
                        if (qtemp == 3) q20++; else
                        if ((qtemp == 2) && (is_any_neighbor[ddx+1][ddy+1] == is_any_neighbor[ddx][ddy])) q30++;

                        is_any_neighbor[1][1] = true;
                        qtemp = is_any_neighbor[ddx+1][ddy+1] + is_any_neighbor[ddx+1][ddy] + is_any_neighbor[ddx][ddy+1] + is_any_neighbor[ddx][ddy];
                        if (qtemp == 1) q1++; else
                        if (qtemp == 3) q2++; else
                        if ((qtemp == 2) && (is_any_neighbor[ddx+1][ddy+1] == is_any_neighbor[ddx][ddy])) q3++;
                    }
                }
            }

            qtemp = (q1 - q2 + 2 * q3) - (q10 - q20 + 2 * q30);

            if (qtemp % 4 != 0)
            {
                printf("Non-integer Euler number");
                exit(0);
            }
            qtemp /= 4;

            for(di = 0; di < neighborsCount; di++)
            {
                int x_new = pointLevels[thresh][k].x + dx[di];
                int y_new = pointLevels[thresh][k].y + dy[di];

                // TODO: implement corresponding function?
                if ((x_new < 0) || (y_new < 0) || (x_new >= originalImage.cols) || (y_new >= originalImage.rows))
                {
                    continue;
                }

                if (changed)
                {
                    proot = uf_Find(pointLevels[thresh][k], parentsArray);
                    point_region = regionsArray[proot.x][proot.y];
                    // point_region should exist!
                }

                // p1 is neighbor of point of interest
                Point p1(x_new, y_new);

                if (parentsArray[p1.x][p1.y] != Point(-1, -1))
                {
                    // Entering here means that p1 belongs to some region since has a parent
                    // Will now find root
                    Point p1root = uf_Find(p1, parentsArray);

                    Region* neighbor_region = regionsArray[p1root.x][p1root.y];
                    // TODO: isn't this redundant check? p1 is already in region
                    if (neighbor_region != NULL)
                    {
                        // Need to union. Three cases: rank1>rank2, rank1<rank2, rank1=rank2
                        int point_rank = ranksArray[p0.x][p0.y];
                        int neighbor_rank = ranksArray[p1root.x][p1root.y];

                        Point root_to_find;
                        root_to_find = p1root;

                        bool is_good_neighbor[3][3];
                        int neighborsInRegions = 0;
                        int horizontalNeighbors = 0;

                        for(int ddx = -1; ddx <= 1; ddx++)
                        {
                            for(int ddy = -1; ddy <= 1; ddy++)
                            {
                                if ((ddx != 0) || (ddy != 0))
                                {
                                    is_good_neighbor[ddx+1][ddy+1] = uf_Find(Point(p0.x + ddx, p0.y + ddy), parentsArray) == root_to_find;

                                    if (is_good_neighbor[ddx+1][ddy+1])
                                    {
                                        if (ddy == 0)
                                        {
                                            horizontalNeighbors++;
                                        }

                                        if ((ddy == 0) || (ddx == 0))
                                        {
                                            neighborsInRegions++;
                                        }
                                    }
                                }
                            }
                        }

                        int cr[bwImage.rows];
                        for (int cri = 0; cri < originalImage.rows; cri++)
                        {
                            cr[cri] = 0;
                        }
                        int* crp = &cr[0];
                        crp[p0.y] = -2 * horizontalNeighbors;

                        // uf_union
                        if (point_rank < neighbor_rank)
                        {
                            parentsArray[proot.x][proot.y] = p1root;
                            regionsArray[p1root.x][p1root.y]->Attach(regionsArray[proot.x][proot.y], neighborsInRegions, crp);
                            if (proot != p1root)
                            {
                                // TODO: check if smth is really erased
                                regionsArray[proot.x][proot.y] = NULL;
                                changed = true;
                            }
                        }
                        else if (point_rank > neighbor_rank)
                        {
                            parentsArray[p1root.x][p1root.y] = proot;
                            regionsArray[proot.x][proot.y]->Attach(regionsArray[p1root.x][p1root.y], neighborsInRegions, crp);
                            if (proot != p1root)
                            {
                                // TODO: check if smth is really erased
                                regionsArray[p1root.x][p1root.y] = NULL;
                            }
                        }
                        else
                        {
                            parentsArray[p1root.x][p1root.y] = proot;
                            ranksArray[proot.x][proot.y]++;
                            regionsArray[proot.x][proot.y]->Attach(regionsArray[p1root.x][p1root.y], neighborsInRegions, crp);
                            if (proot != p1root)
                            {
                                // TODO: check if smth is really erased
                                regionsArray[p1root.x][p1root.y] = NULL;
                            }
                        }
                    }
                    else
                    {
                        // Neighbor point not in region. Do nothing
                    }
                }
                else
                {
                    // Neighbor not in region. Doing nothing
                }
            }

            Point p0r = uf_Find(p0, parentsArray);
            regionsArray[p0r.x][p0r.y]->CorrectEuler(qtemp);
        }
        /*
        printf("Threshold: %d. Regions count: %ld.\n", thresh, regions.size());
        for(map<Point, Region, PointComp>::iterator it = regions.begin(); it != regions.end(); it++)
        {
            if (it->second.Area2() > 100)
            {
                //printf("Region bounds: %d %d %d %d\n", it->second.Bounds().x, it->second.Bounds().y, it->second.Bounds().width, it->second.Bounds().height);
                rectangle(bwImage, it->second.Bounds(), Scalar(0, 0, 0));
            }
        }
        */
    }

    t = (double)getTickCount() - t;

    int regionsCount = 0;
    for(i = 0; i < bwImage.cols; i++)
    {
        for(j = 0; j < bwImage.rows; j++)
        {
            if (regionsArray[i][j] != NULL)
            {
                regionsCount++;

                rectangle(originalImage, regionsArray[i][j]->Bounds(), Scalar(0, 0, 0));

                printf("New region: %d\n", regionsCount);
                printf("Area: %d\n", regionsArray[i][j]->Area());
                printf("Bounding box (%d; %d) + (%d; %d)\n", regionsArray[i][j]->Bounds().x, regionsArray[i][j]->Bounds().y, regionsArray[i][j]->Bounds().width, regionsArray[i][j]->Bounds().height);
                printf("Perimeter: %d\n", regionsArray[i][j]->Perimeter());
                printf("Euler number: %d\n", regionsArray[i][j]->Euler());
                printf("Crossings: ");
                for(int jj = regionsArray[i][j]->Bounds().y; jj < regionsArray[i][j]->Bounds().y + regionsArray[i][j]->Bounds().height; jj++)
                {
                    printf("%d ", regionsArray[i][j]->Crossings()[jj]);
                }
                printf("\n");
                printf("=====\n\n");
            }
        }
    }

    printf("Working time: %g ms\n", t * 1000. / getTickFrequency());

    if (showImage)
    {
        imshow("Approach", originalImage);
        waitKey();
    }
}










int main()
{
    string filename;
    //filename = "../testimages/ontario_small.jpg";
    //filename = "../testimages/vilnius.jpg";
    //filename = "../testimages/lines.jpg";
    //filename = "../testimages/painting.jpg";
    //filename = "../testimages/road.jpg";
    //filename = "../testimages/floor.jpg";
    //filename = "../testimages/campaign.jpg";
    filename = "../testimages/incorrect640.jpg";
    //filename = "../testimages/points4.jpg";
    //filename = "../testimages/lettera.jpg";
    //filename = "../testimages/abv.jpg";
    Mat originalImage = imread(filename);
    Mat originalImage2 = imread(filename);

    GroundTruth(originalImage);
    MatasLike(originalImage2);

    return 0;
}
