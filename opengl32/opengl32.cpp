/*
* Game-Deception Blank Wrapper v2
* Copyright (c) Crusader 2002
*/

/*
* Useful ogl functions for half-life including hooked extensions
*/

#include "opengl32.h"
#include "uifunction.h"
#include <vector>
using namespace std;
#define GL_TEXTURE_RECTANGLE_ARB 0x84F5
bool drawing_model;
bool draw = false;
bool draw_overlay = false;
unsigned int draw_stride = 12;
unsigned int count = 0;
bool resizeableClient = false;
struct Model
{
	GLfloat x;				//x,y,z coords
	GLfloat y;
	GLfloat z;
	unsigned int stride;
	unsigned int x_s;		//screen coord
	unsigned int y_s;		//screen coord
	unsigned int id;		//id of model
	unsigned int count;
	unsigned int triangles;
};

vector<Model> models;

GLuint lastBuffer = 0;

//vector<unsigned int> bufferCRC;
unsigned int bufferCRC[50000];

HDC				hDC;
HFONT			hOldFont;
HFONT			hFont;
UINT			FontBase;
bool            bFontsBuild = 0;

char szSharedMemoryName[]="Local\\InterceptionMappingObject";
DWORD * pCommands;

unsigned int last_stride;

void BuildFonts()
{
	hDC			= wglGetCurrentDC();
	
	FontBase	= glGenLists(96);
	hFont		= CreateFont(-10, 0, 0, 0, FW_NORMAL,	FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_DONTCARE|DEFAULT_PITCH, "Courier"); //"Lucida Console");
	hOldFont	= (HFONT) SelectObject(hDC, hFont);
	wglUseFontBitmaps(hDC, 32, 96, FontBase);
	SelectObject(hDC, hOldFont);
	DeleteObject(hFont);

	bFontsBuild = true;
}

void CreateSharedMemory()
{
	HANDLE hMapFile;

	hMapFile = CreateFileMapping(
                 INVALID_HANDLE_VALUE,    // use paging file
                 NULL,                    // default security
                 PAGE_READWRITE,          // read/write access
                 0,                       // maximum object size (high-order DWORD)
                 255,                     // maximum object size (low-order DWORD)
                 szSharedMemoryName);     // name of mapping object

	if (hMapFile == NULL)
	{
		add_log("Could not create file mapping object (%d)",GetLastError());
		return;
	}
	pCommands = (DWORD *)MapViewOfFile(hMapFile,   // handle to map object
							FILE_MAP_ALL_ACCESS, // read/write permission
							0,
							0,
							255);
	if (pCommands == NULL)
	{
		add_log("Could not map view of file (%d).\n",GetLastError());
		CloseHandle(hMapFile);
		return;
	}
}

void ExecuteCommands()
{ //pCommands[2] is going to be a length indicator when I get to figure out how to get array's passed from pascal to C++
	if(pCommands[1] == 1)		//if command status "not done"
	{
		if(pCommands[0] == 1)	//if command = FindModelByID
		{
			for (int i = 0; i < models.size(); i++)
			{
				if (models[i].id == pCommands[3])
				{
				pCommands[3] = models[i].x_s;	//xcoord
				pCommands[4] = models[i].y_s;	//ycoord
				pCommands[1] = 2;	//set command status to response
				}
			}
		}
		if(pCommands[0] == 2){ //set overlay
			draw_overlay = (pCommands[3] == 1? true : false) ;
		}
		if(pCommands[0] == 3){ // get viewport
			int Viewport[4];
			orig_glGetIntegerv(GL_VIEWPORT, Viewport);
			pCommands[3] = Viewport[0];
			pCommands[4] = Viewport[1];
			pCommands[5] = Viewport[2];
			pCommands[6] = Viewport[3];
			pCommands[1] = 2;
		}
		if(pCommands[0] == 4){ //get GL Position
			double x, y ,z;
			MouseCoordinateToGLPos(pCommands[3], pCommands[4],x,y,z,resizeableClient);
			pCommands[3] = x;
			pCommands[4] = y;
			pCommands[5] = z;
			pCommands[1] = 2;
		}
		if(pCommands[0] == 5){ //using ResizeableClient?
			resizeableClient = (pCommands[3] == 1? true : false) ;
		}
		if(pCommands[0] == 6){ //FindModelByTriangle
			for (int i = 0; i < models.size(); i++)
			{
				if (models[i].triangles == pCommands[3])
				{
					pCommands[3] = models[i].x_s;	//xcoord
					pCommands[4] = models[i].y_s;	//ycoord
					pCommands[1] = 2;	//set command status to response
				}
			}
		}

	}
	if(pCommands[1] != 2) //we have read the command and are not required to respond
		pCommands[1] = 0;
}


