#include "uifunction.h"
#include "opengl32.h"
#include <gl/glu.h>

void MouseCoordinateToGLPos(int mouseX, int mouseY,double& glpX, double& glpY, double& glpZ,bool resizeable)
{
    GLint viewport[4];
    GLdouble modelview[16];
    GLdouble projection[16];
    GLfloat x, y, z;
    GLdouble posX, posY, posZ;
 
    orig_glGetDoublev( GL_MODELVIEW_MATRIX, modelview );
    orig_glGetDoublev( GL_PROJECTION_MATRIX, projection );
	if(resizeable){
		orig_glGetIntegerv( GL_VIEWPORT, viewport );
	}
		else{
			viewport[0] = 4;
			viewport[1] = 4;
			viewport[2] = 509;
			viewport[3] = 333;
		}
    x = (float)mouseX;
    y = (float)viewport[3] - (float)mouseY;
    orig_glReadPixels( x, int(y), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &z );
 
    gluUnProject( x, y, z, modelview, projection, viewport, &posX, &posY, &posZ);
	glpX = (double)posX;
	glpY = (double)posY;
	glpZ = (double)posZ;
}