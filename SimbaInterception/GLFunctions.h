#ifndef _GLFUNCTIONS_H
#define _GLFUNCTIONS_H
extern "C" bool getViewport( int&, int&, int&, int&);
extern "C" void getGLPosition(int,int,double&,double&,double&);
extern "C" void SetUsingResizeableClient(bool);
extern "C" void GetModelPositionByChecksum(int,int&,int&);
#endif	/* _SMART_H */