void glPrint(int x, int y, const char *fmt, ...)
{
	if (!bFontsBuild) BuildFonts();

	if (fmt == NULL)	return;

	glRasterPos2i(x,y);

	char		text[256];
	va_list		ap;

	va_start(ap, fmt);
	vsprintf(text, fmt, ap);
	va_end(ap);

	glPushAttrib(GL_LIST_BIT);
	glListBase(FontBase - 32);
	glCallLists(strlen(text), GL_UNSIGNED_BYTE, text);
	glPopAttrib();
}

DWORD QuickChecksum(DWORD *pData, int size)
{
	if(!pData) { return 0x0; }

	DWORD sum;
	DWORD tmp;
	sum = *pData;

	for(int i = 1; i < (size/4); i++)
	{
		tmp = pData[i];
		tmp = (DWORD)(sum >> 29) + tmp;
		tmp = (DWORD)(sum >> 17) + tmp;
		sum = (DWORD)(sum << 3)  ^ tmp;
	}

	return sum;
}



void sys_glMultiTexCoord2fARB(GLenum target, GLfloat s, GLfloat t)
{
	orig_glMultiTexCoord2fARB(target,s,t);
}

void sys_glActiveTextureARB(GLenum target)
{
	orig_glActiveTextureARB(target);
}

void sys_glBufferDataARB(GLenum target, GLsizei size, const void* data, GLenum usage)
{
	//	if(target == 0x8892)

	//bufferCRC.reserve(lastBuffer+10);
	bufferCRC[lastBuffer] = QuickChecksum((DWORD*)data, size);
	//bufferCRC[lastBuffer] = lastBuffer;

	orig_glBufferDataARB(target, size, data, usage);
}
void sys_glVertexPointer (GLint size,  GLenum type,  GLsizei stride,  const GLvoid *pointer)
{
	last_stride = stride;
	drawing_model = true;

	if(stride == 12 && pointer != 0)		//Model model
	{

		Model newModel;
		GLfloat* temp;

		temp = (GLfloat*)pointer;
		newModel.x = *temp;
		temp = (GLfloat*)((DWORD)pointer+4);
		newModel.y = *temp;
		temp = (GLfloat*)((DWORD)pointer+8);
		newModel.z = *temp;

		newModel.stride = stride;

		models.push_back(newModel);
	}
	else
	{
		Model newModel;
		newModel.x = 0;
		newModel.y = 0;
		newModel.z = 0;
		newModel.stride = stride;
		models.push_back(newModel);
	}

	(*orig_glVertexPointer) (size, type, stride, pointer);
}

