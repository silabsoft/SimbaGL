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
	arr[0] = CMD_GET_VIEWPORT;
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
	arr[0] = CMD_GET_GL_POSITION;
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
	arr[0] = CMD_SET_RESIZEABLE;
	arr[3] = b ? 1 : 0;
	arr[1] = 1;
}

bool GetModelPositionByChecksum(int &x, int &y, unsigned long id)
{
	DWORD * arr = requestSharedMemory();
	arr[0] = CMD_FIND_MODEL_BY_CHECKSUM;
	arr[3] = id;
	arr[1] = 1;

	if(isGoingToCallBack()){
		x = arr[3];
		y = arr[4];
		return true;
	}

	return false;
}

bool GetModelPositionByChecksum2(int &x, int &y, unsigned long id2)
{
	DWORD * arr = requestSharedMemory();
	arr[0] = CMD_FIND_MODEL_BY_ID2;
	arr[3] = id2;
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
	arr[0] = CMD_FIND_MODEL_BY_TRIANGLE_COUNT;
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

bool FindInventoryFirst(int &x, int &y, unsigned long id)
{
	DWORD *arr = requestSharedMemory();
	arr[0] = CMD_FIND_INVENTORY_FIRST;
	arr[3] = id;
	arr[1] = 1;

	if(isGoingToCallBack()){
		x = arr[3];
		y = arr[4];
		return true;
	}

	return false;
}