#include "GLFunctions.h"
#include "Main.h"
#include <cstring>
#include <cstdlib>
#include <iostream>
using namespace std;

signed int * glGetIntegerv(signed int * arr){

	cout << "I'm A MORON AND CAN FIGURE OUT HOW TO MAKE ARRAYS WORK! \n";
	return 0;
}

bool getViewport(int& x,int& y, int& width, int& height){
	DWORD * arr = requestSharedMemory();
	arr[0] = 3;
	arr[1] = 1;

	if(isGoingToCallBack()){
		x = arr[3];
		y = arr[4];
		height= arr[5];
		width = arr[6];
		arr[1] = 0;
		return true;
	}
	return false;
}

void getGLPosition(int x, int y, double& posX,double& posY, double& posZ){
	DWORD * arr = requestSharedMemory();
	arr[0] = 4;
	arr[3] = x;
	arr[4] = y;
	arr[1] = 1;
	if(isGoingToCallBack()){
		posX = arr[3];
		posY = arr[4];
		posZ = arr[5];
	}
	else{
		posX = -1;
		posY = -1;
		posZ = -1;
	}
}
void SetUsingResizeableClient(bool b){
	DWORD * arr = requestSharedMemory();
	arr[0] = 5;
	arr[3] = b ? 1 : 0;
	arr[1] = 1;
}

bool GetModelPositionByChecksum(int &x, int &y, unsigned long id)
{
	DWORD * arr = requestSharedMemory();
	arr[0] = 1;
	arr[3] = id;
	arr[1] = 1;

	if(isGoingToCallBack()){
		x = arr[3];
		y = arr[4];
		return true;
	}

	return false;
}
void GetModelPositionByTriangleCount(int id, int& x,int& y){
	DWORD * arr = requestSharedMemory();
	arr[0] = 6;
	arr[3] = id;
	arr[1] = 1;
	if(isGoingToCallBack()){
		x = arr[3];
		y = arr[4];
	}
	else{
		x = -1;
		y = -1;
	}
}