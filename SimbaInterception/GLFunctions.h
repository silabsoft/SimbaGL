#pragma once

#include "FunctionIDs.h"

extern "C" bool getViewport(int &, int &, int &, int &);
extern "C" void getGLPosition(int, int, double &, double &, double &);
extern "C" void SetUsingResizeableClient(bool);
extern "C" bool GetModelPositionByChecksum(int &x, int &y, unsigned long id);
extern "C" bool GetModelPositionByChecksum2(int &x, int &y, unsigned long id);
extern "C" bool FindInventoryFirst(int &x, int &y, unsigned long id);