void sys_glDrawElements (GLenum mode,  GLsizei count,  GLenum type,  const GLvoid *indices)
{
	if(drawing_model){
		models.back().id = bufferCRC[lastBuffer];
		models.back().triangles = count/3;
		if(last_stride == draw_stride && draw_overlay)
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		else
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
		else{
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
	(*orig_glDrawElements) (mode, count, type, indices);
}
void sys_glDrawArrays(GLenum  mode,  GLint  first,  GLsizei  count){
		(*orig_glDrawArrays) (mode,first,count);
}
void sys_glBindBufferARB(GLenum target, GLuint id)
{
	lastBuffer = id;

	orig_glBindBufferARB(target, id);
}

void sys_glPopMatrix (void)
{
	double ModelView[16];
	double ProjView[16];
	int Viewport[4];
	double View2D[3];

	if(drawing_model)
	{
		orig_glGetDoublev(GL_MODELVIEW_MATRIX, ModelView);
		orig_glGetDoublev(GL_PROJECTION_MATRIX, ProjView) ;
		if(resizeableClient){
		orig_glGetIntegerv(GL_VIEWPORT, Viewport);
		}
		else{
			Viewport[0] = 4;
			Viewport[1] = 4;
			Viewport[2] = 509;
			Viewport[3] = 333;
		}

		if(gluProject(models.back().x, models.back().y, models.back().z, ModelView, ProjView, Viewport, &View2D[0], &View2D[1], &View2D[2]) == GL_TRUE)
		{
			models.back().x_s = (unsigned int)View2D[0];
			models.back().y_s = (unsigned int)Viewport[3]-(unsigned int)View2D[1];
		}
	}

	drawing_model = false;

	(*orig_glPopMatrix) ();
}

void sys_glOrtho (GLdouble left,  GLdouble right,  GLdouble bottom,  GLdouble top,  GLdouble zNear,  GLdouble zFar)
{

	(*orig_glOrtho) (left, right, bottom, top, zNear, zFar);
}
int dataDisplay = 0;
bool drawAll;
void sys_glEnable (GLenum cap)
{
	Model drawModel;
	if(cap == GL_TEXTURE_RECTANGLE_ARB){
		ExecuteCommands();											//check if command needs to be run every frame
		while (!models.empty())
		{
			drawModel = models.back();
			models.pop_back();	
			orig_glPushMatrix();
			orig_glLoadIdentity();	
			glEnableClientState( GL_VERTEX_ARRAY );		
			orig_glDisable(GL_TEXTURE_RECTANGLE_ARB);
			glDisable(GL_SCISSOR_TEST);
			if(draw_overlay && draw_stride == drawModel.stride || draw_overlay && drawAll){
				orig_glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
				switch(dataDisplay){
					case 0:
						glPrint(drawModel.x_s,drawModel.y_s,"C: %u",drawModel.id);
					break;
					case 1:
						glPrint(drawModel.x_s,drawModel.y_s,"T: %u",drawModel.triangles);
					break;
				}
			}
			glEnable(GL_SCISSOR_TEST);
			orig_glEnable(GL_TEXTURE_RECTANGLE_ARB);
			orig_glPopMatrix();
		}

	}
	if(GetAsyncKeyState(VK_END)&1){
		draw = !draw;
		draw_overlay = !draw_overlay;
	}
	if(GetAsyncKeyState(VK_F1)&1) 
		draw_stride = 12;
	if(GetAsyncKeyState(VK_F2)&1)
		draw_stride = 16;
	if(GetAsyncKeyState(VK_F3)&1)
		draw_stride = 24;
	if(GetAsyncKeyState(VK_F4)&1)
		draw_stride = 20;
	if(GetAsyncKeyState(VK_F5)&1) //used in higher detail modes
		draw_stride = 36;
	if(GetAsyncKeyState(VK_F6)&1) // used in higher detail modes
		draw_stride = 40;
	if(GetAsyncKeyState(VK_F7)&1) // used in higher detail modes
		draw_stride = 28;
	if(GetAsyncKeyState(VK_F8)&1) // used in higher detail modes
		drawAll = !drawAll; //overlay for all models regardless of stride.
	if(GetAsyncKeyState(VK_PRIOR)&1){
		dataDisplay++;
		dataDisplay = dataDisplay % 2; //display options for the debugging. 
	}
	(*orig_glEnable) (cap);
}

void sys_glPushMatrix (void)
{
	(*orig_glPushMatrix) ();
}

void sys_wglSwapBuffers(HDC hDC)
{
	(*orig_wglSwapBuffers) (hDC);
}

void sys_BindTextureEXT(GLenum target, GLuint texture)
{
	orig_BindTextureEXT(target,texture);
}

void sys_glAlphaFunc (GLenum func,  GLclampf ref)
{
	(*orig_glAlphaFunc) (func, ref);
}

void sys_glBegin (GLenum mode)
{
	(*orig_glBegin) (mode);
}

void sys_glBitmap (GLsizei width,  GLsizei height,  GLfloat xorig,  GLfloat yorig,  GLfloat xmove,  GLfloat ymove,  const GLubyte *bitmap)
{
	(*orig_glBitmap) (width, height, xorig, yorig, xmove, ymove, bitmap);
}

void sys_glBlendFunc (GLenum sfactor,  GLenum dfactor)
{
	(*orig_glBlendFunc) (sfactor, dfactor);
}

void sys_glClear (GLbitfield mask)
{
	(*orig_glClear)(mask);
}

void sys_glClearAccum (GLfloat red,  GLfloat green,  GLfloat blue,  GLfloat alpha)
{
	(*orig_glClearAccum) (red, green, blue, alpha);
}

void sys_glClearColor (GLclampf red,  GLclampf green,  GLclampf blue,  GLclampf alpha)
{
	(*orig_glClearColor) (red, green, blue, alpha);
}

void sys_glColor3f (GLfloat red,  GLfloat green,  GLfloat blue)
{
	(*orig_glColor3f) (red, green, blue);
}

void sys_glColor3ub (GLubyte red,  GLubyte green,  GLubyte blue)
{
	(*orig_glColor3ub) (red, green, blue);
}

void sys_glColor3ubv (const GLubyte *v)
{
	(*orig_glColor3ubv) (v);
}

void sys_glColor4f (GLfloat red,  GLfloat green,  GLfloat blue,  GLfloat alpha)
{
	(*orig_glColor4f) (red, green, blue, alpha);
}

void sys_glColor4ub (GLubyte red,  GLubyte green,  GLubyte blue,  GLubyte alpha)
{
	(*orig_glColor4ub) (red, green, blue, alpha);
}

void sys_glCullFace (GLenum mode)
{
	(*orig_glCullFace) (mode);
}

void sys_glDeleteTextures (GLsizei n,  const GLuint *textures)
{
	(*orig_glDeleteTextures) (n, textures);
}

void sys_glDepthFunc (GLenum func)
{
	(*orig_glDepthFunc) (func);
}

void sys_glDepthMask (GLboolean flag)
{
	(*orig_glDepthMask) (flag);
}

void sys_glDepthRange (GLclampd zNear,  GLclampd zFar)
{
	(*orig_glDepthRange) (zNear, zFar);
}

void sys_glDisable (GLenum cap)
{
	(*orig_glDisable) (cap);
}

void sys_glEnd (void)
{
	(*orig_glEnd) ();
}

void sys_glFrustum (GLdouble left,  GLdouble right,  GLdouble bottom,  GLdouble top,  GLdouble zNear,  GLdouble zFar)
{
	(*orig_glFrustum) (left, right, bottom, top, zNear, zFar);
}

void sys_glPopName (void)
{
	(*orig_glPopName) ();
}

void sys_glPrioritizeTextures (GLsizei n,  const GLuint *textures,  const GLclampf *priorities)
{
	(*orig_glPrioritizeTextures) (n, textures, priorities);
}

void sys_glPushAttrib (GLbitfield mask)
{
	(*orig_glPushAttrib) (mask);
}

void sys_glPushClientAttrib (GLbitfield mask)
{
	(*orig_glPushClientAttrib) (mask);
}

void sys_glRotatef (GLfloat angle,  GLfloat x,  GLfloat y,  GLfloat z)
{
	
	(*orig_glRotatef) (angle, x, y, z);
}

void sys_glShadeModel (GLenum mode)
{
	(*orig_glShadeModel) (mode);
}

void sys_glTexCoord2f (GLfloat s,  GLfloat t)
{
	(*orig_glTexCoord2f) (s, t);
}

void sys_glTexEnvf (GLenum target,  GLenum pname,  GLfloat param)
{
	(*orig_glTexEnvf) (target, pname, param);
}

void sys_glTexImage2D (GLenum target,  GLint level,  GLint internalformat,  GLsizei width,  GLsizei height,  GLint border,  GLenum format,  GLenum type,  const GLvoid *pixels)
{
	(*orig_glTexImage2D) (target, level, internalformat, width, height, border, format, type, pixels);
}

void sys_glTexParameterf (GLenum target,  GLenum pname,  GLfloat param)
{
	(*orig_glTexParameterf) (target, pname, param);
}

void sys_glTranslated (GLdouble x,  GLdouble y,  GLdouble z)
{
	(*orig_glTranslated) (x, y, z);
}

void sys_glTranslatef (GLfloat x,  GLfloat y,  GLfloat z)
{
	(*orig_glTranslatef) (x, y, z);
}

void sys_glVertex2f (GLfloat x,  GLfloat y)
{
	(*orig_glVertex2f) (x, y);
}

void sys_glVertex3f (GLfloat x,  GLfloat y,  GLfloat z)
{
	(*orig_glVertex3f) (x, y, z);
}

void sys_glVertex3fv (const GLfloat *v)
{
	(*orig_glVertex3fv) (v);
}

void sys_glViewport (GLint x,  GLint y,  GLsizei width,  GLsizei height)
{
	(*orig_glViewport) (x, y, width, height);
}

PROC sys_wglGetProcAddress(LPCSTR ProcName)
{
	if (!strcmp(ProcName,"glMultiTexCoord2fARB"))
	{
		orig_glMultiTexCoord2fARB=(func_glMultiTexCoord2fARB_t)orig_wglGetProcAddress(ProcName);
		return (FARPROC)sys_glMultiTexCoord2fARB;
	}
	else if (!strcmp(ProcName,"glBindTextureEXT"))
	{
		orig_BindTextureEXT=(func_BindTextureEXT_t)orig_wglGetProcAddress(ProcName);
		return (FARPROC)sys_BindTextureEXT;
	}
	else if(!strcmp(ProcName,"glActiveTextureARB"))
	{
		orig_glActiveTextureARB=(func_glActiveTextureARB_t)orig_wglGetProcAddress(ProcName);
		return (FARPROC)sys_glActiveTextureARB;
	}
	else if(!strcmp(ProcName,"glBufferDataARB"))
	{
		orig_glBufferDataARB=(func_glBufferDataARB_t)orig_wglGetProcAddress(ProcName);
		return (FARPROC)sys_glBufferDataARB;
	}
	else if(!strcmp(ProcName,"glBindBufferARB"))
	{
		orig_glBindBufferARB=(func_glBindBufferARB_t)orig_wglGetProcAddress(ProcName);
		return (FARPROC)sys_glBindBufferARB;
	}

	return orig_wglGetProcAddress(ProcName);
}

#pragma warning(disable:4100)
BOOL __stdcall DllMain (HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved)
{
	switch(fdwReason)
	{
		case DLL_PROCESS_ATTACH:
			DisableThreadLibraryCalls (hOriginalDll);
			CreateSharedMemory();
			return Init();

		case DLL_PROCESS_DETACH:
			if ( hOriginalDll != NULL )
			{
				FreeLibrary(hOriginalDll);
				hOriginalDll = NULL;
			}
			break;
	}
	return TRUE;
}
#pragma warning(default:4100)


void __cdecl add_log (const char * fmt, ...)
{
	va_list va_alist;
	char logbuf[256] = "";
    FILE * fp;

	va_start (va_alist, fmt);
	_vsnprintf (logbuf+strlen(logbuf), sizeof(logbuf) - strlen(logbuf), fmt, va_alist);
	va_end (va_alist);

	if ( (fp = fopen ("c:\\rs\\opengl32.log", "ab")) != NULL )
	{
		fprintf ( fp, "%s\n", logbuf );
		fclose (fp);
	}
}